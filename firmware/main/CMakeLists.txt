idf_component_register(
    SRCS
        "app_main.c"
        "battery.c"
        "board.c"
        "game_state.c"
        "marshmallon_assets.c"
        "ui.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc esp_driver_gpio esp_driver_usb_serial_jtag esp_hw_support
)
