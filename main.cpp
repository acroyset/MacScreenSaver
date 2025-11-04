#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#define GLAD_GL_IMPLEMENTATION
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------- Globals ----------
static GLFWwindow* window = nullptr;
static GLuint shaderProgram = 0;
static GLuint vao = 0;

static int fbWidth = 0, fbHeight = 0;
static double startTime = 0.0;

// ---------- Utilities ----------
static std::string loadTextFile(const char* path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open file: " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint sh = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
        std::string log((size_t)logLen, '\0');
        glGetShaderInfoLog(sh, logLen, nullptr, log.data());
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return sh;
}

static GLuint createProgramFromFiles(const char* vertPath, const char* fragPath) {
    std::string vsrc = loadTextFile(vertPath);
    std::string fsrc = loadTextFile(fragPath);
    if (vsrc.empty() || fsrc.empty()) return 0;

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vsrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        std::string log((size_t)logLen, '\0');
        glGetProgramInfoLog(prog, logLen, nullptr, log.data());
        std::cerr << "Program link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------- Callbacks ----------
static void errorCallback(int code, const char* desc) {
    std::cerr << "GLFW error " << code << ": " << desc << "\n";
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, GLFW_TRUE);
}

static void framebufferSizeCallback(GLFWwindow* /*w*/, int width, int height) {
    fbWidth = width;
    fbHeight = height;
    glViewport(0, 0, fbWidth, fbHeight);
}

// ---------- Init / Shutdown ----------
static bool init() {
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    // Fullscreen on primary monitor
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    window = glfwCreateWindow(mode->width, mode->height, "Fullscreen Shader", mon, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // GLAD (GLAD1-style loader)
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD2\n";
        return false;
    }


    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Minimal state
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);

    // Program
    shaderProgram = createProgramFromFiles("shaders/fullscreen.vert", "shaders/fullscreen.frag");
    if (!shaderProgram) return false;

    // Empty VAO required for core profile when using gl_VertexID
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Cursor hidden for screensaver feel
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    startTime = glfwGetTime();
    return true;
}

static void shutdown() {
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
}

float smoothstepCubed(float a, float b, float t)  {
    float v = -2*t*t*t + 3*t*t;
    return glm::mix(a, b, v);
}

float randomValue(glm::uint& state){
    state = state * 747796405u + 2891336453u;
    glm::uint result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return float(result) * (1/4294967295.0);
}

float perlinNoise(glm::vec3 pos, int scale, glm::uvec3 res, glm::uint state){
    glm::ivec3 base = glm::ivec3(pos/float(scale))*scale;
    glm::vec3 frac = (pos - glm::vec3(base))/float(scale);

    float mags[8] = {0,0,0,0,0,0,0,0};

    for (int z = 0; z <= scale; z += scale){
        for (int y = 0; y <= scale; y += scale){
            for (int x = 0; x <= scale; x += scale){
                int index = z/scale*4 + y/scale*2 + x/scale;
                glm::uvec3 corner(base.x+x, base.y+y, base.z+z);
                glm::uvec3 stateCorner(
                    corner.x < res.x ? corner.x : corner.x-res.x,
                    corner.y < res.y ? corner.y : corner.y-res.y,
                    corner.z < res.z ? corner.z : corner.z-res.z
                );
                glm::uint stateTemp = glm::uint(stateCorner.z + stateCorner.y*res.x + stateCorner.x) + state;
                glm::vec3 vector = glm::normalize(glm::vec3(2*randomValue(stateTemp)-1, 2*randomValue(stateTemp)-1, 2*randomValue(stateTemp)-1));
                float mag = glm::dot(vector, (glm::vec3(corner)-pos)/float(scale));
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

void updateNoiseMap(std::vector<glm::vec4>& noiseMap, const int scale, const float speed, const glm::uvec2 resolution, const float iTime) {
    noiseMap.clear();
    for (int y = 0; y < resolution.y; y += scale) {
        for (int x = 0; x < resolution.x; x += scale) {
            glm::ivec2 a = glm::ivec2(x, y);

            glm::vec4 noise = glm::vec4(
                perlinNoise(glm::vec3(a, int(iTime*speed)), 800, glm::uvec3(resolution, 252000), 0u),
                perlinNoise(glm::vec3(a, int(iTime*speed)), 700, glm::uvec3(resolution, 252000), 1u),
                perlinNoise(glm::vec3(a, int(iTime*speed)), 1000, glm::uvec3(resolution, 252000), 2u),
                perlinNoise(glm::vec3(a, int(iTime*speed)), 900, glm::uvec3(resolution, 252000), 3u)
                );

            noiseMap.push_back(noise);
        }
    }
}

// ---------- Main ----------
int main() {
    if (!init()) return -1;

    // Cache uniform locations
    const GLint uResolution = glGetUniformLocation(shaderProgram, "resolution");
    const GLint uTime       = glGetUniformLocation(shaderProgram, "iTime");

    int scale = 40;
    float speed = 200;

    glm::uvec2 resolution = glm::uvec2(fbWidth, fbHeight);

    std::vector<glm::vec4> noiseMap;
    noiseMap.resize(resolution.x*resolution.y/scale/scale);

    GLuint ssboNoiseMap;
    glGenBuffers(1, &ssboNoiseMap);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNoiseMap);
    glBufferData(GL_SHADER_STORAGE_BUFFER, int(noiseMap.size() * sizeof(glm::vec4)), noiseMap.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboNoiseMap);

    while (!glfwWindowShouldClose(window)) {
        // Update time
        const float t = static_cast<float>(glfwGetTime() - startTime);

        glClear(GL_COLOR_BUFFER_BIT);

        updateNoiseMap(noiseMap, scale, speed, resolution, t);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboNoiseMap);
        void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec4)*noiseMap.size(),GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        memcpy(ptr, noiseMap.data(), sizeof(glm::vec4)*noiseMap.size());
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glUseProgram(shaderProgram);
        glUniform2ui(uResolution, static_cast<float>(fbWidth), static_cast<float>(fbHeight));
        glUniform1f(uTime, t);

        // Draw 1 big triangle via gl_VertexID (0..2) — no VBO needed
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    shutdown();
    return 0;
}
