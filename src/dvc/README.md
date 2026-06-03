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

Orientation presets are configured in the device layer:

| Orientation | Logical size | MADCTL | Offset |
| --- | ---: | ---: | --- |
| `Portrait` | 80x160 | `0xC8` | `x=26, y=1` |
| `Landscape` | 160x80 | `0xA8` | `x=1, y=26` |
| `PortraitInverted` | 80x160 | `0x08` | `x=26, y=1` |
| `LandscapeInverted` | 160x80 | `0x68` | `x=1, y=26` |

Use `ST7735::makeConfig(ST7735::Orientation::Landscape)` for the 160x80 logical layout. GIF assets must be regenerated at the same logical size as the selected orientation.

## WS2812 LED

`WS2812` owns the LED protocol details and uses the firmware-layer `RMT` transmitter internally.

Default protocol settings:

| Item | Value |
| --- | ---: |
| RMT resolution | 10 MHz |
| Bit rate | 800 kHz |
| Color order | GRB |
| `0` bit | 0.4 us high, 0.85 us low |
| `1` bit | 0.8 us high, 0.45 us low |
| Reset/latch | 80 us low |

Current `WS2812Example` default pin mapping:

| WS2812 pin | ESP32-C3 GPIO | Notes |
| --- | ---: | --- |
| GND | GND | Must share ground with ESP32 |
| VCC | 5V or 3V3 | Depends on LED module and brightness |
| DIN | GPIO8 | RMT TX data |

For 5V WS2812 strips, a level shifter is recommended for reliable DIN signaling. Add a 330-470 ohm series resistor on DIN and a bulk capacitor near LED power for longer strips.
