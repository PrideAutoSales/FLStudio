// Checks on the chord engine that run without a DAW:
//   c++ -std=c++20 -O2 -I../Source EngineTest.cpp -o engine-test && ./engine-test
#include "ChordEngine.h"
#include "Presets.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace cruzchords;

static int failures = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (! ok)
        ++failures;
}

static int countTokens (const std::string& s)
{
    int n = 0;
    bool in = false;
    for (char c : s)
    {
        if (c != ' ' && ! in)
            ++n;
        in = c != ' ';
    }
    return n;
}

struct Event
{
    long long sample;
    int note;
    bool on;
};

static std::vector<Event> play (const Sequence& seq, double bpm, double sr, double seconds, int block = 512)
{
    std::vector<Event> events;
    Player player;
    const double bps = bpm / 60.0 / sr;
    const long long total = (long long) (seconds * sr);
    for (long long s = 0; s < total; s += block)
    {
        const int n = (int) std::min<long long> (block, total - s);
        player.process (seq, (double) s * bps, bps, n, s == 0,
                        [&] (int off, int note, float, bool on) { events.push_back ({ s + off, note, on }); });
    }
    player.stop ([&] (int, int note, float, bool on) { events.push_back ({ total, note, on }); }, 0);
    return events;
}

int main()
{
    // Every factory progression reads completely.
    for (const auto& p : presets())
    {
        const auto chords = parseProgression (p.progression);
        check ((int) chords.size() == countTokens (p.progression), std::string ("preset reads: ") + p.genre + " / " + p.name);
    }
    check (presets().size() >= 50, "at least 50 factory progressions");

    // Chord spelling.
    {
        auto v = voiceChord (0, Maj7, 0, 0, Close);
        check (v == std::vector<int> ({ 48, 52, 55, 59 }), "Cmaj7 is C E G B");
        v = voiceChord (9, Min7, 1, 0, Close);
        check (v == std::vector<int> ({ 60, 64, 67, 69 }), "Am7 first inversion is C E G A");
    }

    // Chords in the key.
    {
        const int triads[] = { Major, Minor, Minor, Major, Major, Minor, Dim };
        const int sevenths[] = { Maj7, Min7, Min7, Maj7, Dom7, Min7, HalfDim7 };
        bool ok3 = true, ok7 = true;
        for (int d = 0; d < 7; ++d)
        {
            ok3 &= diatonicChordType (sMajor, d, Triads) == triads[d];
            ok7 &= diatonicChordType (sMajor, d, Sevenths) == sevenths[d];
        }
        check (ok3, "major key triads: I ii iii IV V vi vii-dim");
        check (ok7, "major key sevenths");
        check (diatonicChordType (sMajor, 0, Ninths) == Maj9 && diatonicChordType (sMajor, 1, Ninths) == Min9
                   && diatonicChordType (sMajor, 4, Ninths) == Dom9,
               "major key ninths: Imaj9 ii9 V9");
        check (diatonicChordType (sHarmonicMinor, 4, Sevenths) == Dom7, "harmonic minor V is a dominant 7");
        check (diatonicChordType (sMinor, 0, Ninths) == Min9, "minor key i9");
    }

    // Names and numerals.
    {
        Chord c;
        check (parseRoman ("bVImaj7", c) && c.offset == 8 && c.type == Maj7, "bVImaj7 reads");
        check (romanNumeral (c) == "bVImaj7", "bVImaj7 writes back");
        check (chordName (c, 9, keyUsesFlats (9, sMinor)) == "Fmaj7", "bVImaj7 in A minor is Fmaj7");
        check (parseRoman ("vi9/2", c) && c.offset == 9 && c.type == Min9 && c.beats == 2.0, "vi9/2 reads with length");
        check (! parseRoman ("xyz", c), "nonsense is rejected");
        check (keyUsesFlats (3, sMajor) && ! keyUsesFlats (7, sMajor), "Eb major uses flats, G major uses sharps");
    }

    // Smooth voicing keeps chords close together and in range.
    {
        RenderOptions opt;
        opt.voicing.smooth = true;
        for (const auto& p : presets())
        {
            const auto chords = parseProgression (p.progression);
            const auto voiced = voiceProgression (chords, 0, opt.voicing);
            bool inRange = true, close = true;
            for (size_t i = 0; i < voiced.size(); ++i)
            {
                for (int n : voiced[i])
                    inRange &= n >= 36 && n <= 90;
                if (i > 0)
                {
                    double a = 0, b = 0;
                    for (int n : voiced[i - 1]) a += n;
                    for (int n : voiced[i]) b += n;
                    close &= std::abs (a / voiced[i - 1].size() - b / voiced[i].size()) <= 7.0;
                }
            }
            if (! inRange || ! close)
                check (false, std::string ("smooth voicing for ") + p.name);
        }
        check (true, "smooth voicing stays in range for every preset");
    }

    // Playback: I then V, four beats each, sustained, 120 BPM at 48 kHz.
    {
        RenderOptions opt;
        opt.voicing.smooth = false;
        auto seq = render (parseProgression ("I V"), opt);
        check (seq.totalBeats == 8.0 && seq.numHits == 2, "two chords, eight beats, two hits");

        const auto ev = play (seq, 120.0, 48000.0, 12.0); // 3 loops of 4 s
        int ons = 0, offs = 0;
        std::map<int, int> balance;
        long long gStart = -1;
        for (const auto& e : ev)
        {
            if (e.on) { ++ons; ++balance[e.note]; }
            else { ++offs; --balance[e.note]; }
            if (e.on && e.note == 59 && gStart < 0)
                gStart = e.sample;
        }
        bool balanced = true;
        for (auto& [n, b] : balance)
            balanced &= b == 0;

        check (ons == 6 * 3, "three loops play six chords of three notes");
        check (balanced, "every note on has a note off");
        check (gStart == 96000, "the V chord starts exactly on beat 5 (sample 96000)");
    }

    // House stabs land on the off-beats.
    {
        RenderOptions opt;
        opt.rhythm = rHouseStabs;
        auto seq = render (parseProgression ("i7"), opt);
        const auto ev = play (seq, 120.0, 48000.0, 2.0);
        std::vector<long long> starts;
        for (const auto& e : ev)
            if (e.on && (starts.empty() || starts.back() != e.sample))
                starts.push_back (e.sample);
        check (starts.size() == 4 && starts[0] == 12000 && starts[1] == 36000, "stabs at 1&, 2&, 3&, 4&");
    }

    // Arpeggio plays one note at a time.
    {
        RenderOptions opt;
        opt.rhythm = rArpUp;
        opt.voicing.smooth = false;
        auto seq = render (parseProgression ("I"), opt);
        const auto ev = play (seq, 120.0, 48000.0, 2.0);
        std::vector<int> notes;
        for (const auto& e : ev)
            if (e.on)
                notes.push_back (e.note);
        check (notes.size() == 8 && notes[0] == 48 && notes[1] == 52 && notes[2] == 55 && notes[3] == 48, "arp up: C E G C ...");
    }

    // Starting in the middle of a chord plays it at once.
    {
        RenderOptions opt;
        auto seq = render (parseProgression ("I V"), opt);
        Player player;
        int ons = 0;
        player.process (seq, 5.0, 120.0 / 60.0 / 48000.0, 512, true, [&] (int off, int, float, bool on) { ons += on && off == 0; });
        check (ons == 3, "jumping into beat 6 plays the V chord immediately");
    }

    // One key plays a chord.
    {
        VoicingOptions v;
        v.spread = Close;
        auto e = keyChordOffsets (0, sMajor, Triads, v, 64);
        check (e == std::vector<int> ({ 0, 3, 7 }), "E in C major plays E minor");
        check (keyChordOffsets (0, sMajor, Triads, v, 61).empty(), "C# in C major is not in the scale");
    }

    std::printf ("\n%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
