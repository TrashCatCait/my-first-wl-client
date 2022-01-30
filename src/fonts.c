#include <ft2build.h>
#include <freetype/freetype.h>
#include <stdint.h>
#include <client.h>

void ft_init(ft_font_ptr font, char *fntpath) {
    
    if(FT_Init_FreeType(&font->lib) == 0) {
        if(FT_New_Face(font->lib, fntpath, 0, &font->face) == 0) {

        } else {
            printf("Face Error\n");
            FT_Done_FreeType(font->lib);
            return;
        }
    } else {
        printf("Libaray Error\n");
    }
}

void ft_render_text_to_fb(uint32_t *fb, char *str, int32_t width, int32_t height, int32_t x, int32_t y, ft_font_t font) {
    y = 0;
    while(*str != '\0') {
        int ptsz = 10; //Pixel size of font 
        if(FT_Set_Pixel_Sizes(font.face, 0, ptsz) == 0) {
            FT_ULong character = *str;
            FT_UInt gi = FT_Get_Char_Index(font.face, character);
            if(gi != 0) {
                FT_Load_Glyph(font.face, gi, FT_LOAD_NO_BITMAP);

                int bbox_ymax = font.face->bbox.yMax / 64;
                int glyph_width = font.face->glyph->metrics.width / 64;
                int advance = font.face->glyph->metrics.horiAdvance / 64;
                int x_off = (advance - glyph_width) / 2;
                int y_off = bbox_ymax - font.face->glyph->metrics.horiBearingY / 64;
                FT_Render_Glyph(font.face->glyph, FT_RENDER_MODE_NORMAL);

                for(int i = 0; i < (int)font.face->glyph->bitmap.rows; i++) {
                    int row_offset = y + i + y_off;
                    for(int j = 0; j < (int)font.face->glyph->bitmap.width; j++) {
                        unsigned char ptr = font.face->glyph->bitmap.buffer[i * font.face->glyph->bitmap.pitch + j];
                        if(x + j + x_off >= width) {
                            y += row_offset;
                            x = 0;
                        }
			if(y + i + y_off >= height) {
			    printf("Refusing to render not enough space\n");
			    break;
			}
                        if(ptr) {
                            fb[(x + j + x_off) + (y + i + y_off) * width] = 0xfff008f8;
                        }
                    }
                }
                x += advance;
                str++;
            } else {
                printf("Glyph Index Error\n");
                //Nothing 
            }
        } else {
            printf("Error Setting Font Size\n");
            //Nothing 
        }
    }
}

