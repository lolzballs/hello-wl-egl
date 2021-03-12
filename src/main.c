#include <assert.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "egl_common.h"
#include "gl_render.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

static bool running = true;
static int32_t width;
static int32_t height;

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct xdg_wm_base *xdg_wm_base;
static struct zxdg_decoration_manager_v1 *xdg_decoration_manager;

static struct wl_surface *surface;
static struct xdg_toplevel *xdg_toplevel;

static struct wl_egl_window *egl_window;
static EGLSurface egl_surface;

static struct wl_callback *frame_callback;

static void draw(void);

static void
noop() {
	// This space intentially left blank
}

static void
surface_frame_callback(void *data, struct wl_callback *cb, uint32_t time) {
	wl_callback_destroy(cb);
	frame_callback = NULL;
	draw();
}

static struct wl_callback_listener frame_listener = {
	.done = surface_frame_callback,
};

static void
draw(void) {
	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

	gl_render_draw();

	frame_callback = wl_surface_frame(surface);
	wl_callback_add_listener(frame_callback, &frame_listener, NULL);

	eglSwapBuffers(egl_display, egl_surface);
}

static void
xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface,
		uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	if (egl_window != NULL) {
		wl_egl_window_resize(egl_window, width, height, 0, 0);
	}

	wl_surface_commit(surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure_handler,
};

static void
xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
	running = false;
}

static void
xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel,
		int32_t reqwidth, int32_t reqheight, struct wl_array *states) {
	if (reqwidth == 0) {
		width = 640;
	} else {
		width = reqwidth;
	}
	if (reqheight == 0) {
		height = 480;
	} else {
		height = reqheight;
	}
	printf("toplevel configure: %d %d\n", width, height);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure_handler,
	.close = xdg_toplevel_close_handler,
};

static void
global_registry_handler(void *data, struct wl_registry *registry, uint32_t id,
		const char *interface, uint32_t version) {
	printf("Got a registry event for %s id %d v%d\n", interface, id, version);
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, id,
				&wl_compositor_interface, 4);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		xdg_wm_base = wl_registry_bind(registry, id,
				&xdg_wm_base_interface, 1);
	} else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		xdg_decoration_manager = wl_registry_bind(registry, id,
				&zxdg_decoration_manager_v1_interface, 1);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = global_registry_handler,
	.global_remove = noop,
};

int
main(int argc, char **argv) {
	display = wl_display_connect(NULL);
	assert(display);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	assert(compositor);
	assert(xdg_wm_base);
	assert(xdg_decoration_manager);

	eglBindAPI(EGL_OPENGL_ES_API);
	egl_init(display);

	surface = wl_compositor_create_surface(compositor);
	struct xdg_surface *xdg_surface =
		xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
	xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
	xdg_toplevel_set_title(xdg_toplevel, "Wayland Sandbox");
	struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration =
		zxdg_decoration_manager_v1_get_toplevel_decoration(
				xdg_decoration_manager, xdg_toplevel);
	zxdg_toplevel_decoration_v1_set_mode(xdg_toplevel_decoration,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);

	wl_surface_commit(surface);
	wl_display_roundtrip(display);

	egl_window = wl_egl_window_create(surface, width, height);
	egl_surface = eglCreatePlatformWindowSurfaceEXT(egl_display,
			egl_config, egl_window, NULL);

	wl_display_roundtrip(display);

	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
	gl_render_init();

	draw();

	while (wl_display_dispatch(display) != -1 && running) {
		noop();
	}

	return EXIT_SUCCESS;
}

