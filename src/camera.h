// First-person free-fall camera in Boyer-Lindquist coordinates.
// The user does not push the camera Newtonianly; they nudge the geodesic
// constants (E, L, Q) and the radial direction.  RK4 advances the timelike
// trajectory each frame.  See spec section 7.

#pragma once

#include "metric.h"
#include <glm/glm.hpp>
#include <deque>

namespace kerr {

struct Camera {
    BlackHole bh;

    // Boyer-Lindquist position (t, r, theta, phi).
    double t      = 0.0;
    double r      = 10.0;
    double theta  = 3.14159265358979 * 0.5;
    double phi    = 0.0;

    // Coordinate-velocity components.  Initial: radial infall.
    double dr     = -0.10;
    double dtheta = 0.0;
    double dphi   = 0.0;

    // Conserved-quantity nudges (controls translate into these).
    double E = 1.0;   // energy at infinity
    double L = 2.0;   // axial angular momentum
    double Q = 0.0;   // Carter constant

    // Mouse-driven look direction in the local ZAMO tetrad.
    glm::vec3 lookDir = glm::vec3(0, 0, -1);
    float yaw   = 0.0f;
    float pitch = 0.0f;

    UniverseMode mode = UniverseMode::NORMAL;
    UniverseMode prevMode = UniverseMode::NORMAL;

    // Trail of recent positions for the geodesic line overlay.
    std::deque<glm::vec3> trail;

    // Whiteout flash trigger (Cauchy horizon crossing).
    float whiteoutTimer = 0.0f;

    void reset();
    void step(double dt);                  // advance one frame
    void applyInput(float fwd, float side, // WASD/QE accumulated
                    float updown, double dt);
    void applyMouse(float dx, float dy);

    // ZAMO 4-velocity components, used to build the local tetrad.
    glm::mat4 zamoTetrad() const;
};

} // namespace kerr
