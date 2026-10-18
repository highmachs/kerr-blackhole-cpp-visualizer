#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace kerr {

static constexpr double kPi    = 3.14159265358979323846;
static constexpr double kHalfPi = kPi * 0.5;

void Camera::reset() {
    t = 0; r = 14.0; theta = 1.35; phi = 0.0;
    dr = -0.15; dtheta = 0.0; dphi = 0.0;
    E = 1.0; L = 2.0; Q = 0.0;
    yaw = 0; pitch = 0; roll = 0.0f;
    yawTarget = pitchTarget = rollTarget = 0.0f;
    fov = fovTarget = 60.0f;
    velocity = glm::vec3(0.0f);
    thrustMultiplier = 1.0f;
    isAutoOrbit = false;
    simSpeed = 1.0f;
    isTouring = false;
    tourTime = 0.0f;
    lookDir = glm::vec3(0, 0, -1);
    mode = prevMode = UniverseMode::NORMAL;
    trail.clear();
    whiteoutTimer = 0.0f;
}

void Camera::applyInput(float fwd, float side, float updown, float rollInput, double dt) {
    if (std::abs(fwd) > 0.01f || std::abs(side) > 0.01f || std::abs(updown) > 0.01f) {
        isAutoOrbit = false;
        isTouring = false;
    }

    // Roll angle adjustment (Z/C) with smooth damping
    rollTarget += rollInput * 1.5f * (float)dt;
    while (rollTarget > (float)kPi)  { rollTarget -= (float)(2.0 * kPi); roll -= (float)(2.0 * kPi); }
    while (rollTarget < -(float)kPi) { rollTarget += (float)(2.0 * kPi); roll += (float)(2.0 * kPi); }

    // 1. Current Cartesian position
    double curR = std::abs(r);
    double sTh = std::sin(theta);
    double cTh = std::cos(theta);
    glm::vec3 pos(
        (float)(curR * sTh * std::cos(phi)),
        (float)(curR * sTh * std::sin(phi)),
        (float)(curR * cTh)
    );

    // 2. Camera basis in world space (matching compute.glsl)
    glm::vec3 centerFwd = (glm::length(pos) > 1e-4f) ? -glm::normalize(pos) : glm::vec3(0, 0, -1);
    glm::vec3 worldK(0.0f, 0.0f, 1.0f);
    glm::vec3 centerRight = glm::cross(centerFwd, worldK);
    if (glm::length(centerRight) < 1e-4f) centerRight = glm::vec3(1.0f, 0.0f, 0.0f);
    centerRight = glm::normalize(centerRight);
    glm::vec3 centerUp = glm::cross(centerRight, centerFwd);

    // 3. User's world look direction
    glm::vec3 worldFwd = glm::normalize(lookDir.x * centerRight + lookDir.y * centerUp - lookDir.z * centerFwd);
    glm::vec3 worldRight = glm::normalize(glm::cross(worldFwd, worldK));
    if (glm::length(worldRight) < 1e-4f) worldRight = centerRight;
    glm::vec3 worldCamUp = glm::normalize(glm::cross(worldRight, worldFwd));

    // 4. Compute movement vector with smooth thruster inertia damping
    float targetSpeed = 4.5f * thrustMultiplier;
    glm::vec3 desiredVel = (worldFwd * fwd + worldRight * side + worldCamUp * updown) * targetSpeed;
    float blendFactor = 1.0f - std::exp(-8.0f * (float)dt);
    velocity = glm::mix(velocity, desiredVel, blendFactor);

    if (glm::length(velocity) < 1e-4f)
        return;

    glm::vec3 moveVec = velocity * (float)dt;

    // 5. Update position
    glm::vec3 nextPos = pos + moveVec;
    double newDist = std::max(0.01, (double)glm::length(nextPos));
    double signR = (r < 0.0) ? -1.0 : 1.0;

    // Ring throat traversal: crossing the disk z=0 within radius rho < a flips into/out of Negative Universe
    double rho = std::sqrt((double)(nextPos.x * nextPos.x + nextPos.y * nextPos.y));
    bool crossedEquator = (pos.z * nextPos.z <= 0.0f) && (std::abs(pos.z - nextPos.z) > 1e-5f);
    if (crossedEquator && rho < bh.a) {
        signR = -signR;
        whiteoutTimer = 0.35f;
        std::printf("[kerr] Traversed ring singularity throat (rho=%.2f < a=%.2f). Transitioned to %s Universe!\n",
                    rho, bh.a, (signR < 0.0) ? "NEGATIVE" : "POSITIVE");
    }

    double newR = signR * newDist;

    // REAL PHYSICS CONSTRAINT: Event horizon point of no return!
    // Once inside the outer horizon (0 < r < rOuter), spacetime geometry forces dr < 0.
    // Backward thrusters cannot push you back out of the horizon!
    double rOut = rOuterHorizon(bh);
    if (r > 0.0 && r <= rOut) {
        if (newR > r) {
            newR = r; // cannot escape back out once past horizon!
        }
    }

    // Radial velocity update: manual thruster damping on dr to avoid spring-back fighting
    double radialThrust = (newR - r) / dt;
    if (r > rOut || r < 0.0) {
        dr = dr * 0.3 + radialThrust * 0.7;
    } else {
        dr = std::min(dr, -0.15); // strictly inward inside horizon
    }

    r = newR;
    theta = std::acos(std::clamp((double)nextPos.z / newDist, -1.0, 1.0));
    theta = std::clamp(theta, 0.01, kPi - 0.01);
    phi = std::atan2((double)nextPos.y, (double)nextPos.x);
}

void Camera::toggleAutoOrbit() {
    isAutoOrbit = !isAutoOrbit;
    if (isAutoOrbit) isTouring = false;
}

void Camera::toggleTour() {
    isTouring = !isTouring;
    if (isTouring) {
        isAutoOrbit = false;
        tourTime = 0.0f;
    }
}

void Camera::applyMouse(float dx, float dy) {
    yawTarget   += dx * 0.0022f;
    pitchTarget += dy * 0.0022f;
    pitchTarget  = std::clamp(pitchTarget, -1.5533f, 1.5533f);
}

// Exact radial acceleration derived from Carter's geodesic equations:
//   (dr/dtau)^2 = R(r)
//   d^2r/dtau^2 = 1/2 R'(r)
// Normalized to coordinate time using regularized Sigma.
// This smoothly passes through r = 0 into negative r (negative universe)
// with zero coordinate singularity and zero division by zero.
static double radialAccel(const BlackHole& bh, double r, double theta, double L, double E, double Q) {
    const double a = bh.a;
    const double cosT = std::cos(theta);
    const double sigma = r * r + a * a * cosT * cosT + 0.05;
    double dR = carterRadialDerivative(bh, r, E, L, Q);
    double aRad = 0.5 * dR / (sigma * sigma);
    return std::clamp(aRad, -25.0, 25.0);
}

static double azimuthalRate(const BlackHole& bh, double r, double theta, double L, double /*E*/) {
    const double a = bh.a;
    const double sinT = std::sin(theta);
    const double cosT = std::cos(theta);
    const double sigma = r * r + a * a * cosT * cosT + 0.05;
    const double sin2 = std::max(0.01, sinT * sinT);
    // Regular ingoing Kerr rate: orbital angular momentum + frame dragging (ZAMO)
    double r2pa2 = r * r + a * a;
    double omega = (2.0 * bh.M * a * r) / (r2pa2 * r2pa2 + 0.1);
    return (L / (sigma * sin2)) + omega;
}

// C1-continuous Catmull-Rom spline interpolation for silky smooth camera trajectories
static double catmullRom(double p0, double p1, double p2, double p3, double u) {
    double u2 = u * u;
    double u3 = u2 * u;
    return 0.5 * ((2.0 * p1) +
                  (-p0 + p2) * u +
                  (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * u2 +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * u3);
}

void Camera::step(double dt) {
    if (whiteoutTimer > 0)
        whiteoutTimer = std::max(0.0f, whiteoutTimer - (float)dt);

    dt *= (double)std::clamp(simSpeed, 0.05f, 5.0f);

    if (isTouring) {
        tourTime += (float)dt;

        // Phased tour through the 6 stages without restart:
        // [0..7s]: Approach 14M -> 5M
        // [7..14s]: Outer horizon plunge 5M -> 1.8M
        // [14..19s]: Between horizons 1.8M -> 0.72M
        // [19..22s]: Cauchy flash crossing at 0.68M
        // [22..25s]: Ring singularity throat passage 0.15M -> -0.8M
        // [25..30s]: Negative universe cruising into -2.5M
        // >= 30s: DROP OFF IN NEGATIVE UNIVERSE! AUTO-TURNAROUND FRAMES WHITE HOLE!
        if (tourTime >= 30.0f) {
            isTouring = false;
            r = -2.5;
            theta = 1.45;
            dr = -0.05;
            mode = UniverseMode::NEGATIVE;
            yaw = 0.0f;
            pitch = 0.0f;
            roll = 0.0f;
            lookDir = glm::vec3(0, 0, -1);
            std::printf("[kerr] Tour completed. Left observer in Negative Universe (r = -2.5M, White Hole framed directly ahead).\n");
            return;
        }

        static const double tourTimes[] = { 0.0, 7.0, 14.0, 19.0, 22.0, 25.0, 30.0 };
        static const double tourR[]     = { 14.0, 5.0, 1.8, 0.72, 0.15, -0.8, -2.5 };
        static const double tourTheta[] = { 1.35, 1.45, 1.52, 1.54, 1.57, 1.52, 1.45 };
        const int numPts = 7;

        int seg = 0;
        for (int i = 0; i < numPts - 1; ++i) {
            if (tourTime >= (float)tourTimes[i] && tourTime <= (float)tourTimes[i + 1]) {
                seg = i;
                break;
            }
        }

        double t0 = tourTimes[seg];
        double t1 = tourTimes[seg + 1];
        double u = std::clamp((tourTime - t0) / (t1 - t0), 0.0, 1.0);

        int i0 = std::max(0, seg - 1);
        int i1 = seg;
        int i2 = seg + 1;
        int i3 = std::min(numPts - 1, seg + 2);

        double targetR = catmullRom(tourR[i0], tourR[i1], tourR[i2], tourR[i3], u);
        double targetTheta = catmullRom(tourTheta[i0], tourTheta[i1], tourTheta[i2], tourTheta[i3], u);

        // Cauchy horizon infinite blueshift optical flash trigger
        if (tourTime >= 19.2f && tourTime <= 19.6f && whiteoutTimer <= 0.05f) {
            whiteoutTimer = 0.40f;
        }

        // C2-continuous SmootherStep turnaround in Negative Universe (24s -> 30s)
        if (tourTime >= 24.0f) {
            float uTurn = std::clamp((tourTime - 24.0f) / 6.0f, 0.0f, 1.0f);
            float s = uTurn * uTurn * uTurn * (uTurn * (uTurn * 6.0f - 15.0f) + 10.0f);
            yawTarget = yaw = (1.0f - s) * (float)kPi;
            pitchTarget = pitch = 0.0f;
            rollTarget = roll = 0.0f;
            float cp = std::cos(pitch);
            lookDir = glm::vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp);
        } else {
            yawTarget = yaw = (float)kPi;
            pitchTarget = pitch = 0.0f;
            rollTarget = roll = 0.0f;
            lookDir = glm::vec3(0, 0, 1);
        }

        dr = (dt > 1e-5) ? ((targetR - r) / dt) : -0.10;
        r = targetR;
        theta = targetTheta;
        mode = classify(bh, r);
        phi += 0.35 * dt;
        while (phi > kPi) phi -= 2.0 * kPi;
        while (phi < -kPi) phi += 2.0 * kPi;
        Vec3 p = bl2cart(r, theta, phi);
        trail.emplace_back((float)p.x, (float)p.y, (float)p.z);
        if (trail.size() > 512) trail.pop_front();
        return;
    }

    // Framerate-independent sub-frame exponential smoothing on look direction and roll
    float lookAlpha = 1.0f - std::exp(-28.0f * (float)dt);
    yaw   = glm::mix(yaw,   yawTarget,   lookAlpha);
    pitch = glm::mix(pitch, pitchTarget, lookAlpha);
    roll  = glm::mix(roll,  rollTarget,  lookAlpha);
    float cp = std::cos(pitch);
    lookDir = glm::vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp);

    if (isAutoOrbit && r > (rOuterHorizon(bh) + 0.15)) {
        dr = dr * std::max(0.0, 1.0 - 4.0 * dt);
        double L_circ = std::sqrt(bh.M * std::max(1.0, std::abs(r)));
        L = (L < 0.0 ? -1.0 : 1.0) * L_circ;
        Q = 0.0;
        theta = theta * 0.95 + kHalfPi * 0.05;
    }

    // Hard caps: long-integration stability.
    dt = std::min(dt, 0.05);
    dr = std::clamp(dr, -4.0, 4.0);
    L  = std::clamp(L,  -20.0, 20.0);
    Q  = std::clamp(Q,    0.0, 20.0);

    // Sub-step adaptively: 2-8 sub-steps per frame.
    const double rOut = rOuterHorizon(bh);
    double prox = std::max(0.1, std::abs(r - rOut));
    int substeps = std::clamp((int)std::round(2.0 + 4.0 / prox), 2, 8);
    double h = dt / substeps;

    auto accel = [&](double r_){ return radialAccel(bh, r_, theta, L, E, Q); };

    for (int i = 0; i < substeps; ++i) {
        double k1r = dr;                       double k1v = accel(r);
        double k2r = dr + 0.5*h*k1v;           double k2v = accel(r + 0.5*h*k1r);
        double k3r = dr + 0.5*h*k2v;           double k3v = accel(r + 0.5*h*k2r);
        double k4r = dr +     h*k3v;           double k4v = accel(r +     h*k3r);

        double nr  = r  + (h / 6.0) * (k1r + 2.0*k2r + 2.0*k3r + k4r);
        double ndr = dr + (h / 6.0) * (k1v + 2.0*k2v + 2.0*k3v + k4v);

        if (std::isfinite(nr) && std::isfinite(ndr)) { r = nr; dr = ndr; }
        else                                         { dr *= 0.5; }
    }

    double dph = azimuthalRate(bh, r, theta, L, E);
    if (std::isfinite(dph)) { dphi = dph; phi += dphi * dt; }
    t += dt;

    // Mode transitions (spec §7 & §19).
    prevMode = mode;
    mode = classify(bh, r);
    // Cauchy horizon crossing triggers whiteout flash (infinite blueshift)
    if (prevMode == UniverseMode::BETWEEN && mode == UniverseMode::INSIDE) {
        whiteoutTimer = 0.25f;
    }
    // Ring singularity crossing into negative universe also triggers whiteout flash
    if (prevMode == UniverseMode::INSIDE && mode == UniverseMode::NEGATIVE) {
        whiteoutTimer = 0.25f;
    }

    while (phi >  kPi) phi -= 2.0 * kPi;
    while (phi < -kPi) phi += 2.0 * kPi;

    // Trail (pseudo-cartesian; allow negative-r by reflecting).
    Vec3 p = bl2cart(r, theta, phi);
    trail.emplace_back((float)p.x, (float)p.y, (float)p.z);
    if (trail.size() > 512) trail.pop_front();
}

glm::mat4 Camera::zamoTetrad() const {
    double curR = std::abs(r);
    double sTh = std::sin(theta);
    double cTh = std::cos(theta);
    glm::vec3 pos(
        (float)(curR * sTh * std::cos(phi)),
        (float)(curR * sTh * std::sin(phi)),
        (float)(curR * cTh)
    );

    glm::vec3 centerFwd = (glm::length(pos) > 1e-4f) ? -glm::normalize(pos) : glm::vec3(0, 0, -1);
    glm::vec3 worldK(0.0f, 0.0f, 1.0f);
    glm::vec3 centerRight = glm::cross(centerFwd, worldK);
    if (glm::length(centerRight) < 1e-4f) centerRight = glm::vec3(1.0f, 0.0f, 0.0f);
    centerRight = glm::normalize(centerRight);
    glm::vec3 centerUp = glm::cross(centerRight, centerFwd);

    glm::vec3 worldFwd = glm::normalize(lookDir.x * centerRight + lookDir.y * centerUp - lookDir.z * centerFwd);
    glm::vec3 worldRight = glm::cross(worldFwd, worldK);
    if (glm::length(worldRight) < 1e-4f) worldRight = centerRight;
    worldRight = glm::normalize(worldRight);
    glm::vec3 worldUp = glm::normalize(glm::cross(worldRight, worldFwd));

    if (std::abs(roll) > 1e-4f) {
        float cr = std::cos(roll);
        float sr = std::sin(roll);
        glm::vec3 r2 = worldRight * cr + worldUp * sr;
        glm::vec3 u2 = -worldRight * sr + worldUp * cr;
        worldRight = r2;
        worldUp = u2;
    }

    return glm::mat4(glm::vec4(worldRight, 0), glm::vec4(worldUp, 0),
                     glm::vec4(-worldFwd, 0),  glm::vec4(0, 0, 0, 1));
}

} // namespace kerr
