#pragma once

// Small DSP building blocks shared by Cruz Opto and Cruz FET.
// Plain C++ with no JUCE dependency, so the models can be tested on their own.

#include <algorithm>
#include <cmath>

namespace cruz
{
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Coefficient for a one-pole smoother with time constant `seconds`:
// state += coef * (target - state)
inline float onePoleCoef (double seconds, double sampleRate) noexcept
{
    return (float) (1.0 - std::exp (-1.0 / (std::max (seconds, 1.0e-7) * sampleRate)));
}

// Removes the DC offset that asymmetric saturation leaves behind (about 5 Hz corner).
struct DcBlocker
{
    void prepare (double sampleRate) noexcept { r = 1.0f - (float) (2.0 * 3.14159265358979 * 5.0 / sampleRate); reset(); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

// First-order high-pass, used to tilt the sidechain toward high frequencies.
struct OnePoleHighPass
{
    void prepare (double sampleRate, double cutoffHz) noexcept
    {
        a = (float) std::exp (-2.0 * 3.14159265358979 * cutoffHz / sampleRate);
        reset();
    }
    void reset() noexcept { lp = 0.0f; }
    float process (float x) noexcept
    {
        lp = x + a * (lp - x);
        return x - lp;
    }

    float a = 0.0f, lp = 0.0f;
};

// Asymmetric tube-style curve. Unity gain for small signals, adds mostly
// second harmonic that grows with level. `drive` sets how early it bends.
struct TubeStage
{
    void setShape (float newDrive, float newBias) noexcept
    {
        drive = newDrive;
        bias = newBias;
        tb = std::tanh (bias);
        norm = 1.0f / (drive * (1.0f - tb * tb));
    }
    float process (float x) const noexcept { return (std::tanh (drive * x + bias) - tb) * norm; }

    float drive = 0.35f, bias = 0.12f, tb = 0.0f, norm = 1.0f;
};
} // namespace cruz
