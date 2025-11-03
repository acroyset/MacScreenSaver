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
    if (t < 0 || t > 1) return min(length(p-a), length(p-b));
    vec2 closest = a + t * ab;
    return length(p - closest);
}

vec4 drawLineSegment(vec2 p, vec2 a, vec2 b, float width, vec4 lineColor, vec4 bgColor) {
    width *= length(p-a)/length(a-b);
    lineColor.a *= length(p-a)/length(a-b);
    // Signed distance from line edge: negative inside
    float d = distToSegment(p, a, b) - 0.5 * width;

    // Antialias: transition over ~one pixel
    float aa = fwidth(d); // needs derivatives (GLSL 3.30+ / ES 3.0+)
    float coverage = 1.0 - smoothstep(0.0, aa, d);

    // If you want a hard boolean, use: float onLine = step(d, 0.0);
    return mix(bgColor, lineColor, coverage);
}

// Simple animated gradient; replace with your effect.
void main(){
    // Optional: aspect-corrected UV if you need square pixels
    ivec2 screenSpace = ivec2(int(fragCoord.x*resolution.x), int(fragCoord.y*resolution.y));
    if (screenSpace.x > 2000 || screenSpace.y > 1000) return;
    vec2 uv = fragCoord * vec2(float(resolution.x)/float(resolution.y), 1.0);

    vec3 color = vec3(
            perlinNoise(vec3(screenSpace, int(iTime*200)), 800, uvec3(resolution, 3200), 0u),
            perlinNoise(vec3(screenSpace, int(iTime*200)), 800, uvec3(resolution, 3200), 1u),
            perlinNoise(vec3(screenSpace, int(iTime*200)), 800, uvec3(resolution, 3200), 2u)
        );
    color *= 2.5*perlinNoise(vec3(screenSpace, int(iTime*200)), 1600, uvec3(resolution, 3200), 4u);

    int scale = 200;

    int overlap = 5;

    vec4 accum = vec4(0);

    for (int x = -scale + scale/overlap; x <= scale - scale/overlap; x += scale/overlap){
        for (int y = -scale + scale/overlap; y <= scale - scale/overlap; y += scale/overlap){
            ivec2 a = screenSpace/scale*scale + ivec2(scale/2) + ivec2(x, y);

            float angle = 2*3.14159265*perlinNoise(vec3(a, int(iTime*200)), 800, uvec3(resolution, 3200), 5u);
            vec2 vector = vec2(cos(angle), sin(angle));

            float mag = perlinNoise(vec3(screenSpace, int(iTime*200)), 1600, uvec3(resolution, 3200), 4u);

            ivec2 b = a+ivec2(vector*scale*0.9*mag);

            accum += drawLineSegment(screenSpace, a,b, 10, vec4(color,1), vec4(0));
        }
    }
    FragColor = accum;
}
