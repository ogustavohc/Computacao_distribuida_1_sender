idf_component_register(SRCS "display_spi_config.c" "main.c"
                    INCLUDE_DIRS "." REQUIRES esp_wifi esp_netif nvs_flash esp_lcd driver esp_timer esp_driver_uart)