# GPU driver for a simple emulated GPU

This is a Linux driver for an emulated GPU. The GPU is very simple, but the driver pipeline was as accurate as possible to a real GPU.
This was written on a Rasberry pi Debian environment on linux 6.6.74.

The actual displaying is done by the real GPU or whatever has capabilities to display to the screen directly. This GPU and driver will use the low level API of SDL2 to display a framebuffer object.

## Features

- Atomic driver
- Double buffering
- SDL2-based rendering  
- CRTC, planes, GEM backed buffer objects

## Build Requirements

- **gcc** 
- **SDL2**
- **Make**
