idf_component_register(SRCS "st7567_hw.c"
                    REQUIRES esp_driver_gpio freertos esp_rom
                    INCLUDE_DIRS "include")
