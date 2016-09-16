# Mandelbrot Zoomer

A [mandelbrot](http://en.wikipedia.org/wiki/Mandelbrot_set) zoomer using GLSL (OpenGL shader language).

Requires a reasonably modern graphics card. In single precision floating point mode
any recent mid-range graphics card should be able to render at 60FPS.

## USAGE

### Command Line options

```
Usage: mandelbrot-zoomer.exe [-w WIDTH][-H HEIGHT][options]

Options:
  -w : Screen or window width in pixels. Default: 960
  -H : Screen or window height in pixels. Default: 720
  -x : X coordinate to zoom in at. Default: -1.1623416
  -y : Y coordinate to zoom in at. Default: 0.2923689343
  -s : Initial zoom speed. Default: 10
  -r : Save frame-buffer to raw video stream. SSD recommended.
  -f : Open in fullscreen mode. Escape to exit.
  -d : Use double-precision floating point. Slower, but can zoom further.
  -h : Print this help message and quit.
```

### Controls

**Direction Keys** - Move zoom axis. <br />
**w** - Increase speed. <br />
**s** - Decrease speed. <br />
**q** - Take screenshot, ignored if launched with '-r'. <br />
**SPACEBAR** - Reset zoom. <br />
**ESC** - Exit.

## BUILDING

### Linux

For linux the following dependencies are required:
 * libsdl-dev
 * libglew-dev

Build by running `build.sh`.

### Windows

Visual Studio 2015 edition is required to build with the included solution file.

[SDL](https://www.libsdl.org/download-1.2.php) and [Glew](http://glew.sourceforge.net/index.html) should be installed to `C:\SDL\SDL-1.2.15` and `C:\glew\glew-2.0.0` respectively for the configured paths to work correctly.

**Note:** The pre-built SDL binaries on their site were built using a previous version of Visual Studio and are not compatible with Visual Studio 2015, so you will need to build this from source.