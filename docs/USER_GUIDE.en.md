# hf-propagation · User Guide

[简体中文](USER_GUIDE.md) · [Back to README](../README.en.md)

---

## 1. Scope

This application helps plan amateur-radio HF contacts. It estimates:

- `MUF`, `LUF`, path distance, bearing, and daylight-path fraction
- Per-band score and rating for 80m–10m
- Best band and best 24-hour operating window

Inputs are `SFI`, `Kp`, the selected station and target coordinates, and the
current UTC time/date.

> The model is a simplified estimate. It is not a substitute for professional
> propagation software or on-air measurements. Antennas, power, noise, terrain,
> ionospheric disturbances, and operating skill also affect results.

## 2. Preparation

- FoloToy AI Passport device
- Data-capable USB-C cable for flashing or logs
- A 2.4 GHz WiFi network for data updates and auto location
- A phone or computer for the WiFi setup page

The device does not support 5 GHz WiFi.

## 3. Buttons

| Button | Short press | Long press |
| --- | --- | --- |
| `UP` | Move up; previous band; increase value | — |
| `DOWN` | Move down; next band; decrease value | — |
| `OK` | Enter, confirm, start update | Back; save and return from Settings |

While editing a setting, long-press `OK` cancels the edit.

## 4. Overview and Details

The overview shows the selected band, `MUF`, `LUF`, score/rating, best-band
hint, station, target, SFI/Kp, and local time.

- Short `UP`/`DOWN`: change band.
- Short `OK`: open band details.
- Long `OK`: open the menu.

The detail page shows the band frequency, score, rating, best window,
daylight-path fraction, distance, and bearing. Short `UP`/`DOWN` changes band;
long `OK` returns.

Rating meanings:

| Rating | Meaning |
| --- | --- |
| Poor | Not a priority under the current model |
| Fair | Possible, but limited |
| Good | Favorable conditions |
| Excellent | Best modeled conditions |

## 5. Settings

Open `Settings` from the menu. Fields:

| Field | Description |
| --- | --- |
| Station | Beijing, Shanghai, Guangzhou, Chengdu, Urumqi, Harbin, Lhasa, Haikou, or Auto locate |
| Target | Target region |
| Timezone | Local offset from UTC in 30-minute steps |
| SFI | Solar flux index, 70–250 |
| Kp | Planetary K index, 0–9 |
| UTC time | Manual clock setting |
| Date | Manual date setting |
| Location | Read-only: not located or current latitude/longitude |

Operation:

1. Short `UP`/`DOWN` selects a field.
2. Short `OK` enters edit mode.
3. Short `UP`/`DOWN` changes the value.
4. Short `OK` confirms.
5. Long `OK` saves and returns; long `OK` while editing cancels.

## 6. Auto Locate

1. Open `Settings`.
2. Select the Station row and press `OK`.
3. Change it to **Auto locate**, confirm, and save.
4. Complete WiFi setup.
5. Open `Data Update` and press `OK`.
6. The device shows “Getting network location…” and then downloads NOAA data.
7. The Location row shows coordinates such as `36.96N 117.21E`.

The coordinates are stored in NVS and survive a reboot. They are refreshed on
each online data update.

How it works:

- It uses the router's public IP address and a third-party geolocation service.
- It is **not GPS** and does not triangulate nearby WiFi access points.
- Accuracy is normally city-level. VPNs, proxies, carrier gateways, and
  corporate networks can cause errors.
- If lookup fails, the previous coordinates are kept. If none exist, Beijing is
  used as a fallback.
- Fixed cities remain available as an override.
- The geolocation providers receive the device's public IP address.

## 7. WiFi Setup

1. Open `WiFi Setup` and press `OK` to start the hotspot.
2. On a phone or computer, connect to:
   - SSID: `AI-PASSPORT-XXXXXX`
   - Password: `aipassport`
3. Open <http://192.168.4.1>.
4. The page scans nearby 2.4 GHz networks automatically.
5. Tap a network to fill its SSID, enter the password, and save.
6. The device leaves setup mode and returns to its normal screen.

The hotspot supports up to two clients. If scanning briefly interrupts the
connection, the page retries automatically.

## 8. Data Update

1. Complete WiFi setup.
2. Open `Data Update` and press `OK`.
3. The device connects to the router, optionally fetches a network location,
   then downloads NOAA WWV data.
4. On success, it displays `SFI`, `Kp`, `A`, and the NOAA issue time.
5. It saves the indices, recalculates all bands, and disconnects WiFi.

NOAA source: <https://services.swpc.noaa.gov/text/wwv.txt>

If the update fails, check the router's internet connection, WiFi password,
signal strength, DNS/firewall, or retry later.

## 9. Troubleshooting

| Symptom | Check |
| --- | --- |
| “Please set WiFi first” | Save a router WiFi network in WiFi Setup |
| Hotspot not visible | Start the hotspot with `OK`; move closer; check 2.4 GHz |
| My WiFi is not in the scan list | Router may be 5 GHz-only; move closer; hidden SSID must be typed manually |
| Location says “Not located” | Complete WiFi setup and run Data Update; try a fixed city if blocked |
| Location city is wrong | IP geolocation is city-level; VPN/proxy can mislead; choose the nearest fixed city |
| SFI/Kp unchanged | NOAA may not have published a new WWV value; manual input is allowed |
| Time is wrong | Adjust UTC time, date, and timezone in Settings |

## 10. Privacy and Disclaimer

- WiFi credentials stay in local NVS.
- Network location sends the public IP to third-party providers.
- NOAA and location data are model inputs only.
- This is a community derivative application, not official FoloToy firmware.

## 11. Reflash and Reset

- Reconfigure WiFi from `WiFi Setup`.
- A full image flash writes bootloader, partition table, and OTA data; stored
  data may be affected.
- An app-only image at `0x20000` normally keeps NVS when the partition layout
  matches.

See [BUILD.en.md](BUILD.en.md).
