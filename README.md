# Ben's Marshmallon

Interactive 240 × 240 prototype for a Yu-Gi-Oh!-style life counter and battle pet.

## Browser prototype

Open `index.html` in any modern browser. The package is self-contained: no dependencies, build step, server, or network requests.

## Controls

- Tap **Ben · P1** or **Player 2**.
- Choose **Loss** or **Gain**.
- Enter any Life Point amount and press **Apply**.
- Marshmallon reacts according to which player gained or lost Life Points.
- Reducing either player to zero opens the animated game-over screen.
- Press **New Game?** to restore both players to 8000 LP.

## Files

- `index.html` — standalone, directly runnable export.
- `source/duel-pet-mock.fragment.html` — editable source fragment containing the SVG artwork, styles, animations, and application logic.

## ESP32 firmware

The interface is designed at the native 240 × 240 resolution of the Waveshare ESP32-S3-Touch-LCD-1.54 (SKU 33869). The original browser prototype remains available in `index.html` and `source/`.

The native ESP-IDF/LVGL port is now in `firmware/`. It preserves the duel-counter flow and adds saved Life Points plus live battery level, charging state, and low/critical battery warnings. See `firmware/README.md` for build and flash instructions.
