idf_component_register(
    SRCS "display_gfx.c" 
    "fonts/ascii_5x8_fonts.c" 
    "fonts/ascii_8x16_fonts.c" 
    "fonts/ascii_16x32_fonts.c"
                    
    INCLUDE_DIRS "include"
    REQUIRES st7567_driver)
