#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>

#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL/SDL.h>
#endif

#include <stdlib.h>
#include <exception>
#include <GL/glew.h>
#include <string>

#define PROGRAM_LOG_NAME "ProgramLog.txt"
#define SHADER_LOG_NAME "ShaderLog.txt"
static const float FPS_TICKS_60 = 17.0;

using namespace std;

typedef map<char, string> OptMap;

class InitException
{
  public:
    InitException(string m);
    const char * what();
  private:
    string msg;
};

// Helper class. Locks an SDL mutex, and unlocks automatically on scope exit
class SDLMutexLocker
{
  SDL_mutex* _mutex;

public:
  SDLMutexLocker(SDL_mutex* mutex) :
    _mutex(mutex)
  {
    SDL_LockMutex(_mutex);
  }

  ~SDLMutexLocker()
  {
    SDL_UnlockMutex(_mutex);
  }
};

InitException::InitException(string m):
  msg(m)
{}

const char * InitException::what()
{
  return msg.c_str();
}

void printProgramInfoLog(GLuint obj)
{
  int infologLength = 0;
  int charsWritten  = 0;
  char *infoLog;
  FILE * file;

  glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

  if (infologLength > 0)
  {
#ifdef _WIN32
    fopen_s(&file, PROGRAM_LOG_NAME, "w");
#else
    file = fopen(PROGRAM_LOG_NAME, "w");
#endif
    infoLog = (char *)malloc(infologLength);
    glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
    fprintf(file, "%s\n",infoLog);
    free(infoLog);
    fclose(file);
  }
}

int file_open(FILE** file, const char* name, const char* mode)
{
#ifdef _WIN32
  return fopen_s(file, name, mode);
#else
  *file = fopen(name, mode);
  return errno
#endif
}

void printShaderInfoLog(GLuint obj)
{
  int infologLength = 0;
  int charsWritten  = 0;
  char *infoLog;
  FILE * file;

  glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);

  if (infologLength > 0)
  {
    file_open(&file, SHADER_LOG_NAME, "w");
    infoLog = (char *)malloc(infologLength);
    glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
    fprintf(file, "%s\n", infoLog);
    free(infoLog);
    fclose(file);
  }
}

class Program
{
  int w, h;
  int render_width;
  SDL_Event event;
  char * shader_path;
  GLuint frag, prog, bl, tr, res;
  Uint32 flags;
  double bottomleft[2];
  double topright[2];
  double zoom_factor;
  Uint32 _previous_ticks;
  unsigned _screen_shot_idx;
  SDL_Surface * _frame_buffer;
  FILE* _out_stream;
  SDL_mutex * _cond_lock;
  SDL_mutex * _frame_buffer_lock;
  SDL_cond * _write_cond;
  bool _frame_write_enable;
  bool _write_thread_quit;

  void render()
  {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    // Create a flat plane to draw on.
    glBegin(GL_QUADS);
    // Bottom left.
    glVertex2f(-1.0, -1.0);
    // Top left
    glVertex2f(-1.0, 1.0);
    // Top right
    glVertex2f(1.0, 1.0);
    // Bottom right.
    glVertex2f(1.0, -1.0);
    glEnd();
    
    SDL_GL_SwapBuffers();
    //Block until the frame has finished rendering.
    glFinish();
  }

  int readBuffer()
  {
    int ret = 1;
    {
      SDLMutexLocker lock(_frame_buffer_lock);
      glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_BGR,
                   GL_UNSIGNED_BYTE, _frame_buffer->pixels);
    }

    if (glGetError() != GL_NO_ERROR)
    {
      cerr << "Failed to retrieve the frame-buffer." << endl;
      ret = 0;
    }
    return ret;
  }

  void saveScreenshot()
  {
    if (readBuffer())
    {
      stringstream name;
      name.width(2);
      name.fill('0');
      name << "screenshot" << _screen_shot_idx << ".bmp";
      SDL_SaveBMP(_frame_buffer, name.str().c_str());
      _screen_shot_idx++;
    }
  }

  void writeFrame()
  {
    if (readBuffer())
    {
      SDLMutexLocker lock(_cond_lock);
      _frame_write_enable = true;
      SDL_CondSignal(_write_cond);
    }
  }

  // Thread for writing frames to avoid IO waiting
  static int writeThread(void * prog)
  {
    Program* self = static_cast<Program *>(prog);
    while (1)
    {
      {
        SDLMutexLocker lock(self->_cond_lock);
        while (!self->_frame_write_enable && !self->_write_thread_quit)
        {
          SDL_CondWait(self->_write_cond, self->_cond_lock);
        }
        self->_frame_write_enable = false;
      }
      if (self->_write_thread_quit)
        return 0;
      {
        SDLMutexLocker lock(self->_frame_buffer_lock);
        fwrite(self->_frame_buffer->pixels, 1,
               self->_frame_buffer->pitch * self->_frame_buffer->h,
               self->_out_stream);
      }
    }
  }

  void initVideo()
  {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    SDL_WM_SetCaption("Mandelbrot Zoomer", NULL);
    if (SDL_SetVideoMode(this->w, this->h, 32, this->flags) < 0)
    {
      throw InitException(string("Could not initialise video: ") + SDL_GetError());
    }

    // If shader does not compile, display a green screen.
    glClearColor(0.0, 1.0, 0.0, 0.0);
    glewInit();
    if (!glewIsSupported("GL_VERSION_2_0"))
    {
      throw InitException("OpenGL 2.0 not supported.");
    }
  }

  void initCoordinates()
  {
    bottomleft[0] = -2.0;
    bottomleft[1] = -1.0;
    topright[0] = 1.0;
    topright[1] = 1.0;
    zoom_factor = 1.0;
  }

  void initShader()
  {
    ifstream shader_file;
    shader_file.open(shader_path, istream::in);
    if (!shader_file.good())
    {
      throw InitException("Could not read shader file.");
    }
    shader_file.seekg(0, shader_file.end);
    int length = (int)shader_file.tellg();
    shader_file.seekg(0, shader_file.beg);
    char *shader = new char[length + 1];
    memset(shader, 0, length + 1);
    shader_file.read(shader, length);
    frag = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(frag, 1, (const char **)&shader, NULL);
    glCompileShader(frag);
    delete[] shader;
    shader_file.close();
    printShaderInfoLog(frag);
    prog = glCreateProgram();
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    printProgramInfoLog(prog);
    glUseProgram(prog);
    if (glGetError() != 0)
    {
      throw InitException("Shader did not compile, check logs.");
    }
    bl = glGetUniformLocation(prog, "bottomleft");
    tr = glGetUniformLocation(prog, "topright");
    res = glGetUniformLocation(prog, "resolution");
    if ((bl == -1)||(tr == -1)||(res == -1))
    {
      throw InitException("Could not get uniform location.");
    }
    initCoordinates();
    _previous_ticks = SDL_GetTicks();
  }

  void setUniformValues()
  {
    glUniform2dv(bl, 1, bottomleft);
    glUniform2dv(tr, 1, topright);
    glUniform2f(res, (float)render_width, (float)h);
  }

  int zoom(double * axis, double speed, bool save_stream, double min_width)
  {
    Uint32 ticks = SDL_GetTicks();
    int ret = 0;
    // Clamp speed
    if (speed < 1.0)
      speed = 1.0;
    if (speed > 60.0)
      speed = 60.0;
    // Don't adjust speed according to frame-rate when saving to file.
    if (!save_stream)
      speed *= (ticks - _previous_ticks) / FPS_TICKS_60;
    double normalised_speed = 10000 / (10000 + speed);
    for (int i = 0; i < 2; i++)
    {
      bottomleft[i] = axis[i] - (axis[i] - bottomleft[i]) * normalised_speed;
      topright[i] = axis[i] - (axis[i] - topright[i]) * normalised_speed;
    }
    zoom_factor /= normalised_speed;

    // Reset zoom once we reach maximum zoom
    if (!isnan(min_width) && (topright[0] - bottomleft[0]) <= min_width)
    {
      if (save_stream)
        ret = 1;
      else
        initCoordinates();
    }

    _previous_ticks = ticks;
    return ret;
  }

  void init(int sdl_flags=0)
  {
    if (w == 0 || h == 0)
    {
      const SDL_VideoInfo * mode;
      mode = SDL_GetVideoInfo();
      w = mode->current_w;
      h = mode->current_h;
    }
    // Normalise the width passed to the shader to 4:3 to avoid stretching the image.
    render_width = (h * 4) / 3;
    flags = SDL_OPENGL | sdl_flags;
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      throw InitException("Could not initialise SDL.");
    }
    initVideo();
    initShader();
    _frame_buffer = SDL_CreateRGBSurface(0, w, h, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0);
    _frame_buffer_lock = SDL_CreateMutex();
  }

  public: 
    Program(char * shaderPath, int width, int height, bool fullscreen):
      shader_path(shaderPath), w(width), h(height),
      _screen_shot_idx(1),
      _frame_buffer(NULL),
      _out_stream(NULL),
      _frame_write_enable(false),
      _write_thread_quit(false),
      _cond_lock(NULL),
      _frame_buffer_lock(NULL),
      _write_cond(NULL)
    {
      int flags = 0;
      if (fullscreen) flags |= SDL_FULLSCREEN;
      init(flags);
    }

    ~Program()
    {
      if (_frame_buffer) SDL_FreeSurface(_frame_buffer);
      if (_out_stream) fclose(_out_stream);
      if (_frame_buffer_lock) SDL_DestroyMutex(_frame_buffer_lock);
      if (_cond_lock) SDL_DestroyMutex(_cond_lock);
      if (_write_cond) SDL_DestroyCond(_write_cond);
      SDL_Quit();
    }

    void eventLoop(bool save_stream, double axis[], double speed=10, double min_width=NAN)
    {
      int frame_count = 0;
      SDL_Thread* thread = NULL;

      if (save_stream)
      {
        if (file_open(&_out_stream, "out.raw", "wb") != 0)
        {
#ifdef _WIN32
          char buf[80];
          strerror_s(buf, 80, errno);
#else
          const char buf = strerror(errno);
#endif
          fprintf(stderr, "Could not open output stream: %s.\n", buf);
          save_stream = false;
        }
        else
        {
          _write_cond = SDL_CreateCond();
          _cond_lock = SDL_CreateMutex();
          thread = SDL_CreateThread(writeThread, this);
          if (NULL == thread)
          {
            fprintf(stderr, "SDL_CreateThread failed: %s\n", SDL_GetError());
          }
        }
      }

      while (1)
      {
        if (zoom(axis, speed, save_stream, min_width))
          goto exit;
        setUniformValues();
        render();
        frame_count++;
        if (save_stream) writeFrame();
        while(SDL_PollEvent(&event))
        {
          switch (event.type)
          {
            case SDL_KEYDOWN:
              switch (event.key.keysym.sym)
              {
                case SDLK_ESCAPE:
                  goto exit;
                case SDLK_UP:
                  axis[1] += 0.06/zoom_factor;
                  break;
                case SDLK_DOWN:
                  axis[1] -= 0.06/zoom_factor;
                  break;
                case SDLK_RIGHT:
                  axis[0] += 0.06/zoom_factor;
                  break;
                case SDLK_LEFT:
                  axis[0] -= 0.06/zoom_factor;
                  break;
                case SDLK_w:
                  speed += 1;
                  break;
                case SDLK_s:
                  speed -= 1;
                  break;
                case SDLK_q:
                  if (!save_stream) saveScreenshot();
                  break;
                case SDLK_SPACE: 
                  initCoordinates();
                  break;
                default:
                  break;
              }
              break;
            case SDL_QUIT:
              goto exit;
            default:
              break;  
          }
        }
      }
    exit:
      // Thread shutdown
      if (thread)
      {
        {
          SDLMutexLocker lock(_cond_lock);
          _write_thread_quit = true;
          SDL_CondSignal(_write_cond);
        }
        SDL_WaitThread(thread, NULL);
      }
      printf("Rendered %d frames.\n", frame_count);
      printf("Zoom axis: (%.10f, %.10f)\n", axis[0], axis[1]);
      printf("Speed: %f\n", speed);
      return;
    }
};

OptMap get_options(int argc, char* argv[], const char* opt_spec)
{
  char opt_char;
  bool has_arg = false;
  OptMap options;
  for (int i = 0; i < argc; i++)
  {
    if (argv[i][0] == '-' && strlen(argv[i]) == 2 && !isdigit(argv[i][1]))
    {
      if (has_arg) throw InitException(string("Missing argument for option -") + opt_char);
      opt_char = argv[i][1];
      int j = 0;
      while (opt_spec[j] != opt_char && opt_spec[j] != 0)
      {
        j++;
      }
      if (opt_spec[j] == opt_char)
      {
        has_arg = (opt_spec[j + 1] == ':');
        if (!has_arg) options[opt_char] = "1";
      }
      else
      {
        throw InitException(string("Invalid option: -") + opt_char);
      }
    }
    else if (has_arg)
    {
      options[opt_char] = argv[i];
      has_arg = false;
    }
  }
  if (has_arg) throw InitException(string("Missing argument for option -") + opt_char);
  return options;
}

int main(int argc, char* argv[])
{
  int returncode = 0;
  unsigned long w, h;
  // Zoom in on a good point by default.
  double axis[] = {-1.1623416001, 0.2923689343};
  double speed = 10.0;
  const int Y_MAX = 1;
  const int Y_MIN = -1;
  const int X_MAX = 1;
  const int X_MIN = -2;
  w = 960;
  h = 720;

  try
  {
    OptMap options = get_options(argc, argv, "w:H:x:y:s:hrfd");
    if (options.count('h'))
    {
      cout << "Usage: " << argv[0] << " [-w WIDTH][-H HEIGHT][options]" << endl;
      cout << endl;
      cout << "Options:" << endl;
      cout << "  -w : Screen or window width in pixels. Default: " << w << endl;
      cout << "  -H : Screen or window height in pixels. Default: " << h << endl;
      cout << setprecision(10);
      cout << "  -x : X coordinate to zoom in at. Default: " << axis[0] << endl;
      cout << "  -y : Y coordinate to zoom in at. Default: " << axis[1] << endl;
      cout << "  -s : Initial zoom speed. Default: " << speed << endl;
      cout << "  -r : Save frame-buffer to raw video stream. SSD recommended." << endl;
      cout << "  -f : Open in fullscreen mode. Escape to exit." << endl;
      cout << "  -d : Use double-precision floating point. Slower, but can zoom further." << endl;
      cout << "  -h : Print this help message and quit." << endl;
      cout << endl;
      cout << "Controls:" << endl;
      cout << "  Direction Keys : Move zoom axis." << endl;
      cout << "  w : Increase speed." << endl;
      cout << "  s : Decrease speed." << endl;
      cout << "  q : Take screenshot, ignored if launched with '-r'." << endl;
      cout << "  SPACEBAR : Reset zoom." << endl;
      cout << "  ESC : Exit." << endl;
      return returncode;
    }
    if (options.count('w'))
    {
      w = strtoul(options['w'].c_str(), NULL, 10);
    }
    if (options.count('H'))
    {
      h = strtoul(options['H'].c_str(), NULL, 10);
    }
    if (options.count('x'))
    {
      double x = strtod(options['x'].c_str(), NULL);
      if (x > X_MIN && x < X_MAX)
        axis[0] = x;
      else
        fprintf(stderr, "X coordinate must be between %d and %d.\n", X_MIN, X_MAX);
    }
    if (options.count('y'))
    {
      double y = strtod(options['y'].c_str(), NULL);
      if (y > Y_MIN && y < Y_MAX)
        axis[1] = y;
      else
        fprintf(stderr, "Y coordinate must be between %d and %d.\n", Y_MIN, Y_MAX);
    }
    if (options.count('s'))
    {
      double s = strtod(options['s'].c_str(), NULL);
      if (s >= 0)
        speed = s;
      else
        fprintf(stderr, "Speed value must be positive.\n");
    }
    char * shader_file = (options.count('d') > 0) ? "mandelbrot-double.frag" : "mandelbrot-single.frag";
    // Width to limit zooming to.
    double min_width = (options.count('d') > 0) ? 20 * DBL_EPSILON : 20 * FLT_EPSILON;
    Program prog(shader_file, w, h, (options.count('f') > 0));
    prog.eventLoop((options.count('r') > 0), axis, speed, min_width);
  }
  catch (InitException& e)
  {
    cerr << e.what() << endl;
    returncode = 1;
  }
  return returncode;
}
