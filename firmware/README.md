# Ben's Marshmallon firmware

Native ESP-IDF/LVGL firmware for the Waveshare ESP32-S3-Touch-LCD-1.54 (SKU 33869). It ports the bundled 240 × 240 browser prototype to the board's ST7789 display and CST816S touchscreen.

## Features

- Startup choice between Yu-Gi-Oh (8000 LP per player) and two-player MTG Standard (20 life per player), by touch or side buttons
- Life counter with headerless adjustment screens, full-width actions, swipe-back navigation, and separated loss/gain choices
- Format-specific characters: Marshmallon for Yu-Gi-Oh, Jace Beleren for MTG
- Character reactions, game-over screen, and new-game reset
- Tap-to-wiggle Marshmallon interaction
- Cartoon gelatin jiggle through the board's ES8311 speaker when Marshmallon is tapped
- Rapid taps restart the jiggle sound with a short fade so playback follows each new wiggle
- The same Life Point cue plays when either player's points increase or decrease
- Contextual side buttons: Player 2/Ben selection, then left=gain and right=lose; hold-center powers off
- Tap Jace for a floating spell pulse in MTG's blue frame
- Optional animated coin flip hosted by the selected character after game selection and New Game (left=flip, right=skip)
- Explicit Start Duel action plus a three-second safety auto-advance after coin results
- Player-specific calculator action label to prevent adjusting the wrong duelist
- A larger, visually aligned calculator Next touch target
- A teardrop loss treatment for Marshmallon when Ben is defeated
- Active-player physical buttons double as Next during valid amount entry
- Canonical Marshmallon artwork rendered directly from the prototype's SVG paths
- Fresh format selection after every power cycle; New Game resets the currently selected format
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

MTG uses original stylized Jace artwork in `assets/jace_*.svg`, based on his classic hooded appearance. Regenerate its previews and image data with `python3 tools/generate_jace_assets.py` (Pillow plus either `rsvg-convert` or CairoSVG). The startup picker, life counter, coin scene, and results use the selected format's character; Jace's tap pulse is silent.

The jiggle sample is `assets/jiggle.pcm` (22,050 Hz, 16-bit mono). Run `python3 tools/generate_jello_jiggle.py` to regenerate it and the preview `assets/jiggle.wav`. The firmware duplicates the sample into stereo and sends it by I2S DMA to the ES8311. Audio uses MCLK/BCLK/WS/DOUT on GPIO 8/9/10/12 and speaker amplifier enable on GPIO 7; the codec shares the touchscreen's I2C bus on GPIO 41/42.

The optional personal-use Life Point cue is `assets/lp_change.pcm` (22,050 Hz, 16-bit mono). When present at build time, it plays for both gain and loss in either format; the jiggle sample still plays when Marshmallon is tapped. The Life Point recording is kept local and is not included in the public repository. A clean checkout builds without it, leaving Life Point changes silent until a personal sample is supplied.

To rebuild this local version from your own copy of the selected clip, run `ffmpeg -i /path/to/lp-clip.mp3 -af volume=7dB -ac 1 -ar 22050 -c:a pcm_s16le -f s16le assets/lp_change.pcm` before `idf.py build`.

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
