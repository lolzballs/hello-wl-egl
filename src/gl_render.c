#include <GLES3/gl3.h>
#include <stdlib.h>
#include <stdio.h>

#include "gl_render.h"

static const char *vert_shader_text =
	"#version 320 es\n"
	"uniform mat4 transform;\n"
	"layout(location=0) in vec4 pos;\n"
	"layout(location=1) in vec4 color;\n"
	"out vec4 v_color;\n"
	"void main() {\n"
	"  gl_Position = transform * pos;\n"
	"  v_color = color;\n"
	"}\n";
static const char *frag_shader_text =
	"#version 320 es\n"
	"precision mediump float;\n"
	"in vec4 v_color;\n"
	"out vec4 color;\n"
	"void main() {\n"
	"  color = v_color;\n"
	"}\n";

static GLuint shader_program;
static GLint transform_uniform;

static void
init_transform() {
	static const GLfloat transform[4][4] = {
		{ 1.0, 0.0, 0.0, 0.0 },
		{ 0.0, 1.0, 0.0, 0.0 },
		{ 0.0, 0.0, 1.0, 0.0 },
		{ 0.0, 0.0, 0.0, 1.0 },
	};

	glUseProgram(shader_program);
	glUniformMatrix4fv(transform_uniform, 1, GL_FALSE, (GLfloat *) transform);
}

static GLuint
compile_shader(const char *source, GLenum shader_type) {
	GLuint shader = glCreateShader(shader_type);

	glShaderSource(shader, 1, (const char **) &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "error compiling: %.*s", len, log);
		exit(EXIT_FAILURE);
	}
	return shader;
}

void
gl_render_init(void) {
	const uint8_t *version = glGetString(GL_VERSION);
	printf("%s\n", version);

	GLuint vert_shader = compile_shader(vert_shader_text, GL_VERTEX_SHADER);
	GLuint frag_shader = compile_shader(frag_shader_text, GL_FRAGMENT_SHADER);

	shader_program = glCreateProgram();
	glAttachShader(shader_program, vert_shader);
	glAttachShader(shader_program, frag_shader);
	glLinkProgram(shader_program);

	GLint status;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(shader_program, 1000, &len, log);
		fprintf(stderr, "error linking: %.*s", len, log);
		exit(EXIT_FAILURE);
	}

	transform_uniform = glGetUniformLocation(shader_program, "transform");
	init_transform();
}

void
gl_render_draw(void) {
	static const GLfloat verts[3][2] = {
		{ -0.5, -0.5 },
		{ 0.5, -0.5 },
		{ 0, 0.5 },
	};
	static const GLfloat colors[3][3] = {
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 },
	};

	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shader_program);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, colors);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
}
