# Simple GPU Driver

A minimal GPU driver and software rasterizer. Built to interface with a basic emulated platform device on the Raspberry Pi 5, this project simulates core GPU pipeline behavior with a focus on clarity over complexity.

## Features

- Linear framebuffer support
- Pixel format support: `RGB565`, `RGB888`, `RGB8888`
- Simulated memory-mapped I/O support on Raspberry Pi 5
- Basic software rasterization pipeline

## Build & Run
Instructions vary depending on platform, so these are specific for Rasberry Pi 5:
* Compile test.dts with dtc: `dtc -I dts -O dtb -@ -o some_name.dtbo test.dts`
* Place it in `/boot/firmware/overlays/`
* In `/boot/firmware/overlays/config.txt`, add `dtoverlay=some_name`
* Compile the driver files using `make`
* Load the driver using (sudo) insmod: `sudo insmod driver.ko`
