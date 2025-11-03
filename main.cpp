#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
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

    // OpenGL 3.3 core for macOS
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif
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

// ---------- Main ----------
int main() {
    if (!init()) return -1;

    glm::vec2 testVec = glm::vec2(1, 0);

    // Cache uniform locations
    const GLint uResolution = glGetUniformLocation(shaderProgram, "resolution");
    const GLint uTestVec = glGetUniformLocation(shaderProgram, "testVec");
    const GLint uTime       = glGetUniformLocation(shaderProgram, "iTime");

    while (!glfwWindowShouldClose(window)) {
        // Update time
        const float t = static_cast<float>(glfwGetTime() - startTime);
        testVec = glm::vec2(cos(t), sin(t));

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform2ui(uResolution, static_cast<float>(fbWidth), static_cast<float>(fbHeight));
        glUniform2f(uTestVec, testVec.x, testVec.y);
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
