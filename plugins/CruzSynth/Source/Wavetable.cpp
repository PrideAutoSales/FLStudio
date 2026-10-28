#include "Wavetable.h"
#include <complex>

namespace cs
{
namespace
{
    using cplx = std::complex<double>;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Small in-place radix-2 FFT. Kept local so results are identical on every platform.
    void fft (std::vector<cplx>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[i], a[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = twoPi / (double) len * (inverse ? 1.0 : -1.0);
            const cplx wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                cplx w (1.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const cplx u = a[i + k], v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }

        if (inverse)
            for (auto& x : a)
                x /= (double) n;
    }

    std::vector<cplx> spectrumOf (const float* frame)
    {
        std::vector<cplx> s ((size_t) Wavetable::kSize);
        for (int i = 0; i < Wavetable::kSize; ++i)
            s[(size_t) i] = frame[i];
        fft (s, false);
        return s;
    }

    // Writes a real frame from harmonic amplitudes/phases (index 0 = fundamental).
    void frameFromHarmonics (float* out, const std::vector<double>& amps, const std::vector<double>& phases)
    {
        constexpr int N = Wavetable::kSize;
        std::vector<cplx> s ((size_t) N, cplx (0.0));
        const int count = juce::jmin ((int) amps.size(), N / 2 - 1);
        for (int h = 0; h < count; ++h)
        {
            const int k = h + 1;
            const double ph = h < (int) phases.size() ? phases[(size_t) h] : 0.0;
            // A sine with amplitude A is A/2j at +k and its conjugate at -k.
            const cplx c = std::polar (amps[(size_t) h] * 0.5 * N, ph - juce::MathConstants<double>::halfPi);
            s[(size_t) k] = c;
            s[(size_t) (N - k)] = std::conj (c);
        }
        fft (s, true);
        for (int i = 0; i < N; ++i)
            out[i] = (float) s[(size_t) i].real();
    }

    void normaliseFrame (float* f, float target = 0.98f)
    {
        float peak = 0.0f;
        for (int i = 0; i < Wavetable::kSize; ++i)
            peak = juce::jmax (peak, std::abs (f[i]));
        if (peak > 1.0e-6f)
            juce::FloatVectorOperations::multiply (f, target / peak, Wavetable::kSize);
    }
}

//==============================================================================
Wavetable::Wavetable (juce::String n, std::vector<float> frames, int factory)
    : name (std::move (n)), factoryIndex (factory), raw (std::move (frames))
{
    numFrames = juce::jlimit (1, kMaxFrames, (int) (raw.size() / kSize));
    raw.resize ((size_t) numFrames * kSize, 0.0f);
    buildMips();
}

void Wavetable::buildMips()
{
    mips.assign ((size_t) numFrames * kLevels * (kSize + 1), 0.0f);
    std::vector<cplx> work ((size_t) kSize);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto spec = spectrumOf (getRawFrame (f));
        for (int l = 0; l < kLevels; ++l)
        {
            const int maxHarm = 1024 >> l;
            std::fill (work.begin(), work.end(), cplx (0.0));
            for (int k = 1; k <= juce::jmin (maxHarm, kSize / 2 - 1); ++k)
            {
                work[(size_t) k] = spec[(size_t) k];
                work[(size_t) (kSize - k)] = spec[(size_t) (kSize - k)];
            }
            fft (work, true);
            auto* dst = mips.data() + ((size_t) f * kLevels + (size_t) l) * (kSize + 1);
            for (int i = 0; i < kSize; ++i)
                dst[i] = (float) work[(size_t) i].real();
            dst[kSize] = dst[0];
        }
    }
}

//==============================================================================
namespace tables
{
juce::StringArray factoryNames()
{
    return { "Basic Shapes", "Harmonic Sweep", "Pulse Width", "Vowel Morph", "Sync Sweep",
             "FM Growl", "Fold Sweep", "Digital Steps", "Organ Bars", "Breath",
             "Hollow Lead", "Glass Bell", "Sine", "Saw",
             "Talk Growl", "Metal FM", "Squash Saw", "Comb Saw", "Bit Grit", "Chord Stack" };
}

std::vector<float> generateFactory (int index)
{
    constexpr int N = Wavetable::kSize;
    constexpr double pi = juce::MathConstants<double>::pi;
    std::vector<float> out;

    auto addFrames = [&] (int count) { out.assign ((size_t) count * N, 0.0f); };
    auto frame = [&] (int f) { return out.data() + (size_t) f * N; };
    auto zeroPhases = std::vector<double> (512, 0.0);

    auto sawAmps = [] (int n) { std::vector<double> a ((size_t) n); for (int h = 0; h < n; ++h) a[(size_t) h] = 1.0 / (h + 1); return a; };
    auto sawPhases = [] (int n) { std::vector<double> p ((size_t) n); for (int h = 0; h < n; ++h) p[(size_t) h] = (h % 2 == 0) ? 0.0 : pi; return p; };

    switch (index)
    {
        case 0: // Basic Shapes: sine, triangle, saw, square
        {
            addFrames (4);
            frameFromHarmonics (frame (0), { 1.0 }, zeroPhases);
            std::vector<double> tri (512), triPh (512, 0.0), sq (512);
            for (int h = 0; h < 512; ++h)
            {
                const int k = h + 1;
                tri[(size_t) h] = (k % 2 == 1) ? 1.0 / (k * k) : 0.0;
                triPh[(size_t) h] = ((k / 2) % 2 == 1) ? pi : 0.0;
                sq[(size_t) h] = (k % 2 == 1) ? 1.0 / k : 0.0;
            }
            frameFromHarmonics (frame (1), tri, triPh);
            frameFromHarmonics (frame (2), sawAmps (1000), sawPhases (1000));
            frameFromHarmonics (frame (3), sq, zeroPhases);
            break;
        }
        case 1: // Harmonic Sweep: harmonics fade in one after another
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double count = std::pow (1.095, f) + f * 0.6;
                std::vector<double> a (256);
                for (int h = 0; h < 256; ++h)
                    a[(size_t) h] = juce::jlimit (0.0, 1.0, count - h) / std::sqrt ((double) h + 1.0);
                frameFromHarmonics (frame (f), a, zeroPhases);
            }
            break;
        }
        case 2: // Pulse Width: 50% down to a thin pulse
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double duty = 0.5 - 0.48 * f / 63.0;
                std::vector<double> a (800), p (800, pi * 0.5);
                for (int h = 0; h < 800; ++h)
                {
                    const int k = h + 1;
                    const double c = 2.0 / (k * pi) * std::sin (k * pi * duty);
                    a[(size_t) h] = std::abs (c);
                    p[(size_t) h] = (c >= 0 ? pi * 0.5 : -pi * 0.5) - k * pi * duty;
                }
                frameFromHarmonics (frame (f), a, p);
            }
            break;
        }
        case 3: // Vowel Morph: formant peaks move a > e > i > o > u
        {
            addFrames (64);
            const double formants[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 },
                                            { 450, 800, 2830 }, { 325, 700, 2530 } };
            const double gains[3] = { 1.0, 0.6, 0.35 };
            for (int f = 0; f < 64; ++f)
            {
                const double pos = f / 63.0 * 4.0;
                const int v0 = juce::jmin (3, (int) pos);
                const double t = pos - v0;
                std::vector<double> a (200);
                for (int h = 0; h < 200; ++h)
                {
                    const double hz = (h + 1) * 110.0;
                    double amp = 0.0;
                    for (int i = 0; i < 3; ++i)
                    {
                        const double fc = formants[v0][i] * (1.0 - t) + formants[v0 + 1][i] * t;
                        const double bw = 80.0 + fc * 0.08;
                        amp += gains[i] * std::exp (-0.5 * std::pow ((hz - fc) / bw, 2.0));
                    }
                    a[(size_t) h] = amp + 0.02 / (h + 1);
                }
                frameFromHarmonics (frame (f), a, sawPhases (200));
            }
            break;
        }
        case 4: // Sync Sweep: a saw hard-synced at rising ratios
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double ratio = 1.0 + 7.0 * f / 63.0;
                for (int i = 0; i < N; ++i)
                {
                    const double p = std::fmod ((double) i / N * ratio, 1.0);
                    frame (f)[i] = (float) (1.0 - 2.0 * p) * (float) (1.0 - (double) i / N * 0.3);
                }
            }
            break;
        }
        case 5: // FM Growl: phase modulation index rises
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double idx = 7.0 * f / 63.0;
                for (int i = 0; i < N; ++i)
                {
                    const double p = (double) i / N * 2.0 * pi;
                    frame (f)[i] = (float) std::sin (p + idx * std::sin (2.0 * p) + idx * 0.35 * std::sin (3.0 * p));
                }
            }
            break;
        }
        case 6: // Fold Sweep: sine driven into a wavefolder
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double g = 1.0 + 9.0 * f / 63.0;
                for (int i = 0; i < N; ++i)
                {
                    const double s = std::sin ((double) i / N * 2.0 * pi) * 0.8 + 0.2 * std::sin ((double) i / N * 4.0 * pi);
                    frame (f)[i] = (float) std::sin (g * s * pi * 0.5);
                }
            }
            break;
        }
        case 7: // Digital Steps: fewer and fewer amplitude steps
        {
            addFrames (32);
            for (int f = 0; f < 32; ++f)
            {
                const double steps = std::round (std::pow (2.0, 6.0 - 5.0 * f / 31.0));
                for (int i = 0; i < N; ++i)
                {
                    const double x = std::sin ((double) i / N * 2.0 * pi) * 0.5 + (1.0 - 2.0 * i / (double) N) * 0.5;
                    frame (f)[i] = (float) (std::round (x * steps) / steps);
                }
            }
            break;
        }
        case 8: // Organ Bars: drawbar registrations
        {
            addFrames (32);
            const int harmonics[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            const double regs[4][9] = { { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 6, 8, 0, 4, 0, 0, 0 },
                                        { 8, 6, 8, 6, 5, 4, 3, 2, 1 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
            for (int f = 0; f < 32; ++f)
            {
                const double pos = f / 31.0 * 3.0;
                const int r0 = juce::jmin (2, (int) pos);
                const double t = pos - r0;
                std::vector<double> a (16, 0.0);
                for (int b = 0; b < 9; ++b)
                    a[(size_t) harmonics[b] - 1] += (regs[r0][b] * (1.0 - t) + regs[r0 + 1][b] * t) / 8.0;
                frameFromHarmonics (frame (f), a, zeroPhases);
            }
            break;
        }
        case 9: // Breath: random-phase spectrum that opens up
        {
            addFrames (64);
            juce::Random rng (1234);
            std::vector<double> ph (512);
            for (auto& p : ph)
                p = rng.nextDouble() * 2.0 * pi;
            for (int f = 0; f < 64; ++f)
            {
                const double cutoff = 2.0 + std::pow (f / 63.0, 2.0) * 300.0;
                std::vector<double> a (512);
                for (int h = 0; h < 512; ++h)
                    a[(size_t) h] = std::exp (-(double) h / cutoff) / std::sqrt ((double) h + 1.0) * (0.6 + 0.4 * std::abs (std::sin (h * 1.7)));
                frameFromHarmonics (frame (f), a, ph);
            }
            break;
        }
        case 10: // Hollow Lead: square that fills in toward a saw
        {
            addFrames (32);
            for (int f = 0; f < 32; ++f)
            {
                const double even = f / 31.0;
                std::vector<double> a (600);
                for (int h = 0; h < 600; ++h)
                    a[(size_t) h] = ((h + 1) % 2 == 1 ? 1.0 : even) / (h + 1);
                frameFromHarmonics (frame (f), a, zeroPhases);
            }
            break;
        }
        case 11: // Glass Bell: sparse high partials crossfading
        {
            addFrames (32);
            const int partials[] = { 1, 3, 5, 7, 11, 13, 17, 23, 29, 31 };
            for (int f = 0; f < 32; ++f)
            {
                const double t = f / 31.0;
                std::vector<double> a (32, 0.0);
                for (int i = 0; i < 10; ++i)
                {
                    const double centre = t * 9.0;
                    a[(size_t) partials[i] - 1] = std::exp (-0.35 * std::pow (i - centre, 2.0)) * 0.8 + (i == 0 ? 0.5 : 0.0);
                }
                frameFromHarmonics (frame (f), a, zeroPhases);
            }
            break;
        }
        case 12: // Sine
            addFrames (1);
            frameFromHarmonics (frame (0), { 1.0 }, zeroPhases);
            break;
        case 14: // Talk Growl: a bright saw through formants moving o > a > e > i, with a nasal peak
        {
            addFrames (64);
            const double formants[4][3] = { { 450, 800, 2800 }, { 800, 1200, 2700 }, { 500, 1800, 2600 }, { 300, 2300, 3200 } };
            for (int f = 0; f < 64; ++f)
            {
                const double pos = f / 63.0 * 3.0;
                const int v0 = juce::jmin (2, (int) pos);
                const double t = pos - v0;
                std::vector<double> a (300);
                for (int h = 0; h < 300; ++h)
                {
                    const double hz = (h + 1) * 80.0;
                    double amp = 0.0;
                    for (int i = 0; i < 3; ++i)
                    {
                        const double fc = formants[v0][i] * (1.0 - t) + formants[v0 + 1][i] * t;
                        const double bw = 60.0 + fc * 0.06;
                        amp += (i == 0 ? 1.0 : 0.8) * std::exp (-0.5 * std::pow ((hz - fc) / bw, 2.0));
                    }
                    a[(size_t) h] = amp * 1.4 + 0.25 / (h + 1);
                }
                frameFromHarmonics (frame (f), a, sawPhases (300));
            }
            break;
        }
        case 15: // Metal FM: 1:3 and 1:5 phase modulation, index rising into a metallic scream
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double idx = 9.0 * std::pow (f / 63.0, 1.3);
                for (int i = 0; i < N; ++i)
                {
                    const double p = (double) i / N * 2.0 * pi;
                    frame (f)[i] = (float) std::sin (p + idx * std::sin (3.0 * p + 0.6 * std::sin (5.0 * p)));
                }
            }
            break;
        }
        case 16: // Squash Saw: a saw driven harder and harder until it folds over
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double g = 1.0 + 24.0 * std::pow (f / 63.0, 2.0);
                for (int i = 0; i < N; ++i)
                {
                    const double x = 1.0 - 2.0 * (double) i / N;
                    const double y = std::tanh (g * x) / std::tanh (g);
                    frame (f)[i] = (float) (f > 40 ? std::sin (y * pi * 0.5 * (1.0 + (f - 40) / 23.0 * 1.5)) : y);
                }
            }
            break;
        }
        case 17: // Comb Saw: a saw with comb notches sweeping through it, like a flanger frozen in place
        {
            addFrames (64);
            for (int f = 0; f < 64; ++f)
            {
                const double d = 0.02 + 0.3 * f / 63.0;
                std::vector<double> a (800);
                for (int h = 0; h < 800; ++h)
                    a[(size_t) h] = std::abs (std::cos (pi * (h + 1) * d)) / (h + 1);
                frameFromHarmonics (frame (f), a, sawPhases (800));
            }
            break;
        }
        case 18: // Bit Grit: saw plus square, bit-reduced and sample-reduced harder each frame
        {
            addFrames (32);
            for (int f = 0; f < 32; ++f)
            {
                const double bits = std::pow (2.0, 7.0 - 5.0 * f / 31.0);
                const int hold = 1 + f * 2;
                for (int i = 0; i < N; ++i)
                {
                    const int j = (i / hold) * hold;
                    const double ph = (double) j / N;
                    const double x = 0.6 * (1.0 - 2.0 * ph) + 0.4 * (ph < 0.5 ? 1.0 : -1.0);
                    frame (f)[i] = (float) (std::round (x * bits) / bits);
                }
            }
            break;
        }
        case 19: // Chord Stack: harmonics 4, 5 and 6 (a major chord) growing out of a sine
        {
            addFrames (32);
            for (int f = 0; f < 32; ++f)
            {
                const double t = f / 31.0;
                std::vector<double> a (24, 0.0);
                a[0] = 1.0;
                a[1] = 0.5 * t;
                a[3] = 0.9 * t; a[4] = 0.8 * t; a[5] = 0.7 * t;
                a[7] = 0.45 * t * t; a[9] = 0.4 * t * t; a[11] = 0.35 * t * t;
                a[15] = 0.2 * t * t; a[19] = 0.15 * t * t; a[23] = 0.12 * t * t;
                frameFromHarmonics (frame (f), a, zeroPhases);
            }
            break;
        }
        default: // Saw (index 13)
            addFrames (1);
            frameFromHarmonics (frame (0), sawAmps (1000), sawPhases (1000));
            break;
    }

    for (int f = 0; f < (int) out.size() / N; ++f)
    {
        removeDc (frame (f));
        normaliseFrame (frame (f));
    }
    return out;
}

WavetablePtr makeFactory (int index)
{
    const auto names = factoryNames();
    index = juce::jlimit (0, names.size() - 1, index);
    return std::make_shared<const Wavetable> (names[index], generateFactory (index), index);
}

std::vector<float> harmonicMagnitudes (const float* frame, int numHarmonics)
{
    const auto s = spectrumOf (frame);
    std::vector<float> m ((size_t) numHarmonics);
    for (int h = 0; h < numHarmonics; ++h)
        m[(size_t) h] = (float) (std::abs (s[(size_t) h + 1]) * 2.0 / Wavetable::kSize);
    return m;
}

void setHarmonicMagnitudes (float* frame, const std::vector<float>& mags)
{
    constexpr int N = Wavetable::kSize;
    auto s = spectrumOf (frame);
    for (int h = 0; h < (int) mags.size() && h + 1 < N / 2; ++h)
    {
        const int k = h + 1;
        const double ph = std::abs (s[(size_t) k]) > 1.0e-9 ? std::arg (s[(size_t) k]) : -juce::MathConstants<double>::halfPi;
        s[(size_t) k] = std::polar ((double) mags[(size_t) h] * 0.5 * N, ph);
        s[(size_t) (N - k)] = std::conj (s[(size_t) k]);
    }
    s[0] = 0.0;
    fft (s, true);
    for (int i = 0; i < N; ++i)
        frame[i] = (float) s[(size_t) i].real();
}

void removeDc (float* frame)
{
    double sum = 0.0;
    for (int i = 0; i < Wavetable::kSize; ++i)
        sum += frame[i];
    const float dc = (float) (sum / Wavetable::kSize);
    for (int i = 0; i < Wavetable::kSize; ++i)
        frame[i] -= dc;
}

void normalise (std::vector<float>& frames)
{
    float peak = 0.0f;
    for (auto v : frames)
        peak = juce::jmax (peak, std::abs (v));
    if (peak > 1.0e-6f)
        for (auto& v : frames)
            v *= 0.98f / peak;
}

std::vector<float> loadFromFile (const juce::File& file, juce::String& error)
{
    constexpr int N = Wavetable::kSize;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "That file isn't an audio file this synth can read (use WAV or AIFF).";
        return {};
    }

    const auto len = (int) std::min (reader->lengthInSamples, (juce::int64) N * Wavetable::kMaxFrames * 4);
    if (len < 16)
    {
        error = "That file is too short to use as a wavetable.";
        return {};
    }

    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] += buf.getSample (c, i) / (float) buf.getNumChannels();

    auto resample = [] (const float* src, int srcLen, float* dst, int dstLen)
    {
        for (int i = 0; i < dstLen; ++i)
        {
            const double x = (double) i * srcLen / dstLen;
            const int i0 = (int) x;
            const int i1 = (i0 + 1) % srcLen;
            const float t = (float) (x - i0);
            dst[i] = src[i0] + t * (src[i1] - src[i0]);
        }
    };

    std::vector<float> frames;
    if (len % N == 0)
    {
        // Already laid out as 2048-sample frames (the common wavetable format).
        const int count = juce::jmin (Wavetable::kMaxFrames, len / N);
        frames.assign (mono.begin(), mono.begin() + (ptrdiff_t) count * N);
    }
    else if (len <= 2 * N)
    {
        // A single cycle at another length.
        frames.resize ((size_t) N);
        resample (mono.data(), len, frames.data(), N);
    }
    else
    {
        // A longer sound: cut it into evenly spaced slices.
        const int count = juce::jlimit (2, Wavetable::kMaxFrames, len / N);
        const int slice = len / count;
        frames.resize ((size_t) count * N);
        for (int f = 0; f < count; ++f)
            resample (mono.data() + (size_t) f * (size_t) slice, slice, frames.data() + (size_t) f * N, N);
    }

    for (int f = 0; f < (int) frames.size() / N; ++f)
        removeDc (frames.data() + (size_t) f * N);
    normalise (frames);
    return frames;
}

bool saveToWav (const Wavetable& table, const juce::File& file)
{
    const auto& raw = table.getRawFrames();
    juce::AudioBuffer<float> buf (1, (int) raw.size());
    buf.copyFrom (0, 0, raw.data(), (int) raw.size());

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate (44100.0)
                                                   .withNumChannels (1)
                                                   .withBitsPerSample (32)
                                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}
} // namespace tables
} // namespace cs
