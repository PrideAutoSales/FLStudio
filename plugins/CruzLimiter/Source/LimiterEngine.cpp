#include "LimiterEngine.h"

namespace cruz
{

namespace
{
    // The true-peak safety stage reacts within a quarter of a millisecond.
    constexpr double truePeakLookaheadSeconds = 0.00025;
    // Small margin so the interpolated (true) peak stays under the ceiling.
    constexpr float truePeakMarginDb = 0.1f;

    float releaseCoefficient (float milliseconds, double rate) noexcept
    {
        return (float) std::exp (-1.0 / (std::max (0.01, (double) milliseconds) * 0.001 * rate));
    }
}

LimiterEngine::StyleShape LimiterEngine::shapeFor (LimiterStyle style) noexcept
{
    // fast release scale, slow envelope attack (ms), slow release scale, slow amount, soft clip
    switch (style)
    {
        case LimiterStyle::clean:    return { 1.00f,  60.0f, 3.0f, 0.85f, 0.00f };
        case LimiterStyle::punch:    return { 0.50f, 150.0f, 2.0f, 0.50f, 0.10f };
        case LimiterStyle::dynamic:  return { 0.30f, 300.0f, 4.0f, 0.60f, 0.00f };
        case LimiterStyle::allround: return { 0.70f, 100.0f, 2.5f, 0.70f, 0.15f };
        case LimiterStyle::loud:     return { 0.25f, 400.0f, 1.5f, 0.35f, 0.50f };
        case LimiterStyle::modern:   return { 0.45f, 120.0f, 3.0f, 0.65f, 0.25f };
        case LimiterStyle::safe:     return { 1.20f,  40.0f, 5.0f, 0.95f, 0.00f };
        case LimiterStyle::bus:      return { 1.50f,  80.0f, 2.0f, 0.90f, 0.05f };
    }
    return { 1.0f, 100.0f, 2.0f, 0.7f, 0.0f };
}

void LimiterEngine::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    maxBlock = std::max (1, maxBlockSize);
    channelCount = std::clamp (numChannels, 1, maxChannels);

    for (size_t i = 0; i < oversamplers.size(); ++i)
        oversamplers[i].prepare (1 << i, maxBlock);

    prepared = true;

    const auto maxLookahead = (int) std::ceil (maxLookaheadMs * 0.001 * sampleRate);
    truePeakLookahead = std::max (1, (int) std::lround (truePeakLookaheadSeconds * sampleRate));

    mainStage.prepare (maxLookahead * 16);
    truePeakStage.prepare (truePeakLookahead, TruePeakEstimator::delay);
    truePeakStage.setHold (TruePeakEstimator::delay);

    const auto maxLatency = Oversampler::tapsPerPhase + maxLookahead
                          + TruePeakEstimator::delay + truePeakLookahead + 1;

    for (int c = 0; c < maxChannels; ++c)
    {
        truePeakAlign[(size_t) c].prepare (TruePeakEstimator::delay);
        truePeakAlign[(size_t) c].setDelay (TruePeakEstimator::delay);
        dryDelay[(size_t) c].prepare (maxLatency);
        dcBlockers[(size_t) c].prepare (sampleRate);
    }

    dryBuffer.setSize (maxChannels, maxBlock);
    truePeakDetector.setSize (maxChannels, maxBlock);
    grTrace.assign ((size_t) maxBlock, 0.0f);
    inputTrace.assign ((size_t) maxBlock, 0.0f);
    truePeakGr.assign ((size_t) maxBlock, 0.0f);
    driveRamp.assign ((size_t) maxBlock, 1.0f);
    outputRamp.assign ((size_t) maxBlock, 1.0f);
    oversampledGr.assign ((size_t) maxBlock * 16, 0.0f);

    drive.reset (sampleRate, 0.02);
    outputScale.reset (sampleRate, 0.02);

    structureDirty = true;
    setSettings (current);
    drive.setCurrentAndTargetValue (drive.getTargetValue());
    outputScale.setCurrentAndTargetValue (outputScale.getTargetValue());
}

void LimiterEngine::reset()
{
    structureDirty = true;
    setSettings (current);
    for (auto& d : dcBlockers) d.reset();
    for (auto& d : ditherers)  d.reset();
}

void LimiterEngine::setSettings (const LimiterSettings& s)
{
    const auto newLookahead = (int) std::lround (std::clamp (s.lookaheadMs, 0.0f, maxLookaheadMs) * 0.001 * sampleRate);

    if (s.oversamplingIndex != current.oversamplingIndex || s.truePeak != current.truePeak
        || newLookahead != lookaheadSamples)
        structureDirty = true;

    current = s;
    current.oversamplingIndex = std::clamp (s.oversamplingIndex, 0, (int) oversamplers.size() - 1);

    drive.setTargetValue (dbToGain (s.inputGainDb - s.ceilingDb));
    outputScale.setTargetValue (dbToGain (s.ceilingDb) * (s.unityGain ? dbToGain (-s.inputGainDb) : 1.0f));

    if (structureDirty)
        applyStructure();
    else
        applyTiming();
}

void LimiterEngine::applyStructure()
{
    structureDirty = false;

    if (! prepared)
        return;

    // Limiting without oversampling leaves distortion right up to Nyquist, where the
    // true-peak level is not well defined, so true-peak mode always runs at least 2x.
    activeOversampling = std::max (current.oversamplingIndex, current.truePeak ? 1 : 0);
    const auto factor = 1 << activeOversampling;

    lookaheadSamples = (int) std::lround (std::clamp (current.lookaheadMs, 0.0f, maxLookaheadMs) * 0.001 * sampleRate);
    const auto lookahead = lookaheadSamples * factor;
    mainStage.setLookahead (lookahead, (int) std::lround (lookahead * current.attackPercent * 0.01f));
    truePeakStage.setLookahead (truePeakLookahead, truePeakLookahead);

    oversamplers[(size_t) activeOversampling].reset();
    for (auto& e : truePeakEstimators) e.reset();
    for (auto& d : truePeakAlign)      d.clear();
    previousTruePeak.fill (0.0f);

    latency = oversamplers[(size_t) activeOversampling].getLatency()
            + lookaheadSamples
            + (current.truePeak ? TruePeakEstimator::delay + truePeakLookahead : 0);

    for (auto& d : dryDelay)
        d.setDelay (latency);

    applyTiming();
}

void LimiterEngine::applyTiming()
{
    const auto factor = 1 << activeOversampling;
    const auto rate = sampleRate * factor;
    const auto shape = shapeFor (current.style);

    mainStage.setAttack ((int) std::lround (mainStage.getLatency() * current.attackPercent * 0.01f));

    StageTiming t;
    t.fastReleaseCoef = releaseCoefficient (current.releaseMs * shape.fastReleaseScale, rate);
    t.slowAttackCoef = releaseCoefficient (shape.slowAttackMs, rate);
    t.slowReleaseCoef = releaseCoefficient (current.releaseMs * shape.slowReleaseScale, rate);
    t.slowAmount = shape.slowAmount;
    t.linkTransients = std::clamp (current.linkTransientsPercent * 0.01f, 0.0f, 1.0f);
    t.linkRelease = std::clamp (current.linkReleasePercent * 0.01f, 0.0f, 1.0f);
    mainStage.setTiming (t);

    StageTiming safety;
    safety.fastReleaseCoef = releaseCoefficient (20.0f, sampleRate);
    safety.slowAttackCoef = safety.slowReleaseCoef = 0.0f;
    safety.slowAmount = 0.0f;
    safety.linkTransients = safety.linkRelease = 1.0f;
    truePeakStage.setTiming (safety);
}

void LimiterEngine::process (juce::AudioBuffer<float>& buffer)
{
    const auto numChannels = std::min (buffer.getNumChannels(), maxChannels);
    const auto numSamples = buffer.getNumSamples();

    jassert (numSamples <= maxBlock);
    if (numChannels == 0 || numSamples == 0 || ! prepared)
        return;

    processChunk (buffer.getArrayOfWritePointers(), numChannels, std::min (numSamples, maxBlock));
}

void LimiterEngine::processChunk (float* const* channels, int numChannels, int numSamples)
{
    const auto shape = shapeFor (current.style);
    auto* dry = dryBuffer.getArrayOfWritePointers();

    // 1. DC filter and drive (input gain, normalised so the ceiling sits at 1.0).
    for (int i = 0; i < numSamples; ++i)
        driveRamp[(size_t) i] = drive.getNextValue();

    for (int c = 0; c < numChannels; ++c)
    {
        auto* x = channels[c];
        for (int i = 0; i < numSamples; ++i)
        {
            auto s = current.dcFilter ? dcBlockers[(size_t) c].process (x[i]) : x[i];
            s *= driveRamp[(size_t) i];
            x[i] = s;
            dry[c][i] = dryDelay[(size_t) c].process (s);
        }
    }

    // 2. Oversampled main limiter.
    auto& oversampler = oversamplers[(size_t) activeOversampling];
    oversampler.upsample (channels, numChannels, numSamples);
    const auto factor = oversampler.getFactor();
    const auto upSamples = numSamples * factor;

    float* upChannels[maxChannels] {};
    for (int c = 0; c < numChannels; ++c)
        upChannels[c] = oversampler.getChannel (c);

    // Soft clipping needs headroom above Nyquist for its harmonics, so it only runs oversampled.
    if (shape.softClip > 0.0f && factor > 1)
    {
        const auto knee = dbToGain (-6.0f * shape.softClip);
        const auto headroom = dbToGain (3.0f * shape.softClip) - knee;

        for (int c = 0; c < numChannels; ++c)
            for (int i = 0; i < upSamples; ++i)
            {
                const auto s = upChannels[c][i];
                const auto mag = std::abs (s);
                if (mag > knee)
                    upChannels[c][i] = std::copysign (knee + headroom * std::tanh ((mag - knee) / headroom), s);
            }
    }

    mainStage.process (upChannels, nullptr, numChannels, upSamples, 1.0f, oversampledGr.data());

    for (int i = 0; i < numSamples; ++i)
    {
        auto gr = 0.0f;
        for (int k = 0; k < factor; ++k)
            gr = std::max (gr, oversampledGr[(size_t) (i * factor + k)]);
        grTrace[(size_t) i] = gr;
    }

    oversampler.downsample (channels, numChannels, numSamples);

    // 3. True-peak safety stage at the host rate.
    if (current.truePeak)
    {
        auto* detector = truePeakDetector.getArrayOfWritePointers();

        for (int c = 0; c < numChannels; ++c)
        {
            auto& estimator = truePeakEstimators[(size_t) c];
            auto& align = truePeakAlign[(size_t) c];
            auto& previous = previousTruePeak[(size_t) c];

            for (int i = 0; i < numSamples; ++i)
            {
                // The aligned sample sits between the previous and the newest interpolated
                // span, so both spans count towards the level it must be limited by.
                const auto peak = estimator.process (channels[c][i]);
                const auto aligned = align.process (channels[c][i]);
                detector[c][i] = std::max ({ std::abs (aligned), previous, peak });
                previous = peak;
                channels[c][i] = aligned;
            }
        }

        truePeakStage.process (channels, detector, numChannels, numSamples, dbToGain (-truePeakMarginDb), truePeakGr.data());

        for (int i = 0; i < numSamples; ++i)
            grTrace[(size_t) i] += truePeakGr[(size_t) i];
    }

    // 4. Safety clip, delta monitoring, output ceiling and dither.
    for (int i = 0; i < numSamples; ++i)
        outputRamp[(size_t) i] = outputScale.getNextValue();

    const auto ceilingGain = dbToGain (current.ceilingDb);
    std::fill (inputTrace.begin(), inputTrace.begin() + numSamples, 0.0f);

    for (int c = 0; c < numChannels; ++c)
    {
        auto* y = channels[c];
        for (int i = 0; i < numSamples; ++i)
        {
            auto s = std::clamp (y[i], -1.0f, 1.0f);
            if (current.audition)
                s = dry[c][i] - s;
            s *= outputRamp[(size_t) i];
            if (current.ditherBits > 0)
                s = ditherers[(size_t) c].process (s, current.ditherBits, current.noiseShaping);
            y[i] = s;

            inputTrace[(size_t) i] = std::max (inputTrace[(size_t) i], std::abs (dry[c][i]) * ceilingGain);
        }
    }
}

} // namespace cruz
