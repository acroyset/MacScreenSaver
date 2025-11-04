#version 430 core

in vec2 fragCoord;
out vec4 FragColor;

layout(std430, binding = 0) buffer ssboNoiseMap { vec4 noiseMap[]; };

uniform uvec2 resolution; // framebuffer size in pixels
uniform float iTime;      // seconds since start

float distToSegment(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    float t = dot(p - a, ab) / dot(ab, ab); // project p onto segment
    if (t > 1) return length(p-b);
    if (t < 0) return length(p-a);
    vec2 closest = a + t * ab;
    return length(p - closest);
}

vec4 drawLineSegment(vec2 p, vec2 a, vec2 b, float width, vec4 lineColor, vec4 bgColor) {
    if (a == b) return bgColor;
    width *= length(p-a)/length(a-b);
    lineColor *= length(p-a)/length(a-b);
    // Signed distance from line edge: negative inside
    float d = distToSegment(p, a, b) - 0.5 * width;

    // Antialias: transition over ~one pixel
    float aa = fwidth(d); // needs derivatives (GLSL 3.30+ / ES 3.0+)
    float coverage = 1.0 - smoothstep(0.0, aa, d);

    // If you want a hard boolean, use: float onLine = step(d, 0.0);
    return mix(bgColor, lineColor, coverage);
}

vec3 hsb2rgb(vec3 c) {
    // c.x = hue [0.0–1.0], c.y = saturation [0.0–1.0], c.z = brightness [0.0–1.0]
    vec3 rgb = clamp(
    abs(mod(c.x * 6.0 + vec3(0.0, 4.0, 2.0),
    6.0) - 3.0) - 1.0,
    0.0,
    1.0);
    rgb = rgb * rgb * (3.0 - 2.0 * rgb); // smooth curve (optional)
    return c.z * mix(vec3(1.0), rgb, c.y);
}

// Simple animated gradient; replace with your effect.
void main(){
    // Optional: aspect-corrected UV if you need square pixels
    ivec2 screenSpace = ivec2(int(fragCoord.x*resolution.x), int(fragCoord.y*resolution.y)) - ivec2(0, 20);
    vec2 uv = fragCoord * vec2(float(resolution.x)/float(resolution.y), 1.0);

    int scale = 40;
    float speed = 200;

    vec4 accum = vec4(0);

    int maxR = 120;

    ivec2 minPos = screenSpace-ivec2(maxR);
    ivec2 maxPos = screenSpace+ivec2(maxR);

    if (minPos.x < 0) minPos.x = 0;
    if (minPos.y < 0) minPos.y = 0;
    if (maxPos.x >= resolution.x) maxPos.x = int(resolution.x) - 1;
    if (maxPos.y >= resolution.y) maxPos.y = int(resolution.y) - 1;

    minPos = minPos/scale*scale;
    maxPos = (maxPos/scale + ivec2(1))*scale;

    for (int x = minPos.x; x <= maxPos.x; x += scale){
        for (int y = minPos.y; y <= maxPos.y; y += scale){
            ivec2 a = ivec2(x, y);

            uint index = y/scale * resolution.x/scale + x/scale - 1;

            vec2 HS = noiseMap[index].xy;
            float mag = 2*noiseMap[index].z;
            mag = mag*mag / 2;

            vec3 color = hsb2rgb(vec3(HS, mag));

            color *= 2.5*mag;

            float angle = 2*3.14159265*noiseMap[index].w;
            vec2 vector = vec2(cos(angle), sin(angle));

            ivec2 b = a+ivec2(vector*scale*4*mag);

            accum += drawLineSegment(screenSpace, a,b, 10*mag, vec4(color,1), vec4(0));
        }
    }
    FragColor = accum;
}
