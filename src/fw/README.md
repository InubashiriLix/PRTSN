# firmware

Low-level hardware and protocol adapters live here.

Use this layer for GPIO, PWM, I2C, CAN, I2S, UART, timers, and other board/peripheral capabilities. Firmware code should not know about node business logic, controllers, MQTT topics, or concrete application devices.

## RMT TX

`RMT` is a generic pulse-symbol transmitter based on the ESP-IDF RMT TX driver. It does not know about WS2812, IR, servos, or any concrete device protocol.

Typical use:

1. Configure GPIO and resolution, usually `10 MHz` for `0.1 us` ticks.
2. Convert protocol data into `RMT::Symbol` items.
3. Call `transmit()`.
4. Call `waitDone()`.

Device-layer code owns protocol encoding. For example, WS2812 maps each data bit to one high/low RMT symbol and appends a low reset symbol.
