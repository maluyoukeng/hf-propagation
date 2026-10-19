# hf-propagation · Build, Test, and Flash

[简体中文](BUILD.md) · [Back to README](../README.en.md)

---

## 1. Requirements

| Item | Requirement |
| --- | --- |
| Target | ESP32-C3 |
| ESP-IDF | **5.5.3** |
| Python | 3.10 or newer |
| Host C compiler | For host tests only (`cc`/GCC/Clang) |
| Node.js + npx | For font regeneration only |

ESP-IDF installation and activation:
<https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/get-started/>

Dependencies are pinned in `dependencies.lock`:

- `lvgl/lvgl` 9.5.0
- `espressif/esp_lvgl_port` 2.9.0
- `espressif/button` 4.2.0
- `espressif/esp_codec_dev` 1.6.2

`components/bsp/idf_component.yml` declares the BSP dependencies. ESP-IDF
downloads `managed_components/` on the first build; that directory is not
committed.

## 2. Build

From the repository root:

```bash
idf.py set-target esp32c3
idf.py build
```

Outputs:

- `build/hf-propagation.bin` — application image
- `build/hf-propagation.elf` — debug symbols
- `build/hf-propagation.map` — link map
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`

Create the merged image:

```bash
idf.py merge-bin -o build/hf-propagation-full.bin
```

The result is `build/hf-propagation-full.bin`, flashed from `0x0`.
ESP-IDF 5.5 writes `merged-binary.bin` by default; `-o` sets the desired name.

## 3. Partition Layout

| Name | Type | Offset | Size |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x9000` | 24 KB |
| `phy_init` | data/phy | `0xf000` | 4 KB |
| `otadata` | data/ota | `0x10000` | 8 KB |
| `ota_0` | app/ota_0 | `0x20000` | 4032 KB |
| `ota_1` | app/ota_1 | `0x410000` | 4032 KB |

The dual OTA partitions are used for safe app-only flashing. This application
does not expose a software OTA update entry.

## 4. Flash

### 4.1 Full image

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  --before default_reset --after hard_reset \
  write_flash 0x0 build/hf-propagation-full.bin
```

or:

```bash
idf.py -p COMx flash
```

A full flash writes bootloader, partition table, and OTA data. Whether NVS is
preserved depends on the exact write range; do not assume WiFi settings or
parameters will survive.

### 4.2 Application-only update

The device must already have the matching partition layout:

```bash
python -m esptool --chip esp32c3 -p COMx -b 460800 \
  --before default_reset --after hard_reset \
  write_flash 0x20000 build/hf-propagation.bin
```

This normally preserves NVS data.

### 4.3 Serial access

Use the ESP32-C3 native USB Serial/JTAG port. On Windows it is usually `COMx`;
on Linux it is usually `/dev/ttyACM0`. Close any serial monitor before flashing.

## 5. Host Tests

```bash
bash tools/validate.sh
```

If `cc` is not on PATH:

```bash
CC=/path/to/gcc bash tools/validate.sh
```

The script checks the propagation model, settings and NVS persistence, NOAA
WWV parsing, IP-location JSON parsing, and font coverage.

## 6. Font Regeneration

The committed `assets/fonts/hf_font_*.c` already cover the current UI. After
changing Chinese text in `main/hf_app_ui.c` or `main/prop_model.c`:

```bash
python3 tools/gen_hf_font.py --font /path/to/NotoSansSC-Regular.otf
python3 tests/test_hf_font_coverage.py
```

The generator uses `npx lv_font_conv@1.5.2` and requires Node.js. The source
font is not committed; use a properly licensed Noto Sans SC / Source Han Sans
SC font.

## 7. Common Issues

- **Component download fails:** check access to
  `https://components.espressif.com`, or use a mirror/proxy; remove
  `managed_components/` and rebuild.
- **Partition or flash-size mismatch:** confirm 8 MB Flash and the included
  `partitions.csv` and `sdkconfig.defaults`.
- **Display watchdog after adding UI:** the LVGL pool is 64 KB; increase it
  carefully, remembering that ESP32-C3 has no PSRAM.
- **Build passes but display is blank:** check the LCD/backlight, USB power,
  and serial log for `hf_prop: ready: ...`.
