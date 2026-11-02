#include "GrandData.h"
#include "CruzGrandData.h"
#include <cmath>

namespace cs::piano
{
namespace
{
    // Samples stored as differences from the previous frame, per channel.
    void readDeltas (juce::InputStream& in, std::vector<juce::int16>& out, int frames, int channels)
    {
        int run[2] = { 0, 0 };
        for (int s = 0; s < frames * channels; ++s)
        {
            auto& r = run[s % channels];
            r += in.readShort();
            out.push_back ((juce::int16) r);
        }
    }

    // Version 1: stereo, one take per strength (the grand). Version 2 adds the
    // number of takes and channels, and the release recordings (the bass).
    std::unique_ptr<GrandData> decode (const void* data, size_t size)
    {
        juce::MemoryInputStream compressed (data, size, false);
        juce::GZIPDecompressorInputStream unzip (&compressed, false, juce::GZIPDecompressorInputStream::zlibFormat);
        juce::MemoryBlock raw;
        unzip.readIntoMemoryBlock (raw);
        juce::MemoryInputStream in (raw, false);

        char magic[4] {};
        if (in.read (magic, 4) != 4 || std::memcmp (magic, "CGPD", 4) != 0)
            return nullptr;
        const int version = in.readInt();
        if (version != 1 && version != 2)
            return nullptr;

        auto d = std::make_unique<GrandData>();
        d->sampleRate = in.readFloat();
        d->numKeys = (int) (juce::uint16) in.readShort();
        d->numLayers = (int) (juce::uint16) in.readShort();
        if (version >= 2)
        {
            d->numTakes = (int) (juce::uint16) in.readShort();
            d->channels = (int) (juce::uint16) in.readShort();
        }
        if (d->numKeys <= 0 || d->numLayers < 2 || d->numTakes < 1 || d->channels < 1 || d->channels > 2)
            return nullptr;
        for (int k = 0; k < d->numKeys; ++k)
            d->keyNotes.push_back ((int) (juce::uint8) in.readByte());

        const float phaseScale = juce::MathConstants<float>::pi / 32767.0f;
        const int numRecords = d->numKeys * d->numLayers * d->numTakes;
        for (int i = 0; i < numRecords && ! in.isExhausted(); ++i)
        {
            GrandData::Record r;
            r.f0 = in.readFloat();
            r.loudness = in.readFloat();
            r.level = in.readFloat();
            r.attackTime = in.readFloat();
            for (auto& g : r.attack)
                g = in.readFloat();
            const int numModes = (juce::uint16) in.readShort();
            const int numOvertones = (juce::uint16) in.readShort();
            const int residualLength = in.readInt();
            r.residualScale = in.readFloat() / 32767.0f;

            r.firstOvertone = (int) d->overtones.size();
            r.numOvertones = numOvertones;
            for (int o = 0; o < numOvertones; ++o)
            {
                const float f = in.readFloat();
                d->overtones.push_back ({ f, in.readFloat() });
            }

            r.firstMode = (int) d->modes.size();
            r.numModes = numModes;
            for (int m = 0; m < numModes; ++m)
            {
                GrandData::Mode mode;
                mode.centre = in.readFloat();
                mode.offset = in.readFloat();
                mode.decay = in.readFloat();
                float amp[4];
                for (int c = 0; c < 2; ++c)
                {
                    const float mag = std::pow (10.0f, (float) in.readShort() / 256.0f * 0.05f);
                    const float ph = (float) in.readShort() * phaseScale;
                    amp[c * 2] = mag * std::cos (ph);
                    amp[c * 2 + 1] = mag * std::sin (ph);
                }
                mode.lRe = amp[0]; mode.lIm = amp[1]; mode.rRe = amp[2]; mode.rIm = amp[3];
                d->modes.push_back (mode);
            }

            r.firstResidual = (int) (d->residual.size() / (size_t) d->channels);
            r.residualLength = residualLength;
            readDeltas (in, d->residual, residualLength, d->channels);
            d->records.push_back (r);
        }

        if (version >= 2)
        {
            for (int i = 0; i < d->numKeys * d->numTakes && ! in.isExhausted(); ++i)
            {
                GrandData::Release rel;
                rel.length = in.readInt();
                rel.scale = in.readFloat() / 32767.0f;
                rel.level = in.readFloat();
                rel.first = (int) (d->releaseSamples.size() / (size_t) d->channels);
                readDeltas (in, d->releaseSamples, rel.length, d->channels);
                d->releases.push_back (rel);
            }
            if ((int) d->releases.size() != d->numKeys * d->numTakes)
                return nullptr;
        }

        if (! in.isExhausted() || (int) d->records.size() != numRecords)
            return nullptr;

        // Each key's average pitch over all its recordings, for tuning it exactly.
        for (int k = 0; k < d->numKeys; ++k)
        {
            double logSum = 0.0;
            const int n = d->numLayers * d->numTakes;
            for (int i = 0; i < n; ++i)
                logSum += std::log ((double) d->records[(size_t) (k * n + i)].f0);
            d->keyF0.push_back ((float) std::exp (logSum / n));
        }
        return d;
    }
}

const GrandData* GrandData::get()
{
    static const std::unique_ptr<GrandData> data = decode (CruzGrandData::ConcertGrand_cgpd, (size_t) CruzGrandData::ConcertGrand_cgpdSize);
    return data.get();
}

const GrandData* GrandData::bass()
{
    static const std::unique_ptr<GrandData> data = decode (CruzGrandData::BassGuitar_cgpd, (size_t) CruzGrandData::BassGuitar_cgpdSize);
    return data.get();
}

int GrandData::nearestKey (float note) const noexcept
{
    int best = 0;
    float bestDist = 1.0e9f;
    for (int k = 0; k < numKeys; ++k)
    {
        const float dist = std::abs (note - (float) keyNotes[(size_t) k]);
        if (dist < bestDist)
        {
            best = k;
            bestDist = dist;
        }
    }
    return best;
}

float GrandData::levelAt (int key, float layer) const noexcept
{
    const float l = juce::jlimit (0.0f, (float) (numLayers - 1), layer);
    const int a = juce::jmin ((int) l, numLayers - 2);
    const float t = l - (float) a;
    return record (key, a).level * (1.0f - t) + record (key, a + 1).level * t;
}
} // namespace cs::piano
