#!/bin/sh
g++ mandelbrot.cpp textfile.cpp -l SDL -l GLEW -l GL -o mandelbrot -Wno-write-strings
