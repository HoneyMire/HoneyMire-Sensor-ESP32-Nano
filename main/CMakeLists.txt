idf_component_register(
    SRCS "main.c" "telnet_server.c" "ssd1306.c" "display_task.c" "intel.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_event esp_netif esp_timer esp_wifi json mbedtls nvs_flash spi_flash driver
)
