#pragma once
#include "glad/glad.h"
#include "camera.h"
#include <string>

namespace kerr
{

    struct RenderSettings
    {
        // Defaults tuned for a usable first run on mid-range Windows GPUs.
        int maxSteps = 96;
        float diskBrightness = 1.0f;
        float starBrightness = 0.8f;
        float bloomIntensity = 0.20f;
        bool showGrid = false;
        bool showTrail = false;
        bool showPhotonSphere = false;
    };

    class Renderer
    {
    public:
        bool init(int width, int height);
        void resize(int width, int height);
        void shutdown();
        bool loadStarfield(const std::string &path);
        void render(const Camera &cam, const RenderSettings &s, float timeSec);
        bool screenshot(const std::string &path) const;

        int width() const { return W_; }
        int height() const { return H_; }
        int lastSteps() const { return lastSteps_; }

    private:
        void drawTrail(const Camera &cam);

        int W_ = 0, H_ = 0;
        GLuint image_ = 0;
        GLuint starfield_ = 0;
        GLuint computeProg_ = 0;
        GLuint displayProg_ = 0;
        GLuint trailProg_ = 0;
        GLuint vao_ = 0;
        GLuint trailVao_ = 0;
        GLuint trailVbo_ = 0;
        int lastSteps_ = 0;
        bool starfieldIsFile_ = false;
    };

} // namespace kerr
