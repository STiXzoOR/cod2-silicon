/* The one source edit OpenGL needs: vertex shaders write their clip position
 * with an OpenGL world-view-projection matrix, because Direct3D and OpenGL
 * disagree on the clip-space depth range and the Y direction of render
 * targets. Everything else in the game's HLSL is compiled as shipped. */
#ifndef SHADERGEN_GLCLIP_H
#define SHADERGEN_GLCLIP_H

#include <stddef.h>

/* rel is the path below the shader folder, with forward slashes
 * ("lib/globals.hlsl", "sky.hlsl"). text must use "\n" line ends. Returns a
 * malloc'd, NUL-terminated copy with the edit applied (or unchanged when the
 * file needs none) and its length in *out_size. Returns NULL and a reason in
 * error when a vertex shader does not have the expected shape. */
char *GlClip_Patch(const char *rel, const char *text, size_t size, size_t *out_size,
                   char *error, size_t error_size);

/* Convert "\r\n" and lone "\r" to "\n" in place; returns the new length. */
size_t GlClip_NormalizeNewlines(char *text, size_t size);

#endif
