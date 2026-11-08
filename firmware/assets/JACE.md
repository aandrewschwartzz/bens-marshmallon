The `jace_*.svg` files are original stylized vector illustrations of Jace Beleren for this device's MTG mode. They use his classic blue hood and mind magic; no published card illustration was copied into the firmware.

Character reference: [Jace Beleren, Wizards of the Coast](https://magic.wizards.com/en/story/jace-beleren-planeswalker).

Run `python3 tools/generate_jace_assets.py` to regenerate the transparent previews and LVGL image data. Requires Pillow and either `rsvg-convert` or CairoSVG. On macOS with Homebrew Cairo, CairoSVG may also need `DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib`.
