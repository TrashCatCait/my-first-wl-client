#include "freetype/fttypes.h"
#include <stdint.h>
#include <stdlib.h>
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h> 
#include <cairo/cairo.h>
#include <ft2build.h>
#include <freetype/freetype.h>

typedef struct freetype {
    FT_Library lib;
    FT_Face face;

} ft_font_t, *ft_font_ptr;

typedef struct client {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    //client height and width 
    int32_t height;
    int32_t width;
    int32_t xpos;
    int32_t ypos;
    int32_t xmax;
    int32_t ymax;

    uint8_t closed;

    ft_font_t font;
} client_t, *client_ptr;

//Generate Ranomd Name for shm 
static void random_name(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long rand = ts.tv_nsec;
    for(int i = 0; i < 6; i++) {
        buf[i] = 'A' + (rand & 15) + (rand & 16) * 2;
        rand >>= 5;
    }
}

static int create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        random_name(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

int allocate_shm(size_t size) {
    int fd = create_shm_file();
    if(fd < 0) {
        return -1;
    }
    
    int ret; 
    do {
        ret = ftruncate(fd, size);
    } while(ret < 0 && errno == EINTR);

    if(ret < 0) {
        close(fd);
        return -1;
    }
    return fd;

}

void xdg_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_ping,
};

static void registry_handle_global(void *data, struct wl_registry *wl_registry, uint32_t name,
        const char *interface, uint32_t version) {
    client_ptr state = data;

    printf("registry handle global called interface: %s, version: %u, name: %u ", interface, version, name);
    if(strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = wl_registry_bind(wl_registry, name,
                &wl_compositor_interface, version);
        printf("Binding Wayland Compositor\n");
    } else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, version);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
        printf("Binding XDG wm base\n");
    } else if(strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
        printf("Binding wl_shm \n");
    } else {
        printf("<-- (Unhandled Event)\n");
    }
}

static void registry_handle_remove(void *data, struct wl_registry *registry, uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_remove, 
};

//Compositor tells us when to destroy the buffer 
void wl_buffer_release(void *data, struct wl_buffer *buffer) {
    wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

void ft_init(client_ptr client) {
    
    if(FT_Init_FreeType(&client->font.lib) == 0) {
        if(FT_New_Face(client->font.lib, "/usr/share/fonts/TTF/DejaVuSans.ttf", 0, &client->font.face) == 0) {

        } else {
            printf("Face Error\n");
            FT_Done_FreeType(client->font.lib);
            return;
        }
    } else {
        printf("Libaray Error\n");
    }
}

void ft_render_text_to_fb(uint32_t *fb, char *str, client_ptr client) {

    while(*str != '\0') {
        int ptsz = 20; //Pixel size of font 
        if(FT_Set_Pixel_Sizes(client->font.face, 0, ptsz) == 0) {
            FT_ULong character = *str;
            FT_UInt gi = FT_Get_Char_Index(client->font.face, character);
            if(gi != 0) {
                FT_Load_Glyph(client->font.face, gi, FT_LOAD_NO_BITMAP);

                int bbox_ymax = client->font.face->bbox.yMax / 64;
                int glyph_width = client->font.face->glyph->metrics.width / 64;
                int advance = client->font.face->glyph->metrics.horiAdvance / 64;
                int x_off = (advance - glyph_width) / 2;
                int y_off = bbox_ymax - client->font.face->glyph->metrics.horiBearingY / 64;
                FT_Render_Glyph(client->font.face->glyph, FT_RENDER_MODE_NORMAL);

                for(int i = 0; i < (int)client->font.face->glyph->bitmap.rows; i++) {
                    int row_offset = client->ypos + i + y_off;
                    for(int j = 0; j < (int)client->font.face->glyph->bitmap.width; j++) {
                        unsigned char ptr = client->font.face->glyph->bitmap.buffer[i * client->font.face->glyph->bitmap.pitch + j];
                        if(client->xpos + j + x_off >= client->width) {
                            client->ypos += bbox_ymax;
                            client->xpos = 0;
                        }
                        if(ptr) {
                            fb[(client->xpos + j + x_off) + (client->ypos + i + y_off) * client->width] = 0xfff008f8;
                        }
                    }
                }
                client->xpos += advance;
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

struct wl_buffer *draw(client_ptr client) {
    const int width = client->width;
    const int height = client->height;
    int stride = width * 4;
    int size = stride * height;
    
    client->xpos = 0;
    client->ypos = 0;

    int fd = allocate_shm(size);
    if (fd == -1) {
        printf("SHM ERROR\n");
        return NULL;
    }

    uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(data == MAP_FAILED) {
        printf("Map Failed\n");
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(client->wl_shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
            width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    /* Draw checkerboxed background */
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
                data[y * width + x] = 0xaa282a36;
        }
    }
    
    ft_render_text_to_fb(data, "Hello FT, World! Now I Can Render Fonts. So Now We Should Have new Lines when it gets to long", client);
    

    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial) {
    client_ptr client = data;

    xdg_surface_ack_configure(surface, serial);
    
    struct wl_buffer *buffer = draw(client); 
    wl_surface_attach(client->wl_surface, buffer, 0, 0);
    wl_surface_commit(client->wl_surface);
}

static struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    client_ptr state = data;
    state->closed = 1;
}

//Compositor should give us a height and width that we should adhire to if it doesn't don't set it 
static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, 
        int32_t height,  struct wl_array *states) {
    client_ptr state = data;
    if(width == 0 || height == 0) {
        return;
    }

    state->width = width;
    state->height = height;
}

static struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
};


static client_t client = { 0 };

void cleanup(client_ptr client) {
    FT_Done_FreeType(client->font.lib);
        
    xdg_toplevel_destroy(client->xdg_toplevel);
    xdg_surface_destroy(client->xdg_surface);
    wl_surface_destroy(client->wl_surface);
    wl_shm_destroy(client->wl_shm);
    xdg_wm_base_destroy(client->xdg_wm_base);
    wl_compositor_destroy(client->wl_compositor);
    wl_registry_destroy(client->wl_registry);
    wl_display_disconnect(client->wl_display);
}

void sig_handle(int sig) {
    cleanup(&client);

    exit(1);
}

int main(int argc, char **argv) {
    
    signal(SIGTERM, sig_handle);
    signal(SIGINT, sig_handle);

    client.height = 400;
    client.width = 640;
    ft_init(&client);    
    client.wl_display = wl_display_connect(NULL);
    client.wl_registry = wl_display_get_registry(client.wl_display);

    wl_registry_add_listener(client.wl_registry, &registry_listener, &client);
    //Get Compositor 
    wl_display_roundtrip(client.wl_display);

    client.wl_surface = wl_compositor_create_surface(client.wl_compositor);
    
    client.xdg_surface = xdg_wm_base_get_xdg_surface(client.xdg_wm_base, client.wl_surface);
    xdg_surface_add_listener(client.xdg_surface, &xdg_surface_listener, &client);

    client.xdg_toplevel = xdg_surface_get_toplevel(client.xdg_surface);
    xdg_toplevel_add_listener(client.xdg_toplevel, &xdg_toplevel_listener, &client);

    xdg_toplevel_set_title(client.xdg_toplevel, "Example client");
    wl_surface_commit(client.wl_surface);
 
    while (wl_display_dispatch(client.wl_display)) {
        if(client.closed == 1) {
            
        }
    }

    cleanup(&client);

    return 0;
}
