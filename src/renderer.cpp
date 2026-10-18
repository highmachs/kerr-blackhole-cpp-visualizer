#include "renderer.h"
#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cmath>

#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

#ifndef GL_LINE_STRIP
#define GL_LINE_STRIP 0x0003
#endif

namespace kerr
{

    bool Renderer::init(int width, int height)
    {
        W_ = width;
        H_ = height;

        glGenTextures(1, &image_);
        glBindTexture(GL_TEXTURE_2D, image_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, W_, H_);

        glGenVertexArrays(1, &vao_);

        // Support portable standalone distribution: check local "shaders" first, then fallback to SHADER_DIR
        std::string sd = "shaders";
        if (!std::filesystem::exists(sd + "/compute.glsl"))
        {
#ifdef SHADER_DIR
            sd = SHADER_DIR;
#endif
        }

        computeProg_ = loadComputeProgram(sd + "/compute.glsl");
        displayProg_ = loadGraphicsProgram(sd + "/fullscreen.vert",
                                           sd + "/display.frag");
        trailProg_ = loadGraphicsProgram(sd + "/trail.vert",
                                         sd + "/trail.frag");
        if (!computeProg_ || !displayProg_ || !trailProg_)
            return false;

        glGenVertexArrays(1, &trailVao_);
        glGenBuffers(1, &trailVbo_);
        glBindVertexArray(trailVao_);
        glBindBuffer(GL_ARRAY_BUFFER, trailVbo_);
        glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(float) * 3, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void *)0);
        glBindVertexArray(0);

        // Support portable standalone distribution: check local "assets" first, then fallback to ASSET_DIR
        std::string ad = "assets";
        if (!std::filesystem::exists(ad + "/starfield.png") && !std::filesystem::exists(ad + "/starfield.hdr"))
        {
#ifdef ASSET_DIR
            ad = ASSET_DIR;
#endif
        }

        // Try .png, then .hdr, then .jpg — first that exists wins.
        std::string base = ad + "/starfield";
        if (!loadStarfield(base + ".png"))
            if (!loadStarfield(base + ".hdr"))
                loadStarfield(base + ".jpg"); // last resort -> procedural fallback
        return true;
    }

    void Renderer::resize(int width, int height)
    {
        if (width == W_ && height == H_)
            return;
        W_ = width;
        H_ = height;
        glDeleteTextures(1, &image_);
        glGenTextures(1, &image_);
        glBindTexture(GL_TEXTURE_2D, image_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, W_, H_);
    }

    void Renderer::shutdown()
    {
        if (image_)
            glDeleteTextures(1, &image_);
        if (starfield_)
            glDeleteTextures(1, &starfield_);
        if (vao_)
            glDeleteVertexArrays(1, &vao_);
        if (trailVao_)
            glDeleteVertexArrays(1, &trailVao_);
        if (trailVbo_)
            glDeleteBuffers(1, &trailVbo_);
        if (computeProg_)
            glDeleteProgram(computeProg_);
        if (displayProg_)
            glDeleteProgram(displayProg_);
        if (trailProg_)
            glDeleteProgram(trailProg_);
        image_ = starfield_ = vao_ = trailVao_ = trailVbo_ = 0;
        computeProg_ = displayProg_ = trailProg_ = 0;
    }

    bool Renderer::loadStarfield(const std::string &path)
    {
        if (starfield_)
        {
            glDeleteTextures(1, &starfield_);
            starfield_ = 0;
        }
        int w, h, n;
        unsigned char *data = stbi_load(path.c_str(), &w, &h, &n, 3);
        if (!data)
        {
            starfieldIsFile_ = true;
            const int pw = 2048, ph = 1024;
            std::vector<unsigned char> pix(pw * ph * 3, 0);
            unsigned int seed = 0xC0FFEEu;
            auto rnd = [&]()
            { seed = seed * 1664525u + 1013904223u; return seed; };

            // 1. Subtle galactic plane nebula band (Warm Ash & Dried Clay)
            for (int y = 0; y < ph; ++y)
            {
                float ny = ((float)y / (float)ph - 0.5f) * 2.0f; // -1 to 1
                float band = std::exp(-ny * ny * 18.0f);
                for (int x = 0; x < pw; ++x)
                {
                    float nx = (float)x / (float)pw;
                    float wave = 0.5f + 0.5f * std::sin(nx * 12.566f + ny * 3.0f);
                    float dust = band * (0.04f + 0.05f * wave);
                    int idx = (y * pw + x) * 3;
                    pix[idx + 0] = (unsigned char)std::clamp(dust * 196.0f, 0.0f, 255.0f);
                    pix[idx + 1] = (unsigned char)std::clamp(dust * 181.0f, 0.0f, 255.0f);
                    pix[idx + 2] = (unsigned char)std::clamp(dust * 165.0f, 0.0f, 255.0f);
                }
            }

            // 2. Multi-magnitude stars with anti-aliased splats
            const int numStars = 5000;
            for (int i = 0; i < numStars; ++i)
            {
                int sx = rnd() % pw;
                int sy = rnd() % ph;
                unsigned int rval = rnd();
                float brightness = 140.0f + (rval & 0x7F);
                // 10% very bright stars with larger radius
                bool isMajor = (rval % 10 == 0);
                if (isMajor) brightness = 255.0f;

                int radius = isMajor ? 2 : 1;
                for (int dy = -radius; dy <= radius; ++dy)
                {
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        int px_ = (sx + dx + pw) % pw;
                        int py_ = std::clamp(sy + dy, 0, ph - 1);
                        float dist2 = (float)(dx * dx + dy * dy);
                        float falloff = std::exp(-dist2 / (isMajor ? 1.2f : 0.6f));
                        int idx = (py_ * pw + px_) * 3;
                        float val = brightness * falloff;
                        pix[idx + 0] = (unsigned char)std::min(255.0f, pix[idx + 0] + val);
                        pix[idx + 1] = (unsigned char)std::min(255.0f, pix[idx + 1] + val * 0.96f);
                        pix[idx + 2] = (unsigned char)std::min(255.0f, pix[idx + 2] + val * 0.90f);
                    }
                }
            }
            glGenTextures(1, &starfield_);
            glBindTexture(GL_TEXTURE_2D, starfield_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pw, ph, 0, GL_RGB, GL_UNSIGNED_BYTE, pix.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            return false;
        }
        starfieldIsFile_ = true;
        glGenTextures(1, &starfield_);
        glBindTexture(GL_TEXTURE_2D, starfield_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);
        return true;
    }

    void Renderer::render(const Camera &cam, const RenderSettings &s, float timeSec, int displayWidth, int displayHeight)
    {
        // 1. Compute pass.
        glUseProgram(computeProg_);
        glBindImageTexture(0, image_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        auto uf = [&](const char *n, float v)
        { glUniform1f(glGetUniformLocation(computeProg_, n), v); };
        auto ui = [&](const char *n, int v)
        { glUniform1i(glGetUniformLocation(computeProg_, n), v); };
        auto um = [&](const char *n, const glm::mat4 &m)
        {
            glUniformMatrix4fv(glGetUniformLocation(computeProg_, n), 1, GL_FALSE, &m[0][0]);
        };

        ui("u_width", W_);
        ui("u_height", H_);
        ui("u_maxSteps", s.maxSteps);
        ui("u_mode", (int)cam.mode);
        ui("u_showPhotonSphere", s.showPhotonSphere ? 1 : 0);
        ui("u_showGrid", s.showGrid ? 1 : 0);
        ui("u_diskStyle", s.diskStyle);
        ui("u_showReticle", s.showReticle ? 1 : 0);
        uf("u_a", (float)cam.bh.a);
        uf("u_M", (float)cam.bh.M);
        uf("u_time", timeSec);
        uf("u_diskBrightness", s.diskBrightness);
        uf("u_starBrightness", s.starBrightness);
        uf("u_starfieldIsFile", starfieldIsFile_ ? 1.0f : 0.0f);

        uf("u_r", (float)cam.r);
        uf("u_theta", (float)cam.theta);
        uf("u_phi", (float)cam.phi);
        uf("u_dr", (float)cam.dr);
        uf("u_dphi", (float)cam.dphi);
        um("u_camBasis", cam.zamoTetrad());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, starfield_);
        ui("u_starfield", 1);

        // Workgroup is 16x16 (matches compute.glsl).
        GLuint gx = (GLuint)((W_ + 15) / 16);
        GLuint gy = (GLuint)((H_ + 15) / 16);
        glDispatchCompute(gx, gy, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        lastSteps_ = s.maxSteps;

        // 2. Display pass.
        int dispW = (displayWidth > 0) ? displayWidth : W_;
        int dispH = (displayHeight > 0) ? displayHeight : H_;

        glViewport(0, 0, dispW, dispH);
        glClearColor(0.04f, 0.03f, 0.027f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(displayProg_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, image_);
        glUniform1i(glGetUniformLocation(displayProg_, "u_image"), 0);
        glUniform1f(glGetUniformLocation(displayProg_, "u_bloom"), s.bloomIntensity);
        glUniform1f(glGetUniformLocation(displayProg_, "u_whiteout"), cam.whiteoutTimer);
        glUniform2f(glGetUniformLocation(displayProg_, "u_res"), (float)dispW, (float)dispH);

        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 3. Trail overlay (optional).
        if (s.showTrail && cam.trail.size() >= 2)
            drawTrail(cam, dispW, dispH);
    }

    void Renderer::drawTrail(const Camera &cam, int displayWidth, int displayHeight)
    {
        int dispW = (displayWidth > 0) ? displayWidth : W_;
        int dispH = (displayHeight > 0) ? displayHeight : H_;

        std::vector<float> verts;
        verts.reserve(cam.trail.size() * 3);
        for (const auto &p : cam.trail)
        {
            verts.push_back(p.x);
            verts.push_back(p.y);
            verts.push_back(p.z);
        }
        glBindBuffer(GL_ARRAY_BUFFER, trailVbo_);
        // Reallocate to current size each frame (count caps at 1000, cheap).
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

        glUseProgram(trailProg_);

        // Build a simple view-projection that orbits the BH around the camera
        // current position so the trail appears anchored in world space.
        glm::mat4 tetrad = cam.zamoTetrad();
        glm::vec3 fwd = -glm::vec3(tetrad[2]);
        glm::vec3 up  = glm::vec3(tetrad[1]);
        glm::vec3 camP((float)(cam.r * std::sin(cam.theta) * std::cos(cam.phi)),
                       (float)(cam.r * std::sin(cam.theta) * std::sin(cam.phi)),
                       (float)(cam.r * std::cos(cam.theta)));
        glm::mat4 view = glm::lookAt(camP, camP + fwd, up);
        glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                          (float)dispW / (float)dispH, 0.05f, 200.0f);
        glm::mat4 vp = proj * view;
        glUniformMatrix4fv(glGetUniformLocation(trailProg_, "u_viewProj"), 1, GL_FALSE, &vp[0][0]);
        glUniform1i(glGetUniformLocation(trailProg_, "u_count"), (int)cam.trail.size());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(trailVao_);
        glDrawArrays(GL_LINE_STRIP, 0, (int)cam.trail.size());
        glDisable(GL_BLEND);
    }

    bool Renderer::screenshot(const std::string &path) const
    {
        std::vector<unsigned char> pix(W_ * H_ * 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, W_, H_, GL_RGB, GL_UNSIGNED_BYTE, pix.data());
        std::vector<unsigned char> flip(W_ * H_ * 3);
        for (int y = 0; y < H_; ++y)
        {
            std::memcpy(&flip[(H_ - 1 - y) * W_ * 3], &pix[y * W_ * 3], W_ * 3);
        }
        return stbi_write_png(path.c_str(), W_, H_, 3, flip.data(), W_ * 3) != 0;
    }

} // namespace kerr
