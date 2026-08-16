# Ben's Marshmallon firmware

Native ESP-IDF/LVGL firmware for the Waveshare ESP32-S3-Touch-LCD-1.54 (SKU 33869). It ports the bundled 240 × 240 browser prototype to the board's ST7789 display and CST816S touchscreen.

## Features

- Two-player Life Point counter with headerless adjustment screens, full-width actions, swipe-back navigation, and separated loss/gain choices
- Marshmallon reactions, game-over screen, and new-game reset
- Tap-to-wiggle Marshmallon interaction
- Contextual side buttons: Player 2/Ben selection, then left=gain and right=lose; hold-center powers off
- Optional animated duel-coin flip hosted by Marshmallon at boot and after New Game (left=flip, right=skip)
- Explicit Start Duel action plus a three-second safety auto-advance after coin results
- Player-specific calculator action label to prevent adjusting the wrong duelist
- A larger, visually aligned calculator Next touch target
- A teardrop loss treatment for Marshmallon when Ben is defeated
- Active-player physical buttons double as Next during valid amount entry
- Canonical Marshmallon artwork rendered directly from the prototype's SVG paths
- Fresh 8000–8000 Life Point state after every power cycle
- Persistent battery voltage/percentage indicator
- Lightning bolt inside the battery meter whenever USB power is detected
- Dismissible warning at 20% and critical warning at 10%
- USB-only indication when no valid battery voltage is detected

Battery percentage is estimated from measured Li-ion voltage. The ADC input on GPIO1 is converted through the board's 200 kOhm / 100 kOhm divider (3:1), averaged over 24 samples, and calibrated with the ESP32-S3's ADC calibration data when available. Charge status is read from the active-low signal on GPIO3.

## Build

Install ESP-IDF 5.5.x, then from this directory run:

```sh
. "$IDF_PATH/export.sh"
idf.py build
```

The component manager resolves and locks LVGL, `esp_lvgl_port`, and the CST816S driver from `main/idf_component.yml` and `dependencies.lock`.

The source-of-truth character vectors are in `assets/marshmallon_*.svg`. Run `python3 tools/generate_marshmallon_assets.py` after changing them to regenerate the transparent PNG previews and LVGL ARGB8888 image data.

## Flash

Connect the board over USB and identify its serial port (for example `/dev/cu.usbmodem101`). Back up the full 16 MB flash before replacing any existing firmware:

```sh
mkdir -p ../backups
python -m esptool --chip esp32s3 --port /dev/cu.usbmodem101 read_flash 0 0x1000000 ../backups/original-full-flash.bin
```

Then flash and monitor:

```sh
idf.py -p /dev/cu.usbmodem101 flash monitor
```

Exit the monitor with `Ctrl-]`.

The backup can contain saved Wi-Fi credentials or other private configuration from the previous firmware. Keep it local; the package `.gitignore` excludes `backups/`.
