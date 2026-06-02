# dvc

Concrete device abstractions live here.

Use this layer for devices such as LEDs, serial consoles, motors, sensors, encoders, IMUs, power monitors, and other components built from firmware-layer capabilities.

## ST7735 80x160 LCD

Current `ST7735AnimationExample` pin mapping:

| LCD pin | ESP32-C3 GPIO | Notes |
| --- | ---: | --- |
| GND | GND | Ground |
| VCC | 3V3 | Power |
| SCL | GPIO2 | SPI SCK |
| SDA | GPIO3 | SPI MOSI |
| RES | GPIO1 | Reset |
| DC | GPIO0 | Data/command |
| CS | GPIO7 | Chip select |
| BL | GPIO10 | Backlight |

The ST7735 device driver owns the display details: it allocates an internal DMA-capable transfer buffer by default, supports RGB565 frame/animation pushing, and provides 5x7 ASCII text drawing with scale, color, background, transparency, newline, and optional wrapping.
