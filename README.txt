dae2taascene
============

author:  Thomas Atwood (tatwood.net)
date:    2011
license: see UNLICENSE.txt

This is a utility to convert taascene files into wavefront obj files.

Usage
=====
    Required arguments
      -anim               Export meshes with animation deformation
      -o [FILE]           Path to output obj file
      -scene [FILE]       Path to input taascene file
      -static             Export meshes without animation deformation
      -time [FLOAT]       If -anim, time in seconds to export the frame

    Options
      -up [Y|Z]           Up axis for exported obj file

Building
========

## Linux ###
The the following dependencies are required to build on Linux:
    ../libdae
    ../taamath
    ../taascene
    ../taasdk
    -lm

### Windows ###
The the following dependencies are required to build on Microsoft Windows:
    ../libdae
    ../taamath
    ../taascene
    ../taasdk
