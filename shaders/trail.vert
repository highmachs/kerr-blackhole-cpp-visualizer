#version 430 core
// Geodesic trail (spec §10): color-graded line strip from oldest to newest.
layout(location = 0) in vec3 in_pos;

uniform mat4 u_viewProj;
uniform int  u_count;

out float v_t; // 0 = oldest, 1 = newest

void main() {
    v_t = float(gl_VertexID) / max(float(u_count - 1), 1.0);
    gl_Position = u_viewProj * vec4(in_pos, 1.0);
}
