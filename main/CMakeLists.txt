idf_component_register(
    SRCS "main.c"
         "prop_model.c"
         "app_settings.c"
         "app_clock.c"
         "app_wifi.c"
         "app_wifi_config.c"
         "app_location.c"
         "app_location_parse.c"
         "app_spaceweather.c"
         "hf_app_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp nvs_flash
    PRIV_REQUIRES esp_timer esp_wifi esp_netif esp_http_server esp_event esp_http_client esp-tls mbedtls
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/hf_font_14.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/hf_font_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/hf_font_20.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/hf_font_28.c"
)

