#include "GrandData.h"
#include "CruzGrandData.h"
#include <cmath>

namespace cs::piano
{
namespace
{
    std::unique_ptr<GrandData> decode (const void* data, size_t size)
    {
        juce::MemoryInputStream compressed (data, size, false);
        juce::GZIPDecompressorInputStream unzip (&compressed, false, juce::GZIPDecompressorInputStream::zlibFormat);
        juce::MemoryBlock raw;
        unzip.readIntoMemoryBlock (raw);
        juce::MemoryInputStream in (raw, false);

        char magic[4] {};
        if (in.read (magic, 4) != 4 || std::memcmp (magic, "CGPD", 4) != 0 || in.readInt() != 1)
            return nullptr;

        auto d = std::make_unique<GrandData>();
        d->sampleRate = in.readFloat();
        d->numKeys = (int) (juce::uint16) in.readShort();
        d->numLayers = (int) (juce::uint16) in.readShort();
        for (int k = 0; k < d->numKeys; ++k)
            d->keyNotes.push_back ((int) (juce::uint8) in.readByte());

        const float phaseScale = juce::MathConstants<float>::pi / 32767.0f;
        for (int i = 0; i < d->numKeys * d->numLayers; ++i)
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
            const float residualScale = in.readFloat() / 32767.0f;

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

            // Stored as differences from the previous frame.
            r.firstResidual = (int) (d->residual.size() / 2);
            r.residualLength = residualLength;
            int run[2] = { 0, 0 };
            for (int s = 0; s < residualLength * 2; ++s)
            {
                run[s & 1] += in.readShort();
                d->residual.push_back ((float) run[s & 1] * residualScale);
            }

            d->records.push_back (r);
        }

        if (in.isExhausted() && (int) d->records.size() == d->numKeys * d->numLayers && d->numKeys > 0)
            return d;
        return nullptr;
    }
}

const GrandData* GrandData::get()
{
    static const std::unique_ptr<GrandData> data = decode (CruzGrandData::ConcertGrand_cgpd, (size_t) CruzGrandData::ConcertGrand_cgpdSize);
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
