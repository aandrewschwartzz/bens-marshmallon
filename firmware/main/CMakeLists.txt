idf_component_register(
    SRCS
        "app_main.c"
        "audio.c"
        "battery.c"
        "board.c"
        "game_state.c"
        "marshmallon_assets.c"
        "ui.c"
    INCLUDE_DIRS "."
    EMBED_FILES "../assets/jiggle.pcm"
    REQUIRES esp_adc esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_usb_serial_jtag esp_hw_support
)
