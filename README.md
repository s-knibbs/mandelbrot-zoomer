Mandelbrot Zoomer
=================

A [mandelbrot](http://en.wikipedia.org/wiki/Mandelbrot_set) zoomer using GLSL (OpenGL shader language).

Zooms into the mandelbrot set in realtime. The zoom location can be controlled using the UP / DOWN / LEFT / RIGHT keys.
The speed can be increased / decreased using 'W' and 'W'. SPACEBAR to reset, ESCAPE to quit.

Requires a reasonably capable graphics card for realtime zooming. Tested on my machine using an AMD Radeon HD 6850 in
an 800 x 600 window, I seem to get 60FPS.

The zoom factor is somewhat limited due to single precision floating point. This can be increased by modifying the
fragment shader to use double precision floating point, however this dramatically reduces the framerate (~10FPS).

Required to build:
 * libsdl-dev
 * libglew-dev
