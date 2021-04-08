#include <assert.h>
#include <GLES3/gl3.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "egl_common.h"
#include "gl_render.h"
#include "viewporter-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"


static bool running = true;
static int32_t width;
static int32_t height;
static int32_t render_width;
static int32_t render_height;
static struct output *active_output;

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct xdg_wm_base *xdg_wm_base;
static struct zxdg_decoration_manager_v1 *xdg_decoration_manager;
static struct wl_list outputs;

static struct wp_viewporter *wp_viewporter;
static struct wp_viewport *wp_viewport;

static struct wl_surface *surface;
static struct xdg_toplevel *xdg_toplevel;

static struct wl_egl_window *egl_window;
static EGLSurface egl_surface;

static struct wl_callback *frame_callback;

struct output {
	struct wl_output *wl_output;
	int32_t scale;
	struct wl_list link;
};

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

	glViewport(0, 0, render_width, render_height);
	gl_render_draw();

	frame_callback = wl_surface_frame(surface);
	wl_callback_add_listener(frame_callback, &frame_listener, NULL);

	eglSwapBuffers(egl_display, egl_surface);
}

static void
set_render_size(void) {
	if (active_output == NULL) {
		return;
	}

	if (egl_window != NULL) {
		render_width = active_output->scale * width;
		render_height = active_output->scale * height;
		printf("rendering at %dx%d\n", render_width, render_height);
		wl_egl_window_resize(egl_window, render_width, render_height, 0, 0);
	}
	if (wp_viewport != NULL) {
		wp_viewport_set_destination(wp_viewport, width, height);
	}

	wl_surface_commit(surface);
}

static void
output_scale_handler(void *data, struct wl_output *output, int32_t scale) {
	struct output *o = data;
	o->scale = scale;
	set_render_size();
}

static struct wl_output_listener output_listener = {
	.scale = output_scale_handler,
	.done = noop,
	.mode = noop,
	.geometry = noop,
};

static void
surface_enter_callback(void *data, struct wl_surface *surface,
		struct wl_output *output) {
	printf("surface entered an output\n");
	struct output *o;
	wl_list_for_each(o, &outputs, link) {
		if (o->wl_output == output) {
			active_output = o;
			break;
		}
	}
	set_render_size();
}

static void
surface_leave_callback(void *data, struct wl_surface *surface,
		struct wl_output *output) {
	printf("surface left an output\n");
}

static struct wl_surface_listener wl_surface_listener = {
	.enter = surface_enter_callback,
	.leave = surface_leave_callback,
};

static void
xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface,
		uint32_t serial) {
	xdg_surface_ack_configure(xdg_surface, serial);
	set_render_size();
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
	} else if (strcmp(interface, wp_viewporter_interface.name) == 0) {
		wp_viewporter = wl_registry_bind(registry, id,
				&wp_viewporter_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct output *output = calloc(sizeof(struct output), 1);
		output->wl_output = wl_registry_bind(registry, id,
				&wl_output_interface, 3);
		output->scale = 1;
		wl_list_insert(&outputs, &output->link);
		wl_output_add_listener(output->wl_output, &output_listener,
				output);
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

	wl_list_init(&outputs);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	assert(compositor);
	assert(xdg_wm_base);
	assert(xdg_decoration_manager);
	assert(wp_viewporter);

	eglBindAPI(EGL_OPENGL_ES_API);
	egl_init(display);

	surface = wl_compositor_create_surface(compositor);
	wl_surface_add_listener(surface, &wl_surface_listener, NULL);

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
	wp_viewport = wp_viewporter_get_viewport(wp_viewporter, surface);

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

