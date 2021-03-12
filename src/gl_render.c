#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>

#include "gl_render.h"

void
gl_render_init(void) {
	const uint8_t *version = glGetString(GL_VERSION);
	printf("%s\n", version);
}

void
gl_render_draw(void) {

}