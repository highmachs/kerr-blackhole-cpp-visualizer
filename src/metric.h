// Kerr metric helpers (Boyer-Lindquist coordinates, geometrized units G=c=M=1).
// CPU-side reference implementation used for:
//   * Camera state updates (horizon crossings, geodesic stepping)
//   * Sanity-checks against the GPU compute shader
//
// Spec reference: sections 2 and 6 of kerrblackhole.md

#pragma once

#include <algorithm>
#include <cmath>

namespace kerr
{

    // Black hole parameters. M is normalized to 1; only the spin a varies.
    struct BlackHole
    {
        double M = 1.0;
        double a = 0.95; // dimensionless spin (0 .. 0.999)
    };

    inline double rOuterHorizon(const BlackHole &bh)
    {
        return bh.M + std::sqrt(bh.M * bh.M - bh.a * bh.a);
    }

    inline double rInnerHorizon(const BlackHole &bh)
    {
        return bh.M - std::sqrt(bh.M * bh.M - bh.a * bh.a);
    }

    inline double rISCO(const BlackHole &bh)
    {
        // Bardeen et al. (1972). Prograde ISCO for a Kerr BH.
        const double a = bh.a;
        const double Z1 = 1.0 + std::cbrt(1.0 - a * a) *
                                    (std::cbrt(1.0 + a) + std::cbrt(1.0 - a));
        const double Z2 = std::sqrt(3.0 * a * a + Z1 * Z1);
        return bh.M * (3.0 + Z2 - std::sqrt((3.0 - Z1) * (3.0 + Z1 + 2.0 * Z2)));
    }

    inline double rPhotonSphere(const BlackHole &bh)
    {
        // Prograde equatorial photon orbit.
        return 2.0 * bh.M * (1.0 + std::cos((2.0 / 3.0) * std::acos(-bh.a / bh.M)));
    }

    // Core metric scalars at (r, theta).
    struct Coeffs
    {
        double Delta; // r^2 - 2 M r + a^2
        double Sigma; // r^2 + a^2 cos^2 theta
        double A;     // (r^2 + a^2)^2 - a^2 Delta sin^2 theta
        double alpha; // lapse
        double omega; // frame-dragging angular velocity (ZAMO)
    };

    inline Coeffs metricCoeffs(const BlackHole &bh, double r, double theta)
    {
        const double a = bh.a;
        const double sinT = std::sin(theta);
        const double cosT = std::cos(theta);
        const double sin2 = sinT * sinT;
        const double cos2 = cosT * cosT;

        Coeffs c{};
        c.Delta = r * r - 2.0 * bh.M * r + a * a;
        c.Sigma = r * r + a * a * cos2;
        c.A = (r * r + a * a) * (r * r + a * a) - a * a * c.Delta * sin2;
        // Guard against the lapse going imaginary when Delta turns negative
        // (i.e. between horizons).  Use absolute value; consumers handle sign.
        c.alpha = std::sqrt(std::max(1e-12, c.Sigma * std::fabs(c.Delta) / c.A));
        c.omega = (2.0 * a * bh.M * r) / c.A;
        return c;
    }

    // Universe-mode classification (spec section 7).
    enum class UniverseMode : int
    {
        NORMAL = 0,   // r > r_outer
        BETWEEN = 1,  // r_inner < r < r_outer (no static observers)
        INSIDE = 2,   // 0 < r < r_inner       (between horizons, time-flipped)
        NEGATIVE = 3, // r < 0                 (through the ring singularity)
    };

    inline UniverseMode classify(const BlackHole &bh, double r)
    {
        const double rPlus = rOuterHorizon(bh);
        const double rMinus = rInnerHorizon(bh);
        if (r > rPlus)
            return UniverseMode::NORMAL;
        if (r > rMinus)
            return UniverseMode::BETWEEN;
        if (r > 0.0)
            return UniverseMode::INSIDE;
        return UniverseMode::NEGATIVE;
    }

    const char *modeName(UniverseMode m);

    // Convert Boyer-Lindquist (r, theta, phi) to a pseudo-Cartesian for HUD only.
    struct Vec3
    {
        double x, y, z;
    };
    inline Vec3 bl2cart(double r, double theta, double phi)
    {
        const double s = std::sin(theta);
        return {r * s * std::cos(phi), r * s * std::sin(phi), r * std::cos(theta)};
    }

} // namespace kerr
