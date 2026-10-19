# hf-propagation

**Shortwave propagation predictor for the FoloToy AI Passport.**

Project: <https://github.com/maluyoukeng/hf-propagation>

[简体中文](README.md) · [User guide](docs/USER_GUIDE.en.md) · [Build and flash](docs/BUILD.en.md) · [Hardware](docs/HARDWARE.en.md)

---

## Overview

This is a portable HF planning tool for amateur radio operators. It estimates
`MUF`, `LUF`, and the usability of eight amateur bands (80m–10m) from the solar
flux index (`SFI`), planetary K index (`Kp`), day/night path geometry, and the
distance between the selected station and target.

The device works offline. When connected to a 2.4 GHz WiFi router, it can also
download current `SFI`, `Kp`, and `A` values from NOAA SWPC. The station can be
one of eight built-in cities or **Auto locate**, which derives city-level
coordinates from the public IP during a data update.

> This is a community derivative application for the FoloToy AI Passport, not
> official FoloToy firmware. Forecasts are for planning reference only and do
> not replace professional propagation software or on-air measurements.

## Features

- Eight bands: 80m, 40m, 30m, 20m, 17m, 15m, 12m, 10m
- `MUF`, `LUF`, path distance, bearing, and daylight-path fraction
- Best band, per-band score (0–100), and four-level rating
- Best 24-hour operating window
- Eight fixed station cities (Beijing, Shanghai, Guangzhou, Chengdu, Urumqi, Harbin, Lhasa, Haikou) plus **Auto locate**
- Six target regions: East Asia, Southeast Asia, Europe, North America,
  Oceania, South America
- NOAA SWPC space-weather updates over WiFi
- WiFi provisioning: device hotspot, browser-based nearby-WiFi scan/select
- NVS persistence for settings, timezone, station, and location
- Chinese on-device UI, three-button navigation, 240 × 320 display

## Hardware

| Item | Specification |
| --- | --- |
| Device | FoloToy AI Passport |
| MCU | ESP32-C3, 8 MB Flash, no PSRAM |
| Display | ST7789P3, 240 × 320 portrait RGB565 |
| Buttons | `UP` / `DOWN` / `OK`, ADC resistor ladder on GPIO0 |
| Wireless | 2.4 GHz WiFi |
| Audio / battery | ES8311 codec and CW2017 fuel gauge (this app uses battery readings only) |
| Flashing / logs | Native ESP32-C3 USB Serial/JTAG |

See [docs/HARDWARE.en.md](docs/HARDWARE.en.md) for pin details.

## Quick Start

### Build

Requirements: ESP-IDF **5.5.3**, Python 3, and a 2.4 GHz WiFi network for data
updates.

```bash
idf.py set-target esp32c3
idf.py build
```

The build produces:

- `build/hf-propagation.bin` — application image
- `build/hf-propagation-full.bin` — merged image after
  `idf.py merge-bin -o build/hf-propagation-full.bin`

> ESP-IDF 5.5 writes `build/merged-binary.bin` by default; use `-o` to name it
> `hf-propagation-full.bin`.

### Flash

Full flash from `0x0` (partition changes and stored data may be affected):

```bash
idf.py -p COMx flash
```

Application-only update at `0x20000`, keeping NVS when the partition layout
matches:

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  write_flash 0x20000 build/hf-propagation.bin
```

See [docs/BUILD.en.md](docs/BUILD.en.md) for details.

### First use

1. Short-press `UP`/`DOWN` to change bands; short-press `OK` to open details;
   long-press `OK` to open the menu.
2. Open `Settings` and choose the station, target, timezone, and time. `SFI`
   and `Kp` can be entered manually.
3. Open `WiFi Setup` and press `OK` to start the device hotspot.
4. Connect a phone or computer:
   - SSID: `AI-PASSPORT-XXXXXX`
   - Password: `aipassport`
   - URL: <http://192.168.4.1>
5. Tap **Scan nearby WiFi**, choose a 2.4 GHz network, enter its password, and
   save.
6. Open `Data Update` and press `OK`. The device connects to the router,
   optionally obtains a network location, downloads NOAA indices, saves them,
   and recalculates the forecast.

## Controls

| Action | Function |
| --- | --- |
| Short `UP` / `DOWN` | Move selection, change band, adjust a setting |
| Short `OK` | Enter, confirm, start update |
| Long `OK` | Back; save and return from Settings |
| Long `OK` while editing | Cancel the edit |

## Auto Locate

Select **Auto locate** under `Settings → Station`. On each online data update,
the device tries several IP geolocation services and stores city-level
latitude/longitude in NVS.

- It uses the public IP address; it is **not GPS** and does not triangulate
  nearby WiFi access points.
- Accuracy is normally city-level. VPNs, proxies, carrier gateways, and
  corporate networks can cause errors.
- If lookup fails, the previous coordinates are kept. If no coordinates have
  ever been stored, Beijing is used as a fallback.
- Fixed cities remain selectable as an override.
- The geolocation providers receive the device's public IP address.

## Data Sources and Privacy

- Space weather: NOAA SWPC WWV text,
  <https://services.swpc.noaa.gov/text/wwv.txt>
- Network location: `ip-api.com`, `ipapi.co`, `ipwho.is`, `freeipapi.com`
- No accounts, API keys, or personal identity data are stored.
- WiFi credentials stay in local NVS.
- This project has no self-hosted cloud service and does not upload device data
  to its authors.

## Repository Layout

```text
main/                Application, UI, WiFi, NOAA, location, propagation model
components/bsp/      FoloToy AI Passport board support
assets/fonts/        Generated LVGL Chinese font subsets and glyph inventory
docs/                User, build, and hardware documentation
tests/               Host-side tests
tools/               Validation and font-generation scripts
CMakeLists.txt       Project entry point
partitions.csv       NVS + PHY + dual-OTA layout
sdkconfig.defaults   ESP32-C3, USB console, LVGL defaults
dependencies.lock    Locked ESP-IDF component versions
```

## Tests

```bash
bash tools/validate.sh
```

The script covers the propagation model, settings persistence, NOAA parsing,
IP-location parsing, and font coverage. A successful build is not hardware
validation; display, buttons, WiFi, and location accuracy still require a
device test.

## License and Credits

- Based on the BSP and baseline project of
  [FoloToy AI Passport](https://github.com/FoloToy/ai-passport), MIT License.
- Upstream copyright: Copyright (c) 2026 FoloToy; see [LICENSE](LICENSE).
- Font subsets derived from Noto Sans SC / Source Han Sans SC under the SIL
  Open Font License 1.1.
- Space-weather data from NOAA SWPC.

See [NOTICE.md](NOTICE.md) for third-party details.
