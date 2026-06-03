#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace kerr {

static constexpr double kPi    = 3.14159265358979323846;
static constexpr double kHalfPi = kPi * 0.5;

void Camera::reset() {
    t = 0; r = 10.0; theta = kHalfPi; phi = 0.0;
    dr = -0.18; dtheta = 0.0; dphi = 0.0;
    E = 1.0; L = 2.0; Q = 0.0;
    yaw = 0; pitch = 0;
    lookDir = glm::vec3(0, 0, -1);
    mode = prevMode = UniverseMode::NORMAL;
    trail.clear();
    whiteoutTimer = 0.0f;
}

void Camera::applyInput(float fwd, float side, float updown, double dt) {
    // Spec §7: WASD adjusts L and dr (not Newtonian thrust).
    dr -= 0.8 * fwd * dt;
    L  += 1.5 * side * dt;
    Q  += 1.0 * updown * dt;
    Q   = std::max(0.0, Q);
}

void Camera::applyMouse(float dx, float dy) {
    yaw   += dx * 0.0025f;
    pitch += dy * 0.0025f;
    pitch  = std::clamp(pitch, -1.5533f, 1.5533f);
    float cp = std::cos(pitch);
    lookDir = glm::vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp);
}

// Effective radial pull derived from the Kerr effective potential for an
// equatorial timelike geodesic with conserved (E, L):
//   V_eff(r) = -M/r + (L² - a²(E²-1))/(2r²) - M(L - aE)²/r³
//   d²r/dλ² = -V_eff'(r)
// This lets r legitimately pass through 0 (the ring singularity) and become
// negative — i.e. the camera emerges into the r<0 universe.
static double radialAccel(const BlackHole& bh, double r, double L, double E) {
    if (std::abs(r) < 1e-3) r = (r >= 0 ? 1e-3 : -1e-3);
    const double a   = bh.a;
    const double M   = bh.M;
    const double r2  = r * r;
    const double r3  = r2 * r;
    const double r4  = r2 * r2;
    // -V_eff'(r):
    return -( M / r2
            - (L*L - a*a*(E*E - 1.0)) / r3
            + 3.0 * M * (L - a*E) * (L - a*E) / r4 );
}

static double azimuthalRate(const BlackHole& bh, double r, double L, double E) {
    const double a = bh.a;
    const double M = bh.M;
    // dφ/dλ for equatorial geodesic:
    //   dφ/dλ = (1/Δ) * [(1 - 2M/r) L + (2Ma/r) E]
    const double Delta = r*r - 2.0*M*r + a*a;
    if (std::abs(Delta) < 1e-4) return 0.0;
    return ( (1.0 - 2.0*M/r) * L + (2.0*M*a/r) * E ) / Delta;
}

void Camera::step(double dt) {
    if (whiteoutTimer > 0)
        whiteoutTimer = std::max(0.0f, whiteoutTimer - (float)dt);

    // Hard caps: long-integration stability. If the user holds W down for a
    // while, dr can blow up; clamp it. Cap dt too so a single 100 ms hitch
    // doesn't shoot the camera to r = +/- 1e20.
    dt = std::min(dt, 0.05);
    dr = std::clamp(dr, -3.0, 3.0);
    L  = std::clamp(L,  -20.0, 20.0);
    Q  = std::clamp(Q,    0.0, 20.0);

    // Sub-step adaptively: more sub-steps when we're close to a horizon or
    // the singularity (where V_eff is stiff). 1–8 sub-steps per frame.
    const double rOut = bh.M + std::sqrt(std::max(0.0, bh.M*bh.M - bh.a*bh.a));
    double prox = std::max(0.1, std::abs(r) - rOut + 1.0);
    int substeps = std::clamp((int)std::round(2.0 + 6.0 / prox), 1, 8);
    double h = dt / substeps;

    auto accel = [&](double r_){ return radialAccel(bh, r_, L, E); };

    for (int i = 0; i < substeps; ++i) {
        double k1r = dr;                       double k1v = accel(r);
        double k2r = dr + 0.5*h*k1v;           double k2v = accel(r + 0.5*h*k1r);
        double k3r = dr + 0.5*h*k2v;           double k3v = accel(r + 0.5*h*k2r);
        double k4r = dr +     h*k3v;           double k4v = accel(r +     h*k3r);

        double nr  = r  + (h / 6.0) * (k1r + 2.0*k2r + 2.0*k3r + k4r);
        double ndr = dr + (h / 6.0) * (k1v + 2.0*k2v + 2.0*k3v + k4v);

        // Reject NaN/Inf — keep last good state instead of poisoning the sim.
        if (std::isfinite(nr) && std::isfinite(ndr)) { r = nr; dr = ndr; }
        else                                         { dr *= 0.5; }
    }

    double dph = azimuthalRate(bh, r, L, E);
    if (std::isfinite(dph)) { dphi = dph; phi += dphi * dt; }
    t += dt;

    // Mode transitions.
    prevMode = mode;
    mode = classify(bh, r);
    if (prevMode == UniverseMode::INSIDE && mode == UniverseMode::NEGATIVE) {
        whiteoutTimer = 0.20f;
    }

    while (phi >  kPi) phi -= 2.0 * kPi;
    while (phi < -kPi) phi += 2.0 * kPi;

    // Trail (pseudo-cartesian; allow negative-r by reflecting).
    Vec3 p = bl2cart(r, theta, phi);
    trail.emplace_back((float)p.x, (float)p.y, (float)p.z);
    if (trail.size() > 512) trail.pop_front();
}

glm::mat4 Camera::zamoTetrad() const {
    glm::vec3 fwd = glm::normalize(lookDir);
    glm::vec3 up0(0, 1, 0);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up0));
    glm::vec3 up    = glm::normalize(glm::cross(right, fwd));
    return glm::mat4(glm::vec4(right, 0), glm::vec4(up, 0),
                     glm::vec4(-fwd, 0),  glm::vec4(0, 0, 0, 1));
}

} // namespace kerr
