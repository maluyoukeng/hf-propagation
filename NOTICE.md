# hf-propagation · Attribution and third-party notices

Project: <https://github.com/maluyoukeng/hf-propagation>

This repository is an independent derivative application for the
**FoloToy AI Passport** hardware platform.

- Upstream platform, BSP, board definitions, and baseline project:
  <https://github.com/FoloToy/ai-passport>
- Upstream license: MIT, Copyright (c) 2026 FoloToy. See `LICENSE`.
- The shortwave propagation model, UI, WiFi provisioning page, NOAA update
  flow, and automatic location feature in this repository are application-layer
  additions maintained by this project's authors.

## Fonts

The generated LVGL font subsets in `assets/fonts/` are derived from
**Noto Sans SC Regular / Source Han Sans SC Regular**, distributed under the
SIL Open Font License 1.1. The original font file is not included in this
repository; only the generated subsets and the glyph inventory are included.

## Data sources

- NOAA SWPC WWV text: <https://services.swpc.noaa.gov/text/wwv.txt>
- IP-based location services used by the device:
  `ip-api.com`, `ipapi.co`, `ipwho.is`, and `freeipapi.com`.
  These services have their own terms and privacy policies. The device sends
  its public IP address to them only when a network location is requested.

NOAA data and third-party location data are used as inputs to an educational
propagation estimate. This project is not affiliated with or endorsed by
FoloToy, NOAA, or any location service provider.

