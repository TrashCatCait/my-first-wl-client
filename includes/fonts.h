#pragma once 

#include <ft2build.h>
#include <freetype/freetype.h>

#include <stdint.h>


typedef struct freetype {
    FT_Library lib;
    FT_Face face;

} ft_font_t, *ft_font_ptr;



void ft_init(ft_font_ptr font, char *fntpath);
void ft_render_text_to_fb(uint32_t *fb, char *str, int32_t width, int32_t height, int32_t x, int32_t y, ft_font_t font);
