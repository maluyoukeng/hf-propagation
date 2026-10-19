# Assets

## Fonts

`fonts/hf_font_14.c`, `hf_font_16.c`, `hf_font_20.c`, and `hf_font_28.c` are
generated LVGL font subsets for the 240 x 320 device display. They contain
printable ASCII plus the Chinese glyphs used by `main/hf_app_ui.c` and
`main/prop_model.c`.

- Source font: Noto Sans SC Regular / Source Han Sans SC Regular.
- License: SIL Open Font License 1.1.
- Generator: `tools/gen_hf_font.py` using `lv_font_conv@1.5.2`.
- Inventory: `fonts/hf_symbols.txt`.

Regenerate the fonts after changing on-device UI text:

```bash
python3 tools/gen_hf_font.py --font /path/to/NotoSansSC-Regular.otf
python3 tests/test_hf_font_coverage.py
```

The browser-only WiFi portal text in `main/app_wifi.c` is rendered by the
phone's browser and is not part of the device font subset.

