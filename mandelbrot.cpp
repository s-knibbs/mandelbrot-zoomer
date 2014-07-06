#include <iostream>
#include <SDL/SDL.h>
#include <stdlib.h>
#include <exception>
#include <GL/glew.h>
#include <string>
#include "textfile.h"

using namespace std;

class InitException
{
  public:
    InitException(char * m);    
    const char * what();
  private:
    string msg;
};

InitException::InitException(char * m):
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
    file = fopen("ProgramLog.txt", "w");
    infoLog = (char *)malloc(infologLength);
    glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
    fprintf(file, "%s\n",infoLog);
    free(infoLog);
    fclose(file);
  }
}

void printShaderInfoLog(GLuint obj)
{
  int infologLength = 0;
  int charsWritten  = 0;
  char *infoLog;
    FILE * file;

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

  if (infologLength > 0)
  {
      file = fopen("ShaderLog.txt", "w");
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
  SDL_Event event;
  char * shader_path;
  GLuint frag, prog, bl, tr, res;
  Uint32 flags;
  double bottomleft[2];
  double topright[2];
  float zoom_factor;
  static const float FPS_TICKS_60 = 17.0;
  Uint32 _previous_ticks;
  
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

  void initVideo()
  {
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
    if (SDL_SetVideoMode(this->w, this->h, 32, this->flags) < 0)
    {
      throw InitException("Could not initialise video.");
    }
    // If shader does not compile, display a green screen.
    glClearColor(0.0, 1.0, 0.0, 0.0);
    glewInit();
    if (glewIsSupported("GL_VERSION_2_0"))
    {
      printf("Ready for OpenGL 2.0\n");
    }
    else
    {
      throw InitException("OpenGL 2.0 not supported.");
    }
  }

  void initShader()
  {
    char * fs = NULL;
    frag = glCreateShader(GL_FRAGMENT_SHADER);
    fs = textFileRead(this->shader_path);
    if (fs == NULL)
    {
      throw InitException("Could not read shader file.");
    }
    const char * ff = fs;
    glShaderSource(frag, 1, &ff, NULL);
    glCompileShader(frag);
    free(fs);
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
    bottomleft[0] = -2.0;
    bottomleft[1] = -1.0;
    topright[0] = 1.0;
    topright[1] = 1.0;
    zoom_factor = 1.0;
    _previous_ticks = SDL_GetTicks();
  }

  void setUniformValues()
  {
    glUniform2dv(bl, 1, bottomleft);
    glUniform2dv(tr, 1, topright);
    glUniform2f(res, (float)w, (float)h);
  }

  void zoom(double * axis, float speed)
  {
    Uint32 ticks = SDL_GetTicks();
    speed *= (ticks - _previous_ticks) / FPS_TICKS_60;
    if (speed < 1.0)
    {
      speed = 1.0;
    }
    if (speed > 40.0)
    {
      speed = 40.0;
    }
    float normalised_speed = 10000 / (10000 + speed);
    for (int i = 0; i < 2; i++)
    {
      bottomleft[i] = axis[i] - (axis[i] - bottomleft[i]) * normalised_speed;
      topright[i] = axis[i] - (axis[i] - topright[i]) * normalised_speed;
    }
    zoom_factor /= normalised_speed;
    _previous_ticks = ticks;
  }

  public: 
    Program(char * shaderPath):
    shader_path(shaderPath)
    {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
      {
        throw InitException("Could not initialise SDL.");
      }
      const SDL_VideoInfo * mode;
      mode = SDL_GetVideoInfo();
      w = mode->current_w;
      h = mode->current_h;
      flags = SDL_OPENGL | SDL_FULLSCREEN;
      initVideo();
      initShader();
    }

    Program(char * shaderPath, int width, int height):
    shader_path(shaderPath), w(width), h(height)
    {
      if (SDL_Init(SDL_INIT_VIDEO) < 0)
      {
        throw InitException("Could not initialise SDL.");
      }
      flags = SDL_OPENGL;
      initVideo();
      initShader();
      setUniformValues();
    }

    ~Program()
    {
      SDL_Quit();
    }

    void eventLoop()
    {
      double axis[] = {-1.193, 0.33};
      float speed = 10;
      while (1)
      {
        zoom(axis, speed);
        setUniformValues();
        render();
        while(SDL_PollEvent(&event))
        {
          switch (event.type)
          {
            case SDL_KEYDOWN:
              switch (event.key.keysym.sym)
              {
                case SDLK_ESCAPE:
                  return;
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
                case SDLK_SPACE:
                  bottomleft[0] = -2.0;
                  bottomleft[1] = -1.0;
                  topright[0] = 1.0;
                  topright[1] = 1.0;
                  zoom_factor = 1.0;
                  break;
                default:
                  break;
              }
              break;
            default:
              break;  
          }
        }
      }
    }
};


int main(int argc, char* argv[])
{
  try
  {
    Program prog("mandelbrot-single.frag", 800, 600);
    prog.eventLoop();
  }
  catch (InitException& e)
  {
    cerr << e.what() << endl;
    exit(1);
  }
  return 0;
}

