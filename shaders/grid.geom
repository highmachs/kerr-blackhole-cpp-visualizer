#version 460 core
// Optional geometry shader that emits the Boyer-Lindquist coordinate grid.
// We feed it GL_POINTS (one per grid line index) and expand to line strips.
// This is reserved for the debug-grid toggle; the main path renders the grid
// directly inside compute.glsl (see gridContribution()), so this shader is
// kept as a reference implementation should the host code switch to a
// rasterized overlay later.

layout(points) in;
layout(line_strip, max_vertices = 64) out;

uniform mat4 u_viewProj;
uniform float u_M;
uniform float u_a;

const float PI = 3.14159265358979323846;

void emitRing(float r, float theta, vec3 color) {
    for (int i = 0; i <= 32; ++i) {
        float ph = float(i) / 32.0 * 2.0 * PI;
        vec3 p = vec3(r * sin(theta) * cos(ph),
                      r * cos(theta),
                      r * sin(theta) * sin(ph));
        gl_Position = u_viewProj * vec4(p, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}

void main() {
    emitRing(3.0 * u_M, PI * 0.5, vec3(0.79, 0.49, 0.35));
    emitRing(5.0 * u_M, PI * 0.5, vec3(0.79, 0.49, 0.35));
}
