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

// Windows high-performance GPU exports (forces RTX 4060 on dual-GPU laptops)
#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace
{

    struct ResPresetItem
    {
        const char *name;
        int w;
        int h;
    };

    static const ResPresetItem kResPresets[] = {
        {"360p  (640x360) - Low GPU / Laptop",    640,  360},
        {"720p  (1280x720) - Fast / 120+ FPS",   1280,  720},
        {"1080p (1920x1080) - Full HD Balanced",  1920, 1080},
        {"1440p (2560x1440) - 2K High Fidelity",   2560, 1440},
        {"4K    (3840x2160) - Ultra HD Native",    3840, 2160},
        {"8K    (7680x4320) - Extreme Stress",     7680, 4320},
        {"Match Window (1:1 Native Canvas)",         0,    0}
    };

    struct AppState
    {
        kerr::Camera camera;
        kerr::Renderer renderer;
        kerr::RenderSettings settings;
        kerr::Audio audio;
        bool showDebugPanel = true;
        bool mouseCaptured = true;
        double lastMouseX = 0, lastMouseY = 0;
        int fbWidth = 1920, fbHeight = 1080;
        int renderWidth = 1920, renderHeight = 1080;
        int resPreset = 2; // Default 1080p balanced
        float renderScale = 1.0f; // 0.25x to 2.0x
        bool autoGpuRecovery = true;
        float frameMsSmoothed = 16.6f;
        float lowFpsDuration = 0.0f;
        bool gpuLagAlert = false;
        float uiScale = 1.0f; // High-DPI UI scaling factor (2.0x on 4K)
        bool vsync = true;
        kerr::UniverseMode audioPrevMode = kerr::UniverseMode::NORMAL;

        // WASDQE state accumulators.
        float fwd = 0, side = 0, vert = 0;
    };

    AppState *g_app = nullptr;

    void applyResolutionPreset(int presetIdx, float scale = 1.0f)
    {
        if (!g_app)
            return;
        g_app->resPreset = std::clamp(presetIdx, 0, 6);
        g_app->renderScale = std::clamp(scale, 0.25f, 2.0f);
        int baseW = (g_app->resPreset == 6 || kResPresets[g_app->resPreset].w <= 0) ? g_app->fbWidth : kResPresets[g_app->resPreset].w;
        int baseH = (g_app->resPreset == 6 || kResPresets[g_app->resPreset].h <= 0) ? g_app->fbHeight : kResPresets[g_app->resPreset].h;
        int targetW = std::clamp((int)(baseW * g_app->renderScale), 320, 7680);
        int targetH = std::clamp((int)(baseH * g_app->renderScale), 180, 4320);
        g_app->renderWidth = targetW;
        g_app->renderHeight = targetH;
        g_app->renderer.resize(targetW, targetH);
        std::printf("[kerr] Resolution switched to %dx%d (Preset %d '%s', Scale %.2fx)\n",
                    targetW, targetH, g_app->resPreset, kResPresets[g_app->resPreset].name, g_app->renderScale);
    }

    void onFramebufferSize(GLFWwindow *, int w, int h)
    {
        if (w == 0 || h == 0)
            return;
        g_app->fbWidth = w;
        g_app->fbHeight = h;
        if (g_app->resPreset == 6)
        {
            applyResolutionPreset(6, g_app->renderScale);
        }
    }

    void jumpToPhase(int phaseIndex)
    {
        if (!g_app)
            return;
        auto &cam = g_app->camera;
        cam.isAutoOrbit = false;
        cam.isTouring = false;
        cam.trail.clear();
        cam.velocity = glm::vec3(0.0f);
        switch (phaseIndex)
        {
        case 1: // Approach (14M)
            cam.r = 14.0; cam.theta = 1.35; cam.phi = 0.0; cam.dr = -0.15;
            cam.mode = kerr::classify(cam.bh, cam.r);
            break;
        case 2: // Outer Horizon (2.0M)
            cam.r = 2.0; cam.theta = 1.42; cam.dr = -0.15;
            cam.mode = kerr::classify(cam.bh, cam.r);
            break;
        case 3: // Between Horizons (1.0M)
            cam.r = 1.0; cam.theta = 1.45; cam.dr = -0.12;
            cam.mode = kerr::classify(cam.bh, cam.r);
            break;
        case 4: // Cauchy Crossing (0.68M)
            cam.r = 0.68; cam.theta = 1.50; cam.dr = -0.10;
            cam.mode = kerr::classify(cam.bh, cam.r);
            cam.whiteoutTimer = 0.25f;
            break;
        case 5: // Ring Singularity (0.05M)
            cam.r = 0.05; cam.theta = 1.57; cam.dr = -0.08;
            cam.mode = kerr::classify(cam.bh, cam.r);
            break;
        case 6: // Negative Universe (-2.0M)
            cam.r = -2.0; cam.theta = 1.42; cam.dr = -0.15;
            cam.mode = kerr::classify(cam.bh, cam.r);
            cam.yaw = 0.0f;
            cam.pitch = 0.0f;
            cam.roll = 0.0f;
            cam.lookDir = glm::vec3(0, 0, -1);
            break;
        }
        std::printf("[kerr] Teleported to Phase %d: %s (r=%.2f)\n",
                    phaseIndex, kerr::modeName(cam.mode), cam.r);
    }

    void onKey(GLFWwindow *win, int key, int /*sc*/, int action, int /*mods*/)
    {
        if (action != GLFW_PRESS)
            return;

        // Smart ESC: if mouse is captured, release cursor; if already released, close window
        if (key == GLFW_KEY_ESCAPE)
        {
            if (g_app->mouseCaptured)
            {
                g_app->mouseCaptured = false;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else
            {
                glfwSetWindowShouldClose(win, GLFW_TRUE);
            }
            return;
        }

        // Mouse capture toggle
        if (key == GLFW_KEY_TAB)
        {
            g_app->mouseCaptured = !g_app->mouseCaptured;
            glfwSetInputMode(win, GLFW_CURSOR,
                             g_app->mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            return;
        }

        // Safe borderless windowed fullscreen toggle
        if (key == GLFW_KEY_F11)
        {
            static bool isFullscreen = false;
            static int savedX = 100, savedY = 100;
            static int savedW = 1280, savedH = 720;
            isFullscreen = !isFullscreen;
            if (isFullscreen)
            {
                glfwGetWindowPos(win, &savedX, &savedY);
                glfwGetWindowSize(win, &savedW, &savedH);
                GLFWmonitor *m = glfwGetPrimaryMonitor();
                int mx = 0, my = 0;
                if (m)
                    glfwGetMonitorPos(m, &mx, &my);
                const GLFWvidmode *vm = m ? glfwGetVideoMode(m) : nullptr;
                if (vm)
                {
                    glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_FALSE);
                    glfwSetWindowPos(win, mx, my);
                    glfwSetWindowSize(win, vm->width, vm->height);
                }
            }
            else
            {
                glfwSetWindowAttrib(win, GLFW_DECORATED, GLFW_TRUE);
                glfwSetWindowPos(win, savedX, savedY);
                glfwSetWindowSize(win, savedW, savedH);
            }
            return;
        }

        // Instant phase teleport shortcuts: keys 1 to 6 (main keyboard and numpad)
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6)
        {
            jumpToPhase(key - GLFW_KEY_1 + 1);
            return;
        }
        if (key >= GLFW_KEY_KP_1 && key <= GLFW_KEY_KP_6)
        {
            jumpToPhase(key - GLFW_KEY_KP_1 + 1);
            return;
        }

        if (ImGui::GetIO().WantCaptureKeyboard)
            return;

        switch (key)
        {
        case GLFW_KEY_R:
            g_app->camera.reset();
            break;
        case GLFW_KEY_T:
            g_app->settings.showGrid = !g_app->settings.showGrid;
            break;
        case GLFW_KEY_P:
        case GLFW_KEY_H:
            g_app->showDebugPanel = !g_app->showDebugPanel;
            g_app->settings.showTrail = !g_app->settings.showTrail;
            break;
        case GLFW_KEY_O:
            g_app->renderer.screenshot("screenshot.png");
            std::printf("[kerr] wrote screenshot.png\n");
            break;
        case GLFW_KEY_SPACE:
            // Fall-through handled in pollInput for continuous 3D vertical thrust
            break;
        case GLFW_KEY_V:
            g_app->vsync = !g_app->vsync;
            glfwSwapInterval(g_app->vsync ? 1 : 0);
            std::printf("[kerr] vsync %s\n", g_app->vsync ? "ON" : "OFF");
            break;
        case GLFW_KEY_X:
            g_app->camera.toggleAutoOrbit();
            std::printf("[kerr] Auto-Orbit: %s\n", g_app->camera.isAutoOrbit ? "ON" : "OFF");
            break;
        case GLFW_KEY_G:
            g_app->camera.toggleTour();
            std::printf("[kerr] Cinematic Tour: %s\n", g_app->camera.isTouring ? "ACTIVE" : "STOPPED");
            break;
        case GLFW_KEY_K:
            g_app->settings.diskStyle = (g_app->settings.diskStyle + 1) % 4;
            std::printf("[kerr] Disk Style: %d\n", g_app->settings.diskStyle);
            break;
        case GLFW_KEY_LEFT_BRACKET:
            g_app->camera.simSpeed = std::max(0.1f, g_app->camera.simSpeed - 0.25f);
            std::printf("[kerr] Sim Speed: %.2fx\n", g_app->camera.simSpeed);
            break;
        case GLFW_KEY_RIGHT_BRACKET:
            g_app->camera.simSpeed = std::min(5.0f, g_app->camera.simSpeed + 0.25f);
            std::printf("[kerr] Sim Speed: %.2fx\n", g_app->camera.simSpeed);
            break;
        case GLFW_KEY_BACKSLASH:
            g_app->camera.simSpeed = 1.0f;
            std::printf("[kerr] Sim Speed: 1.00x\n");
            break;
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

    void onMouseButton(GLFWwindow *win, int button, int action, int /*mods*/)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            if (!ImGui::GetIO().WantCaptureMouse && !g_app->mouseCaptured)
            {
                g_app->mouseCaptured = true;
                glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                double x, y;
                glfwGetCursorPos(win, &x, &y);
                g_app->lastMouseX = x;
                g_app->lastMouseY = y;
            }
        }
    }

    void pollInput(GLFWwindow *win, double dt)
    {
        auto down = [&](int k)
        { return glfwGetKey(win, k) == GLFW_PRESS; };

        // 6-DOF Roll controls: Z (roll left) and C (roll right)
        float rollInput = (down(GLFW_KEY_C) ? 1.f : 0.f) - (down(GLFW_KEY_Z) ? 1.f : 0.f);

        // Variable Thruster: Shift = Turbo 3.0x
        bool turbo = down(GLFW_KEY_LEFT_SHIFT) || down(GLFW_KEY_RIGHT_SHIFT);
        g_app->camera.thrustMultiplier = turbo ? 3.0f : 1.0f;

        // Intuitive 3D flight inputs:
        // W = forward along crosshair, S = backward
        // A / D = strafe left / right
        // Space OR E = fly Up, Left-Ctrl OR Q = fly Down
        g_app->fwd  = (down(GLFW_KEY_W) ? 1.f : 0.f) - (down(GLFW_KEY_S) ? 1.f : 0.f);
        g_app->side = (down(GLFW_KEY_D) ? 1.f : 0.f) - (down(GLFW_KEY_A) ? 1.f : 0.f);

        float upVal   = (down(GLFW_KEY_SPACE) || down(GLFW_KEY_E)) ? 1.0f : 0.0f;
        float downVal = (down(GLFW_KEY_LEFT_CONTROL) || down(GLFW_KEY_RIGHT_CONTROL) || down(GLFW_KEY_Q)) ? 1.0f : 0.0f;
        g_app->vert = upVal - downVal;

        g_app->camera.applyInput(g_app->fwd, g_app->side, g_app->vert, rollInput, dt);
    }

    void drawImGui(double frameMs)
    {
        if (!g_app->showDebugPanel)
            return;

        float scale = g_app->uiScale;
        ImGui::GetStyle().FontScaleMain = scale;

        // --- Minimal Top-Left Quick Info Banner (Clean, no overlap) ---
        ImGui::SetNextWindowPos(ImVec2(20.0f * scale, 20.0f * scale), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.70f);
        ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##ControlsHUD", nullptr, hudFlags))
        {
            ImGui::SetWindowFontScale(scale);
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f), "FLIGHT:");
            ImGui::SameLine();
            ImGui::Text("WASD = fly | Space/E = up | Ctrl/Q = down | Shift = turbo | TAB = mouse | P or H = hide UI");
        }
        ImGui::End();

        auto &cam = g_app->camera;
        auto &set = g_app->settings;

        // --- Single Unified Sidebar (Right side, scaled for 4K / High-DPI) ---
        ImGuiIO &io = ImGui::GetIO();
        float sideWidth = 460.0f * scale; // 920px at 4K (2.0x scale)
        float screenW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : (float)g_app->fbWidth;
        float screenH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : (float)g_app->fbHeight;
        ImGui::SetNextWindowPos(ImVec2(screenW - sideWidth - 20.0f * scale, 20.0f * scale), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sideWidth, screenH - 40.0f * scale), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGui::Begin("Kerr Navigator - Control Deck", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::SetWindowFontScale(scale);

        if (ImGui::BeginTabBar("ControlDeckTabs"))
        {
            // === TAB 1: TELEMETRY ===
            if (ImGui::BeginTabItem("Telemetry"))
            {
                double r = cam.r, a = cam.bh.a, M = cam.bh.M;
                double cosT = std::cos(cam.theta);
                double sigma = r * r + a * a * cosT * cosT + 0.05;
                double r2pa2 = r * r + a * a;
                double omega = (2.0 * M * a * r) / (r2pa2 * r2pa2 + 0.1);
                double vDrag = std::clamp(std::abs(omega * std::sqrt(r2pa2) * std::sin(cam.theta)), 0.0, 0.999);
                float tidal = (float)(2.0 * M / std::pow(std::abs(r) + 0.1, 3.0));

                const char *zoneTitle = "SAFE HIGH ORBIT";
                ImVec4 zoneCol = ImVec4(0.2f, 0.9f, 0.4f, 1.0f);
                const char *zoneLore = "Stable Keplerian domain. Spacetime curvature is mild; orbital paths remain stable.";

                if (r < 0.0) {
                    zoneTitle = "NEGATIVE UNIVERSE (r < 0)";
                    zoneCol = ImVec4(0.2f, 0.85f, 1.0f, 1.0f);
                    zoneLore = "Antigravity domain. Gravitational mass effectively inverts; the singularity pushes you away!";
                } else if (r <= 0.1) {
                    zoneTitle = "RING SINGULARITY THROAT";
                    zoneCol = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
                    zoneLore = "The center is an open spinning ring. Passing through the central aperture opens into r < 0.";
                } else if (r <= 0.75) {
                    zoneTitle = "INNER CAUCHY HORIZON FLASH";
                    zoneCol = ImVec4(0.85f, 0.45f, 1.0f, 1.0f);
                    zoneLore = "Infinite blueshift barrier. Incoming starlight from the external universe piles up into a flash.";
                } else if (r <= kerr::rOuterHorizon(cam.bh)) {
                    zoneTitle = "INSIDE EVENT HORIZON (NO RETURN)";
                    zoneCol = ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
                    zoneLore = "Space and time swap roles. All future-directed timelike paths lead inexorably inward.";
                } else if (r <= 6.0 && (r < 1.0 + std::sqrt(std::max(0.0, 1.0 - a * a * cosT * cosT)))) {
                    zoneTitle = "ERGOSPHERE (STATIC LIMIT)";
                    zoneCol = ImVec4(1.0f, 0.65f, 0.15f, 1.0f);
                    zoneLore = "Frame-dragging forces spacetime to rotate faster than light. Standing still is impossible.";
                }

                ImGui::Spacing();
                ImGui::TextColored(zoneCol, "ZONE: [%s]", zoneTitle);
                ImGui::TextWrapped("%s", zoneLore);
                ImGui::Separator();

                // Telemetry Metrics Grid (Zero layout shift, clean columns)
                if (ImGui::BeginTable("##TelemetryTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableSetupColumn("Value",  ImGuiTableColumnFlags_WidthStretch, 0.54f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Vortex Drag:");
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f c", vDrag);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Time Dilation:");
                    ImGui::TableSetColumnIndex(1);
                    if (r > 2.0 * M) {
                        double dFact = 1.0 - (2.0 * M * r) / sigma;
                        double dilation = (dFact > 0.0001) ? (1.0 / std::sqrt(dFact)) : 999.0;
                        if (dilation < 60.0)
                            ImGui::Text("1s = %.1fs Earth", dilation);
                        else if (dilation < 3600.0)
                            ImGui::Text("1s = %.1f min Earth", dilation / 60.0);
                        else
                            ImGui::Text("1s = %.1f hrs Earth", dilation / 3600.0);
                    } else if (r > 0.0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[INVERTED]");
                    } else {
                        ImGui::TextColored(ImVec4(0.2f, 0.85f, 1.0f, 1.0f), "[REPULSIVE]");
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Tidal Shear:");
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f G/m", tidal);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Hull Integrity:");
                    ImGui::TableSetColumnIndex(1);
                    if (tidal < 0.1f)
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "100%% (Nominal)");
                    else if (tidal < 1.0f)
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "CAUTION (High Shear)");
                    else
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "DANGER (Spaghetti)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Radius / Theta:");
                    ImGui::TableSetColumnIndex(1); ImGui::Text("r = %.2f M | th = %.2f", cam.r, cam.theta);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Ray Pacing:");
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d stp | %.1fms (%.0f fps)",
                                                              g_app->renderer.lastSteps(), frameMs, 1000.0 / std::max(0.001, frameMs));

                    ImGui::EndTable();
                }

                ImGui::Separator();
                ImGui::Text("Autopilot Systems:");
                float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

                if (cam.isAutoOrbit) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
                    if (ImGui::Button("ORBIT LOCKED (X)", ImVec2(btnW, 0))) cam.toggleAutoOrbit();
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button("Lock Orbit (X)", ImVec2(btnW, 0))) cam.toggleAutoOrbit();
                }

                ImGui::SameLine();
                if (cam.isTouring) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.8f, 1.0f));
                    if (ImGui::Button("STOP TOUR (G)", ImVec2(btnW, 0))) cam.toggleTour();
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::Button("Cinematic Tour (G)", ImVec2(btnW, 0))) cam.toggleTour();
                }

                if (cam.isTouring) {
                    ImGui::Spacing();
                    ImGui::ProgressBar(std::min(1.0f, cam.tourTime / 30.0f), ImVec2(-1, 0), "Cruising to Negative Universe");
                }

                ImGui::Spacing();
                ImGui::SliderFloat("Sim Speed", &cam.simSpeed, 0.1f, 5.0f, "%.2fx");
                ImGui::EndTabItem();
            }

            // === TAB 2: TELEPORT & PHASES ===
            if (ImGui::BeginTabItem("Phases"))
            {
                ImGui::Spacing();
                ImGui::Text("Click or press 1 to 6 on keyboard:");
                ImGui::Spacing();

                float phaseBtnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ImGui::Button("1. Approach (14M)", ImVec2(phaseBtnW, 0))) jumpToPhase(1);
                ImGui::SameLine();
                if (ImGui::Button("2. Outer Horizon (2M)", ImVec2(phaseBtnW, 0))) jumpToPhase(2);

                if (ImGui::Button("3. Mid-Horizon (1M)", ImVec2(phaseBtnW, 0))) jumpToPhase(3);
                ImGui::SameLine();
                if (ImGui::Button("4. Cauchy Flash (0.68M)", ImVec2(phaseBtnW, 0))) jumpToPhase(4);

                if (ImGui::Button("5. Ring Throat (0.05M)", ImVec2(phaseBtnW, 0))) jumpToPhase(5);
                ImGui::SameLine();
                if (ImGui::Button("6. Negative Univ (-2M)", ImVec2(phaseBtnW, 0))) jumpToPhase(6);

                ImGui::Separator();
                float aSpin = (float)cam.bh.a;
                if (ImGui::SliderFloat("Spin a", &aSpin, 0.0f, 0.99f))
                {
                    cam.bh.a = aSpin;
                    cam.reset();
                }

                ImGui::Spacing();
                float actionBtnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ImGui::Button("Reset Orbit (R)", ImVec2(actionBtnW, 0))) cam.reset();
                ImGui::SameLine();
                if (ImGui::Button("Screenshot (O)", ImVec2(actionBtnW, 0))) g_app->renderer.screenshot("screenshot.png");
                ImGui::EndTabItem();
            }

            // === TAB 3: VISUAL SETTINGS ===
            if (ImGui::BeginTabItem("Visuals"))
            {
                ImGui::Spacing();
                ImGui::Text("Accretion Disk Style (Key K):");
                const char *diskStyles[] = {
                    "0: Classic Interstellar (Amber Gas + Sapphire)",
                    "1: High-Energy Sapphire Plasma (Electric Cyan)",
                    "2: Naked Gravitational Lens (No Disk)",
                    "3: Doppler Heatmap (Blueshift/Redshift)"
                };
                ImGui::Combo("##DiskStyle", &set.diskStyle, diskStyles, 4);

                ImGui::Separator();
                ImGui::Checkbox("Show Cockpit Flight Reticle", &set.showReticle);
                ImGui::SliderFloat("Disk Brightness", &set.diskBrightness, 0.0f, 2.0f);
                ImGui::SliderFloat("Starfield Brightness", &set.starBrightness, 0.0f, 1.5f);
                ImGui::SliderFloat("Bloom Intensity", &set.bloomIntensity, 0.0f, 1.0f);

                ImGui::Separator();
                ImGui::Checkbox("Show Coordinate Grid (T)", &set.showGrid);
                ImGui::Checkbox("Show Geodesic Trail", &set.showTrail);
                ImGui::Checkbox("Show Photon Sphere", &set.showPhotonSphere);

                ImGui::Separator();
                ImGui::Text("Quality / Performance Presets:");
                float thirdW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
                if (ImGui::Button("Cinematic (320)", ImVec2(thirdW, 0))) set.maxSteps = 320;
                ImGui::SameLine();
                if (ImGui::Button("Balanced (240)", ImVec2(thirdW, 0)))  set.maxSteps = 240;
                ImGui::SameLine();
                if (ImGui::Button("Fast (140)", ImVec2(thirdW, 0)))      set.maxSteps = 140;

                ImGui::SliderInt("Steps / Ray", &set.maxSteps, 30, 800);

                ImGui::Separator();
                ImGui::Text("Interface Scaling (Auto 4K DPI):");
                if (ImGui::SliderFloat("UI Scale", &g_app->uiScale, 1.0f, 3.0f, "%.2fx"))
                {
                    ImGui::GetStyle().FontScaleMain = g_app->uiScale;
                }
                ImGui::EndTabItem();
            }

            // === TAB 4: RESOLUTION & GPU PROTECTION ===
            if (ImGui::BeginTabItem("Display & GPU"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "DYNAMIC RENDER RESOLUTION");
                ImGui::TextWrapped("Change render canvas resolution on-the-fly. Hardware upscaling fills your window smoothly.");
                ImGui::Spacing();

                const char *resLabels[] = {
                    "360p  (640x360) - Low GPU / Laptop",
                    "720p  (1280x720) - Fast / 120+ FPS",
                    "1080p (1920x1080) - Full HD Balanced",
                    "1440p (2560x1440) - 2K High Fidelity",
                    "4K    (3840x2160) - Ultra HD Native",
                    "8K    (7680x4320) - Extreme Stress",
                    "Match Window (1:1 Native Canvas)"
                };

                int currentPreset = g_app->resPreset;
                if (ImGui::Combo("Resolution Preset", &currentPreset, resLabels, 7))
                {
                    applyResolutionPreset(currentPreset, g_app->renderScale);
                }

                if (ImGui::SliderFloat("Render Scale", &g_app->renderScale, 0.25f, 2.0f, "%.2fx"))
                {
                    applyResolutionPreset(g_app->resPreset, g_app->renderScale);
                }

                ImGui::Separator();
                ImGui::Text("GPU Telemetry & Display Pipeline:");
                ImGui::BulletText("Render Buffer: %d x %d (%.2f MP)",
                                  g_app->renderer.width(), g_app->renderer.height(),
                                  (float)(g_app->renderer.width() * g_app->renderer.height()) / 1000000.0f);
                ImGui::BulletText("Window Display: %d x %d", g_app->fbWidth, g_app->fbHeight);
                float upscaleRatio = (g_app->renderer.height() > 0) ? ((float)g_app->fbHeight / (float)g_app->renderer.height()) : 1.0f;
                ImGui::BulletText("Display Upscaling: %.2fx (Hardware Linear)", upscaleRatio);
                ImGui::BulletText("Smooth Frame Time: %.1f ms (~%.0f FPS)", g_app->frameMsSmoothed, 1000.0f / std::max(1.0f, g_app->frameMsSmoothed));

                ImGui::Separator();
                ImGui::Checkbox("Auto-Protect on GPU Lag (<11 FPS)", &g_app->autoGpuRecovery);
                if (ImGui::Checkbox("VSync (Limit to Monitor Hz)", &g_app->vsync))
                {
                    glfwSwapInterval(g_app->vsync ? 1 : 0);
                }

                ImGui::Spacing();
                ImGui::Text("Emergency Quick Recovery:");
                float halfBtnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                if (ImGui::Button("Quick Safe 1080p", ImVec2(halfBtnW, 0)))
                {
                    applyResolutionPreset(2, 1.0f);
                    g_app->gpuLagAlert = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Quick Light 720p", ImVec2(halfBtnW, 0)))
                {
                    applyResolutionPreset(1, 1.0f);
                    g_app->gpuLagAlert = false;
                }

                ImGui::EndTabItem();
            }

            // === TAB 5: FLIGHT MANUAL & HELP ===
            if (ImGui::BeginTabItem("Flight Manual"))
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "6-DOF FLIGHT CONTROLS GUIDE");
                ImGui::Separator();
                if (ImGui::BeginTable("##ControlsGuideTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn("Key / Input", ImGuiTableColumnFlags_WidthStretch, 0.36f);
                    ImGui::TableSetupColumn("Function & Details", ImGuiTableColumnFlags_WidthStretch, 0.64f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "W / S");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Thrust Forward / Backward along crosshair");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "A / D");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Strafe Lateral Left / Right");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Space / E");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Vertical Ascend (Fly Up)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Ctrl / Q");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Vertical Descend (Fly Down)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Z / C");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Roll Left / Roll Right (True 6-DOF)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Left Shift");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Turbo Thrusters (3.0x Velocity Boost)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "TAB / Right-Drag");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Toggle Free-Look Mouse Capture / HUD Cursor");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "P / H");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Toggle Cockpit HUD (Immersive Vista Mode)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "R");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Reset to Canonical Safe Orbit (r=14M, 77 deg)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "K");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Cycle Disk Style (Interstellar/Cyan/Naked/Doppler)");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "T");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Toggle Boyer-Lindquist Coordinate Grid");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Keys 1 to 6");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Instant Teleport to GR Mission Phases");

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "O");
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("Capture Pristine High-Res PNG Screenshot");

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "KERR BLACK HOLE PHYSICS EXPLAINED");
                ImGui::Separator();
                ImGui::BulletText("Phase 1 - Approach (r=14M): Distant Keplerian domain. Dual Einstein gravitational lensing rings visible around the spinning shadow.");
                ImGui::BulletText("Phase 2 - Ergosphere & Outer Horizon (r=2M): Spacetime itself is dragged faster than light. Standing still is impossible.");
                ImGui::BulletText("Phase 3 - Mid-Horizon (r=1M): Coordinates swap; timelike geodesics inexorably point towards the interior Cauchy horizon.");
                ImGui::BulletText("Phase 4 - Cauchy Horizon Flash (r=0.68M): Infinite blueshift surface. Photons from the external universe pile up into a blinding relativistic ring.");
                ImGui::BulletText("Phase 5 - Ring Singularity Throat (r=0.05M): Unlike Schwarzschild point singularities, Kerr rotation flattens the singularity into an open ring. Traversing the throat is mathematically permitted.");
                ImGui::BulletText("Phase 6 - Negative Universe (r < 0): Entering negative radial space inverts gravitational mass into repulsive antigravity. Spacetime acts as a white hole.");

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "SMOOTH PERFORMANCE & GPU SETUP");
                ImGui::Separator();
                ImGui::TextWrapped("The visualizer renders using real-time Runge-Kutta numerical geodesic integration. If your GPU drops below 11 FPS, the automatic protection banner will prompt you to switch to 1080p or 720p. You can freely change canvas resolution in the Display & GPU tab anytime.");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        // --- GPU Lag / Overload Emergency Alert Banner ---
        if (g_app->gpuLagAlert)
        {
            float alertW = std::min(screenW - 40.0f * scale, 750.0f * scale);
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f - alertW * 0.5f, 20.0f * scale), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(alertW, 0.0f), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.85f, 0.12f, 0.12f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            if (ImGui::Begin("##GPULagAlert", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove))
            {
                ImGui::SetWindowFontScale(scale);
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), "WARNING: SEVERE GPU OVERLOAD DETECTED!");
                ImGui::Text("Frame latency spiked to %.1f ms (<11 FPS). Raymarching at %dx%d exceeds GPU throughput.",
                            g_app->frameMsSmoothed, g_app->renderer.width(), g_app->renderer.height());
                ImGui::Spacing();
                float alertBtnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
                if (ImGui::Button("Downscale to 1080p", ImVec2(alertBtnW, 0)))
                {
                    applyResolutionPreset(2, 1.0f);
                    g_app->gpuLagAlert = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Downscale to 720p", ImVec2(alertBtnW, 0)))
                {
                    applyResolutionPreset(1, 1.0f);
                    g_app->gpuLagAlert = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("Dismiss Alert", ImVec2(alertBtnW, 0)))
                {
                    g_app->gpuLagAlert = false;
                }
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }
    }

    [[maybe_unused]]
    void GLAPIENTRY glDebug(GLenum, GLenum, GLuint, GLenum sev,
                            GLsizei, const GLchar *msg, const void *)
    {
        if (sev == 0x826B /* GL_DEBUG_SEVERITY_NOTIFICATION */)
            return;
        std::fprintf(stderr, "[GL] %s\n", msg);
    }

} // namespace

int main(int argc, char **argv)
{
    std::string screenshotPath = "";
    int targetFrames = 10;
    bool headless = false;
    double initialR = 14.0;
    double initialTheta = 1.35; // ~77 deg inclination (Interstellar-style vista)
    double initialPhi = 0.0;
    float initialSpin = 0.95f;
    int initialSteps = 240;
    int initialDiskStyle = 0;
    int winW = 3840, winH = 2160;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) screenshotPath = argv[++i];
        else if (arg == "--frames" && i + 1 < argc) targetFrames = std::atoi(argv[++i]);
        else if (arg == "--headless") headless = true;
        else if (arg == "--r" && i + 1 < argc) initialR = std::atof(argv[++i]);
        else if (arg == "--theta" && i + 1 < argc) initialTheta = std::atof(argv[++i]);
        else if (arg == "--phi" && i + 1 < argc) initialPhi = std::atof(argv[++i]);
        else if (arg == "--spin" && i + 1 < argc) initialSpin = (float)std::atof(argv[++i]);
        else if (arg == "--steps" && i + 1 < argc) initialSteps = std::atoi(argv[++i]);
        else if (arg == "--disk" && i + 1 < argc) initialDiskStyle = std::atoi(argv[++i]);
        else if (arg == "--width" && i + 1 < argc) winW = std::atoi(argv[++i]);
        else if (arg == "--height" && i + 1 < argc) winH = std::atoi(argv[++i]);
    }

    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    // Monitor resolution bounds check for crash-proof window creation
    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *vidMode = primaryMonitor ? glfwGetVideoMode(primaryMonitor) : nullptr;
    if (vidMode && !headless)
    {
        // Smoothly fit within monitor desktop while targeting maximum available resolution
        if (winW > vidMode->width || winH > vidMode->height)
        {
            winW = std::min(winW, vidMode->width);
            winH = std::min(winH, vidMode->height);
        }
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    if (headless)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    GLFWwindow *win = glfwCreateWindow(winW, winH,
                                       "Kerr Black Hole Interior Navigator", nullptr, nullptr);
    if (!win)
    {
        std::fprintf(stderr,
                     "glfwCreateWindow failed (need GL 4.6; macOS caps at 4.1)\n");
        glfwTerminate();
        return 1;
    }
    if (!headless)
    {
        glfwMaximizeWindow(win);
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(headless ? 0 : 1);

    if (!kerrLoadGL((KerrGLLoaderFn)glfwGetProcAddress))
    {
        std::fprintf(stderr, "GL function load failed\n");
        return 1;
    }
    std::printf("[kerr] GL %s | Vendor: %s | Renderer: %s\n",
                glGetString(0x1F02 /* GL_VERSION */),
                glGetString(0x1F00 /* GL_VENDOR */),
                glGetString(0x1F01 /* GL_RENDERER */));

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
    app.settings.maxSteps = initialSteps;
    app.settings.diskStyle = initialDiskStyle;

    int fbw, fbh;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    app.fbWidth = fbw;
    app.fbHeight = fbh;

    // Detect startup preset based on initial requested window dimension
    if (winW <= 720) app.resPreset = 0;       // 360p
    else if (winW <= 1366) app.resPreset = 1; // 720p
    else if (winW <= 2000) app.resPreset = 2; // 1080p
    else if (winW <= 2800) app.resPreset = 3; // 1440p
    else if (winW <= 4200) app.resPreset = 4; // 4K
    else if (winW <= 8000) app.resPreset = 5; // 8K
    else app.resPreset = 6;                   // Match window

    int initialRenderW = (app.resPreset == 6 || kResPresets[app.resPreset].w <= 0) ? fbw : kResPresets[app.resPreset].w;
    int initialRenderH = (app.resPreset == 6 || kResPresets[app.resPreset].h <= 0) ? fbh : kResPresets[app.resPreset].h;
    app.renderWidth = initialRenderW;
    app.renderHeight = initialRenderH;

    if (!app.renderer.init(initialRenderW, initialRenderH))
    {
        std::fprintf(stderr, "renderer init failed (shaders missing?)\n");
        return 1;
    }
    app.camera.reset();
    app.camera.bh.a = initialSpin;
    app.camera.r = initialR;
    app.camera.theta = initialTheta;
    app.camera.phi = initialPhi;
    app.camera.mode = kerr::classify(app.camera.bh, app.camera.r);
    app.audio.init();
    app.audioPrevMode = app.camera.mode;

    glfwSetFramebufferSizeCallback(win, onFramebufferSize);
    glfwSetKeyCallback(win, onKey);
    glfwSetCursorPosCallback(win, onCursor);
    glfwSetMouseButtonCallback(win, onMouseButton);
    if (!headless)
    {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(win, &app.lastMouseX, &app.lastMouseY);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    // Auto 4K DPI Scaling:
    // Automatically compute uiScale = max(1.0f, screenHeight / 1080.0f) (which is 2.0x at 4K).
    // Apply ImGui::GetStyle().ScaleAllSizes(uiScale) so buttons, sliders, padding, and tabs double in size.
    // Scale font to 26px (io.FontGlobalScale = uiScale) so text is large, bold, and readable.
    float autoScale = std::max(1.0f, (float)app.fbHeight / 1080.0f);
    app.uiScale = autoScale;
    ImGui::GetStyle().FontScaleMain = autoScale;
    ImGui::GetStyle().ScaleAllSizes(autoScale);

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    using clock = std::chrono::high_resolution_clock;
    auto last = clock::now();
    auto start = last;
    int frameCount = 0;

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        auto now = clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        double tsec = std::chrono::duration<double>(now - start).count();
        last = now;

        // Frame pacing safety clamp (max 50ms per frame to prevent physics hitches)
        dt = std::clamp(dt, 0.0001, 0.05);

        if (!headless)
        {
            pollInput(win, dt);
        }
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

        // GPU Latency & Overload Monitoring
        float currentFrameMs = (float)(dt * 1000.0);
        app.frameMsSmoothed = app.frameMsSmoothed * 0.92f + currentFrameMs * 0.08f;
        if (app.frameMsSmoothed > 90.0f && app.autoGpuRecovery && !headless)
        {
            app.lowFpsDuration += (float)dt;
            if (app.lowFpsDuration > 2.5f)
            {
                app.gpuLagAlert = true;
            }
        }
        else
        {
            app.lowFpsDuration = std::max(0.0f, app.lowFpsDuration - (float)dt * 0.5f);
        }

        app.renderer.render(app.camera, app.settings, (float)tsec, app.fbWidth, app.fbHeight);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawImGui(dt * 1000.0);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
        frameCount++;

        if (!screenshotPath.empty() && frameCount >= targetFrames)
        {
            app.renderer.screenshot(screenshotPath);
            std::printf("[kerr] benchmark saved %s at r=%.3f, mode=%s\n",
                        screenshotPath.c_str(), app.camera.r, kerr::modeName(app.camera.mode));
            break;
        }
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
