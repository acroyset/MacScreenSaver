#version 330 core

in vec2 fragCoord;
out vec4 FragColor;

uniform uvec2 resolution; // framebuffer size in pixels
uniform vec2 testVec;
uniform float iTime;      // seconds since start

float smoothstepCubed(float a, float b, float t)  {
    float v = -2*t*t*t + 3*t*t;
    return mix(a, b, v);
}

float randomValue(inout uint state){
    state = state * 747796405u + 2891336453u;
    uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return float(result) * (1/4294967295.0);
}

float perlinNoise(vec3 pos, int scale, uvec3 res, uint state){
    ivec3 base = ivec3(pos/scale)*scale;
    vec3 frac = (pos - vec3(base))/scale;

    float mags[8] = float[8](0,0,0,0,0,0,0,0);

    for (int z = 0; z <= scale; z += scale){
        for (int y = 0; y <= scale; y += scale){
            for (int x = 0; x <= scale; x += scale){
                int index = z/scale*4 + y/scale*2 + x/scale;
                vec3 corner = vec3(base.x+x, base.y+y, base.z+z);
                vec3 stateCorner = vec3(
                    corner.x < res.x ? corner.x : corner.x-res.x,
                    corner.y < res.y ? corner.y : corner.y-res.y,
                    corner.z < res.z ? corner.z : corner.z-res.z
                );
                uint stateTemp = uint(stateCorner.z + stateCorner.y*res.x + stateCorner.x) + state;
                vec3 vector = normalize(vec3(2*randomValue(stateTemp)-1, 2*randomValue(stateTemp)-1, 2*randomValue(stateTemp)-1));
                float mag = dot(vector, (vec3(corner)-pos)/scale);
                mags[index] = (mag+1)/2;
            }
        }
    }

    for (int pass = 1; pass <= 3; pass ++){
        for (int i = 0; i < 8/pow(2, pass-1); i += 2){
            mags[i/2] = smoothstepCubed(mags[i], mags[i+1], frac[pass-1]);
        }
    }

    return mags[0];
}

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
    ivec2 screenSpace = ivec2(int(fragCoord.x*resolution.x), int(fragCoord.y*resolution.y));
    vec2 uv = fragCoord * vec2(float(resolution.x)/float(resolution.y), 1.0);

    int scale = 40;

    vec4 accum = vec4(0);

    int maxR = 120;

    ivec2 minPos = screenSpace-ivec2(maxR);
    ivec2 maxPos = screenSpace+ivec2(maxR);

    if (minPos.x < 0) minPos.x = 0;
    if (minPos.y < 0) minPos.y = 0;

    minPos = minPos/scale*scale;
    maxPos = (maxPos/scale + ivec2(1))*scale;

    for (int x = minPos.x; x <= maxPos.x; x += scale){
        for (int y = minPos.y; y <= maxPos.y; y += scale){
            ivec2 a = ivec2(x, y);

            vec2 HS = vec2(
                perlinNoise(vec3(a, int(iTime*200)), 800, uvec3(resolution, 252000), 0u),
                perlinNoise(vec3(a, int(iTime*200)), 700, uvec3(resolution, 252000), 1u)
            );
            float mag = 2*perlinNoise(vec3(a, int(iTime*200)), 1000, uvec3(resolution, 252000), 3u);
            mag = mag*mag / 2;

            vec3 color = hsb2rgb(vec3(HS, mag));

            color *= 2.5*mag;

            float angle = 2*3.14159265*perlinNoise(vec3(a, int(iTime*200)), 900, uvec3(resolution, 252000), 4u);
            vec2 vector = vec2(cos(angle), sin(angle));

            ivec2 b = a+ivec2(vector*scale*4*mag);

            accum += drawLineSegment(screenSpace, a,b, 10*mag, vec4(color,1), vec4(0));
        }
    }
    FragColor = accum;
}
