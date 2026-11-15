#version 330 core
/* passthrough.vert — fullscreen-triangle vertex shader.
 * No vertex buffers: gl_VertexID generates the three corners,
 * vUV is the 0..1 coordinate across the framebuffer. */
out vec2 vUV;

void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
