set(AUDIO_EMBED_FILES "../assets/jiggle.pcm")
set(HAVE_LP_AUDIO 0)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../assets/lp_change.pcm")
    list(APPEND AUDIO_EMBED_FILES "../assets/lp_change.pcm")
    set(HAVE_LP_AUDIO 1)
endif()

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
    EMBED_FILES ${AUDIO_EMBED_FILES}
    REQUIRES esp_adc esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_driver_usb_serial_jtag esp_hw_support
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_LP_AUDIO=${HAVE_LP_AUDIO})
