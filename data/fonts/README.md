Place runtime metadata fallback fonts here for Korean/Japanese rendering.
Either `.ttf` or `.otf` names below are supported:

- `NotoSansKR-Regular.ttf` or `NotoSansKR-Regular.otf` (required for Korean)
- `NotoSansJP-Regular.ttf` or `NotoSansJP-Regular.otf` (optional, improves Japanese/Kanji fallback)

They are loaded by `ui_metadata_fonts_init()` from:

- `/fonts/NotoSansKR-Regular.ttf` or `/fonts/NotoSansKR-Regular.otf`
- `/fonts/NotoSansJP-Regular.ttf` or `/fonts/NotoSansJP-Regular.otf`

Important size note:
- With the current `partitions_ota_spiffs7m.csv`, SPIFFS is ~7.5MB.
- Full KR + full JP files together can exceed SPIFFS.
- For Korean-only verification, keep only KR font in `data/fonts/` and remove JP temporarily.

After adding files, upload filesystem image:

- `pio run -e esp32-p4 -t uploadfs`
