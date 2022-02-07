#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

enum pointer_event_mask {
    PTR_EVENT_ENTER =  1 << 0,
    PTR_EVENT_LEAVE = 1 << 1,
    PTR_EVENT_MOTION = 1 << 2,
    PTR_EVENT_BUTTON = 1 << 3,
    PTR_EVENT_AXIS = 1 << 4,
    PTR_EVENT_AXIS_SOURCE = 1 << 5,
    PTR_EVENT_AXIS_STOP = 1 << 6, 
    PTR_EVENT_AXIS_DISCRETE = 1 << 7,
};


typedef struct axis {
    bool valid;
    wl_fixed_t value;
    int32_t discrete;
} axis_t, *axis_ptr;


typedef struct ptr_event {
    uint32_t event_mask;
    wl_fixed_t surface_x; 
    wl_fixed_t surface_y;
    uint32_t button;
    uint32_t status;
    uint32_t time; 
    uint32_t serial;
    axis_t axes[2];
    uint32_t axis_source;
} ptr_event_t, *ptr_event_ptr;

void wl_pointer_enter(void *data);

struct wl_pointer_listener list = {
    .enter = wl_pointer_enter,
};
