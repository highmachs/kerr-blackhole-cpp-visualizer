// Kerr Black Hole Interior Navigator — application entry point.
// Spec sections 1, 5, 7, 13 of kerrblackhole.md.

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "camera.h"
#include "metric.h"
#include "renderer.h"
#include "audio.h"

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <string>

namespace
{

    struct AppState
    {
        kerr::Camera camera;
        kerr::Renderer renderer;
        kerr::RenderSettings settings;
        kerr::Audio audio;
        bool showDebugPanel = false;
        bool mouseCaptured = true;
        double lastMouseX = 0, lastMouseY = 0;
        int fbWidth = 1920, fbHeight = 1080;
        bool vsync = true;
        kerr::UniverseMode audioPrevMode = kerr::UniverseMode::NORMAL;

        // WASDQE state accumulators.
        float fwd = 0, side = 0, vert = 0;
    };

    AppState *g_app = nullptr;

    void onFramebufferSize(GLFWwindow *, int w, int h)
    {
        if (w == 0 || h == 0)
            return;
        g_app->fbWidth = w;
        g_app->fbHeight = h;
        g_app->renderer.resize(w, h);
    }

    void onKey(GLFWwindow *win, int key, int /*sc*/, int action, int /*mods*/)
    {
        if (ImGui::GetIO().WantCaptureKeyboard)
            return;
        if (action == GLFW_PRESS)
        {
            switch (key)
            {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(win, GLFW_TRUE);
                break;
            case GLFW_KEY_R:
                g_app->camera.reset();
                break;
            case GLFW_KEY_T:
                g_app->settings.showGrid = !g_app->settings.showGrid;
                break;
            case GLFW_KEY_P:
                g_app->showDebugPanel = !g_app->showDebugPanel;
                g_app->settings.showTrail = !g_app->settings.showTrail;
                break;
            case GLFW_KEY_O:
                g_app->renderer.screenshot("screenshot.png");
                std::printf("[kerr] wrote screenshot.png\n");
                break;
            case GLFW_KEY_SPACE:
                g_app->camera.dr = -0.10;
                g_app->camera.L = 2.0;
                g_app->camera.Q = 0.0;
                break;
            case GLFW_KEY_TAB:
                g_app->mouseCaptured = !g_app->mouseCaptured;
                glfwSetInputMode(win, GLFW_CURSOR,
                                 g_app->mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                break;
            case GLFW_KEY_V:
                g_app->vsync = !g_app->vsync;
                glfwSwapInterval(g_app->vsync ? 1 : 0);
                std::printf("[kerr] vsync %s\n", g_app->vsync ? "ON" : "OFF");
                break;
            case GLFW_KEY_F11:
            {
                static bool full = false;
                full = !full;
                GLFWmonitor *m = glfwGetPrimaryMonitor();
                const GLFWvidmode *vm = glfwGetVideoMode(m);
                if (full)
                    glfwSetWindowMonitor(win, m, 0, 0, vm->width, vm->height, vm->refreshRate);
                else
                    glfwSetWindowMonitor(win, nullptr, 80, 60, 1280, 720, 0);
                break;
            }
            }
        }
    }

    void onCursor(GLFWwindow * /*win*/, double x, double y)
    {
        if (ImGui::GetIO().WantCaptureMouse)
        {
            g_app->lastMouseX = x;
            g_app->lastMouseY = y;
            return;
        }
        if (!g_app->mouseCaptured)
        {
            g_app->lastMouseX = x;
            g_app->lastMouseY = y;
            return;
        }
        double dx = x - g_app->lastMouseX, dy = y - g_app->lastMouseY;
        g_app->lastMouseX = x;
        g_app->lastMouseY = y;
        g_app->camera.applyMouse((float)dx, (float)-dy);
    }

    void pollInput(GLFWwindow *win, double dt)
    {
        auto down = [&](int k)
        { return glfwGetKey(win, k) == GLFW_PRESS; };
        g_app->fwd = (down(GLFW_KEY_W) ? 1.f : 0.f) - (down(GLFW_KEY_S) ? 1.f : 0.f);
        g_app->side = (down(GLFW_KEY_D) ? 1.f : 0.f) - (down(GLFW_KEY_A) ? 1.f : 0.f);
        g_app->vert = (down(GLFW_KEY_E) ? 1.f : 0.f) - (down(GLFW_KEY_Q) ? 1.f : 0.f);
        g_app->camera.applyInput(g_app->fwd, g_app->side, g_app->vert, dt);
    }

    void drawImGui(double frameMs)
    {
        if (!g_app->showDebugPanel)
            return;
        auto &cam = g_app->camera;
        auto &set = g_app->settings;

        ImGui::Begin("Kerr Navigator — debug");
        ImGui::Text("Mode: %s", kerr::modeName(cam.mode));
        ImGui::Text("r = %.4f   θ = %.3f   φ = %.3f", cam.r, cam.theta, cam.phi);
        ImGui::Text("dr = %.4f  dφ = %.4f", cam.dr, cam.dphi);
        ImGui::Text("E = %.3f  L = %.3f  Q = %.3f", cam.E, cam.L, cam.Q);
        ImGui::Text("r_outer = %.3f   r_inner = %.3f", kerr::rOuterHorizon(cam.bh), kerr::rInnerHorizon(cam.bh));
        ImGui::Text("ISCO    = %.3f   r_photon = %.3f", kerr::rISCO(cam.bh), kerr::rPhotonSphere(cam.bh));
        ImGui::Text("Steps/ray: %d   Frame: %.2f ms (%.0f FPS)",
                    g_app->renderer.lastSteps(), frameMs, 1000.0 / std::max(0.001, frameMs));
        ImGui::Separator();
        float a = (float)cam.bh.a;
        if (ImGui::SliderFloat("Spin a", &a, 0.0f, 0.99f))
        {
            cam.bh.a = a;
            cam.reset();
        }
        if (ImGui::Button("Screenshot (O)"))
            g_app->renderer.screenshot("screenshot.png");
        ImGui::SameLine();
        if (ImGui::Button("Reset (R)"))
            cam.reset();
        ImGui::End();

        ImGui::Begin("Render settings");
        ImGui::SliderInt("Max steps / ray", &set.maxSteps, 50, 800);
        ImGui::SliderFloat("Disk brightness", &set.diskBrightness, 0.0f, 2.0f);
        ImGui::SliderFloat("Starfield brightness", &set.starBrightness, 0.0f, 1.5f);
        ImGui::SliderFloat("Bloom", &set.bloomIntensity, 0.0f, 1.0f);
        ImGui::Checkbox("Show grid", &set.showGrid);
        ImGui::Checkbox("Show geodesic trail", &set.showTrail);
        ImGui::Checkbox("Show photon sphere", &set.showPhotonSphere);
        ImGui::Separator();
        if (ImGui::Button("Quality: Cinematic (400 steps)"))
            set.maxSteps = 400;
        ImGui::SameLine();
        if (ImGui::Button("Balanced (200)"))
            set.maxSteps = 200;
        ImGui::SameLine();
        if (ImGui::Button("Fast (120)"))
            set.maxSteps = 120;
        ImGui::Text("V = toggle vsync, F11 = fullscreen");
        ImGui::End();
    }

    void GLAPIENTRY glDebug(GLenum, GLenum, GLuint, GLenum sev,
                            GLsizei, const GLchar *msg, const void *)
    {
        if (sev == 0x826B /* GL_DEBUG_SEVERITY_NOTIFICATION */)
            return;
        std::fprintf(stderr, "[GL] %s\n", msg);
    }

} // namespace

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // 1080p default; resizable, sRGB framebuffer for clean tone-mapping.
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0); // compute-shader output, no MSAA needed
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    GLFWwindow *win = glfwCreateWindow(1280, 720,
                                       "Kerr Black Hole Interior Navigator", nullptr, nullptr);
    if (!win)
    {
        std::fprintf(stderr,
                     "glfwCreateWindow failed (need GL 4.6; macOS caps at 4.1)\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // vsync on by default

    if (!kerrLoadGL((KerrGLLoaderFn)glfwGetProcAddress))
    {
        std::fprintf(stderr, "GL function load failed\n");
        return 1;
    }
    std::printf("[kerr] GL %s\n", glGetString(0x1F02 /* GL_VERSION */));

#ifndef NDEBUG
    if (glDebugMessageCallback)
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback((void *)glDebug, nullptr);
    }
#endif

    AppState app;
    g_app = &app;

    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    app.fbWidth = fbw;
    app.fbHeight = fbh;
    if (!app.renderer.init(fbw, fbh))
    {
        std::fprintf(stderr, "renderer init failed (shaders missing?)\n");
        return 1;
    }
    app.camera.reset();
    app.audio.init();
    app.audioPrevMode = app.camera.mode;

    glfwSetFramebufferSizeCallback(win, onFramebufferSize);
    glfwSetKeyCallback(win, onKey);
    glfwSetCursorPosCallback(win, onCursor);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(win, &app.lastMouseX, &app.lastMouseY);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    using clock = std::chrono::high_resolution_clock;
    auto last = clock::now();
    auto start = last;

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        auto now = clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        double tsec = std::chrono::duration<double>(now - start).count();
        last = now;

        pollInput(win, dt);
        app.camera.step(dt);

        // Audio: trigger on universe-mode transitions.
        if (app.camera.mode != app.audioPrevMode)
        {
            using M = kerr::UniverseMode;
            if (app.audioPrevMode == M::NORMAL && app.camera.mode == M::BETWEEN)
                app.audio.trigger(kerr::AudioEvent::OUTER_HORIZON);
            if (app.audioPrevMode == M::BETWEEN && app.camera.mode == M::INSIDE)
                app.audio.trigger(kerr::AudioEvent::INNER_HORIZON);
            if (app.audioPrevMode == M::INSIDE && app.camera.mode == M::NEGATIVE)
            {
                app.audio.trigger(kerr::AudioEvent::RING_SINGULAR);
                app.audio.trigger(kerr::AudioEvent::NEGATIVE_AMBI);
            }
            app.audioPrevMode = app.camera.mode;
        }

        app.renderer.render(app.camera, app.settings, (float)tsec);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawImGui(dt * 1000.0);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    app.audio.shutdown();
    app.renderer.shutdown();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
