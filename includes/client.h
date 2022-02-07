#pragma once 

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <fcft/fcft.h>

typedef struct client {
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_seat *wl_seat;

    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct wl_touchpad *touch;
    //client height and width 
    int32_t height;
    int32_t width;
    int32_t xpos;
    int32_t ypos;
    int32_t xmax;
    int32_t ymax;

    uint8_t closed;

    struct fcft_font *font;
} client_t, *client_ptr;
