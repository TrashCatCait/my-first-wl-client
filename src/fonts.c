#include <fcft/fcft.h>
#include <stdbool.h>
#include <stdlib.h>



struct fcft_font *font_setup() {
    fcft_init(FCFT_LOG_COLORIZE_NEVER, false, FCFT_LOG_CLASS_NONE);

    fcft_set_scaling_filter(FCFT_SCALING_FILTER_LANCZOS3);
    
    struct fcft_font *font = fcft_from_name( 
	    2,
	    (const char *[]) { 
	    "Fira Code:size=20",
	    "Serif:size=10:weight=bold", 
	    },
	    "slant=italic:dpi=140" 
	    );
   
    
    struct fcft_glyph g;
    

    return font;
}


void font_clean(struct fcft_font *font) {
    fcft_destroy(font);

    fcft_fini();
}
