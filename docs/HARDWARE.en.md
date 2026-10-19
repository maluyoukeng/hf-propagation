# hf-propagation · Hardware

[简体中文](HARDWARE.md) · [Back to README](../README.en.md)

This application targets the **FoloToy AI Passport**. Hardware facts are defined
in `components/bsp/include/bsp_pins.h`; this document is a readable summary.

## Platform

| Item | Specification |
| --- | --- |
| MCU | ESP32-C3, RISC-V single core, 160 MHz |
| Flash | 8 MB, no PSRAM |
| Display | ST7789P3, 240 × 320 portrait RGB565, SPI2 |
| Input | `UP` / `DOWN` / `OK`, ADC resistor ladder on GPIO0 |
| Audio | ES8311, I2S0 full duplex |
| Battery | CW2017 fuel gauge on I2C0 |
| Wireless | 2.4 GHz WiFi only |
| Console | Native USB Serial/JTAG |

This app uses the display, buttons, battery readings, and WiFi. Audio playback
and recording are not enabled.

## Pins

### Display

| Signal | GPIO |
| --- | --- |
| SPI MOSI | 9 |
| SPI SCLK | 8 |
| CS | 1 |
| DC | 20 |
| RST | not connected to MCU (`-1`, software reset) |
| Backlight | 21 (LEDC PWM) |

### Buttons

| Button | ADC window |
| --- | --- |
| `UP` | 0–150 mV |
| `DOWN` | 150–447 mV |
| `OK` | 447–1900 mV |
| Released | ≥ 1900 mV (about 3300 mV) |

All three buttons share `ADC1_CH0` / GPIO0. Do not use the internal pull-up:
its value and tolerance collapse the three voltage windows.

### I2C

| Signal | GPIO |
| --- | --- |
| SDA | 10 |
| SCL | 7 |

| Device | 7-bit address |
| --- | --- |
| ES8311 | `0x18` |
| CW2017 | `0x63` |

### I2S / Audio

| Signal | GPIO |
| --- | --- |
| MCLK | 6 |
| BCLK | 5 |
| WS | 3 |
| DOUT (MCU → codec) | 2 |
| DIN (codec → MCU) | 4 |
| PA_CTRL | not connected (`-1`) |

### USB

GPIO18/19 are reserved for native USB Serial/JTAG. The default UART0 TX pin
(GPIO21) conflicts with the backlight, so this project uses USB Serial/JTAG as
the console.

## Wireless

- 2.4 GHz WiFi only.
- Setup hotspot prefix `AI-PASSPORT-`, password `aipassport`, web UI at
  `http://192.168.4.1`.
- AP+STA mode is used during nearby-network scanning; the hotspot stays up.
- For data updates the device connects as a STA, downloads data, then
  disconnects.

## Hardware Changes

Change `components/bsp/include/bsp_pins.h` first, then re-check the display
initialization, button voltage windows, I2C addresses, and I2S pins. Application
code must not duplicate these constants. Re-run display, button, WiFi, and
battery acceptance on the modified hardware.
