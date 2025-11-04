#version 430 core

out vec2 fragCoord;

void main() {
    const vec2 verts[3] = vec2[3](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
    );

    vec2 pos = vec2(verts[gl_VertexID].x, verts[gl_VertexID].y);
    gl_Position = vec4(pos, 0.0, 1.0);
    fragCoord = (pos * 0.5) + 0.5;
}

