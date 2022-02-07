#define _POSIX_C_SOURCE 200112L
#include <fcft/fcft.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <wayland-client.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <unistd.h>
#include <xdg-shell-client-protocol.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h> 
#include <stdint.h>
#include <stdlib.h>
#include <shm.h>
#include <client.h>
#include <fonts.h>
#include <pixman.h>

void xdg_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_ping,
};

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
    struct client_state *state = data;
}

static void wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name) {
    printf("seat name: %s\n", name);
}


static struct wl_seat_listener seat_listener = {
    .capabilities = wl_seat_capabilities,
    .name = wl_seat_name
};


static void registry_handle_global(void *data, struct wl_registry *wl_registry, uint32_t name,
        const char *interface, uint32_t version) {
    client_ptr state = data;
     

    printf("registry handle global called interface: %s, version: %u, name: %u ", 
	    interface, version, name
	    );

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
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        state->wl_seat = wl_registry_bind(wl_registry, name,
		&wl_seat_interface, 7);
        wl_seat_add_listener(state->wl_seat, &seat_listener, state);
	printf("Binding wl_seat\n");
    }

    else {
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

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
                data[y * width + x] = 0xaa282828;
        }
    }
    
    //Attach our data mmap to a pixmap image
    pixman_image_t *canvas = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, data, width * sizeof(uint32_t)); 
    
    static const uint32_t hello[] = U"Hello World!";

    struct {
	int x;
	int y;
    } pen = {.x = 0, .y = 0};
    
    pixman_image_t *color = pixman_image_create_solid_fill(
	    &(pixman_color_t) {
		.red = 0xffff,
		.green = 0xffff,
		.blue = 0xffff,
		.alpha = 0xffff,
	    });

    for(size_t i = 0; i < sizeof(hello) / sizeof(hello[0]) - 1; i++) {
	const struct fcft_glyph *glyph = fcft_rasterize_char_utf32(
		client->font,
		hello[i],
		FCFT_SUBPIXEL_NONE
		);

	if(glyph == NULL) 
	    continue;

	long x_kern = 0;
	if(i > 0) 
	    fcft_kerning(client->font, hello[i - 1], hello[i], &x_kern, NULL);
	
	pen.x += x_kern;

       if (pixman_image_get_format(glyph->pix) == PIXMAN_a8r8g8b8) {
	   /* Glyph is a pre-rendered image; typically a color emoji */
	   pixman_image_composite32(
	       PIXMAN_OP_OVER, glyph->pix, NULL, canvas, 0, 0, 0, 0,
	       pen.x + glyph->x, pen.y + client->font->ascent - glyph->y,
	       glyph->width, glyph->height);
       }

       else {
	   /* Glyph is an alpha mask */
	   pixman_image_composite32(
	       PIXMAN_OP_OVER, color, glyph->pix, canvas, 0, 0, 0, 0,
	       pen.x + glyph->x, pen.y + client->font->ascent - glyph->y,
	       glyph->width, glyph->height);

       }
       /* Advance pen position */
       pen.x += glyph->advance.x;


    }
    pixman_image_unref(canvas);
    pixman_image_unref(color); 
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

//Handle compositor send us the close event
static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    client_ptr state = data;
    printf("closing\n");
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


void cleanup(client_ptr client) {
    font_clean(client->font);

    xdg_toplevel_destroy(client->xdg_toplevel);
    xdg_surface_destroy(client->xdg_surface);
    wl_surface_destroy(client->wl_surface);
    wl_shm_destroy(client->wl_shm);
    xdg_wm_base_destroy(client->xdg_wm_base);
    wl_compositor_destroy(client->wl_compositor);
    wl_registry_destroy(client->wl_registry);
    wl_display_disconnect(client->wl_display);
}

static client_t client = { 0 };

void sig_handle(int sig) {
    cleanup(&client);
    exit(1);
}

int main(int argc, char **argv) {
    
    signal(SIGTERM, sig_handle);
    signal(SIGINT, sig_handle);
    
    client.font = font_setup();
    client.height = 400;
    client.width = 640;
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
	    break; 
        }
    }

    cleanup(&client);

    return 0;
}
