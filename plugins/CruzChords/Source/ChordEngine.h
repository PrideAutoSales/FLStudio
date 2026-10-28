#pragma once

// Music theory and playback for Cruz Chords. No JUCE in here, so the engine
// test can build it on its own.
//
// A progression is a list of chords. Each chord is stored relative to the key
// (semitones above the tonic), so changing the key transposes everything.
// render() turns the progression into a Sequence: the notes of every chord and
// a flat list of timed "hits" from the rhythm. The Player walks that list in
// beats and reports note ons and offs at sample offsets.

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace cruzchords
{
//==============================================================================
// Keys and scales

inline const char* noteName (int pc, bool flats)
{
    static const char* sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    pc = ((pc % 12) + 12) % 12;
    return flats ? flatNames[pc] : sharpNames[pc];
}

struct Scale
{
    const char* name;
    std::array<int, 7> steps;
    int relativeMajorOffset; // semitones from this scale's tonic to its parent major key
};

inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> list {
        { "Major", { 0, 2, 4, 5, 7, 9, 11 }, 0 },
        { "Minor", { 0, 2, 3, 5, 7, 8, 10 }, 3 },
        { "Dorian", { 0, 2, 3, 5, 7, 9, 10 }, 10 },
        { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 }, 8 },
        { "Lydian", { 0, 2, 4, 6, 7, 9, 11 }, 7 },
        { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 }, 5 },
        { "Harmonic minor", { 0, 2, 3, 5, 7, 8, 11 }, 3 },
        { "Melodic minor", { 0, 2, 3, 5, 7, 9, 11 }, 3 },
    };
    return list;
}

// Flats for keys whose signature uses flats (F, Bb, Eb, Ab, Db, Gb majors and their modes).
inline bool keyUsesFlats (int keyPc, int scaleIndex)
{
    const auto& s = scales()[(size_t) std::clamp (scaleIndex, 0, (int) scales().size() - 1)];
    const int major = (keyPc + s.relativeMajorOffset) % 12;
    return major == 5 || major == 10 || major == 3 || major == 8 || major == 1 || major == 6;
}

//==============================================================================
// Chord types

enum ChordType
{
    Major, Minor, Dim, Aug, Sus2, Sus4, Maj7, Dom7, Min7, HalfDim7, Dim7, MinMaj7, AugMaj7,
    Six, Min6, Add9, MinAdd9, Maj9, Dom9, Min9, Min11, Dom7Sus4, Dom7b9, Min7b9, HalfDim9, Power, SixNine,
    numChordTypes
};

struct ChordTypeInfo
{
    const char* suffix;      // after the note name: "m7", "maj9"
    const char* romanSuffix; // after the roman numeral
    const char* menuName;    // for the type menu
    bool minorNumeral;       // lower-case roman numeral
    std::array<int, 6> intervals;
    int count;
};

inline const ChordTypeInfo& chordTypeInfo (int type)
{
    // UTF-8 for the degree and half-diminished signs.
    static const ChordTypeInfo table[numChordTypes] = {
        { "", "", "Major", false, { 0, 4, 7 }, 3 },
        { "m", "", "Minor", true, { 0, 3, 7 }, 3 },
        { "dim", "\xc2\xb0", "Diminished", true, { 0, 3, 6 }, 3 },
        { "aug", "+", "Augmented", false, { 0, 4, 8 }, 3 },
        { "sus2", "sus2", "Sus 2", false, { 0, 2, 7 }, 3 },
        { "sus4", "sus4", "Sus 4", false, { 0, 5, 7 }, 3 },
        { "maj7", "maj7", "Major 7", false, { 0, 4, 7, 11 }, 4 },
        { "7", "7", "Dominant 7", false, { 0, 4, 7, 10 }, 4 },
        { "m7", "7", "Minor 7", true, { 0, 3, 7, 10 }, 4 },
        { "m7b5", "\xc3\xb8" "7", "Half-diminished 7", true, { 0, 3, 6, 10 }, 4 },
        { "dim7", "\xc2\xb0" "7", "Diminished 7", true, { 0, 3, 6, 9 }, 4 },
        { "mMaj7", "maj7", "Minor major 7", true, { 0, 3, 7, 11 }, 4 },
        { "maj7#5", "+maj7", "Augmented major 7", false, { 0, 4, 8, 11 }, 4 },
        { "6", "6", "Major 6", false, { 0, 4, 7, 9 }, 4 },
        { "m6", "6", "Minor 6", true, { 0, 3, 7, 9 }, 4 },
        { "add9", "add9", "Add 9", false, { 0, 4, 7, 14 }, 4 },
        { "madd9", "add9", "Minor add 9", true, { 0, 3, 7, 14 }, 4 },
        { "maj9", "maj9", "Major 9", false, { 0, 4, 7, 11, 14 }, 5 },
        { "9", "9", "Dominant 9", false, { 0, 4, 7, 10, 14 }, 5 },
        { "m9", "9", "Minor 9", true, { 0, 3, 7, 10, 14 }, 5 },
        { "m11", "11", "Minor 11", true, { 0, 3, 7, 10, 14, 17 }, 6 },
        { "7sus4", "7sus4", "7 sus 4", false, { 0, 5, 7, 10 }, 4 },
        { "7b9", "7b9", "7 flat 9", false, { 0, 4, 7, 10, 13 }, 5 },
        { "m7b9", "7b9", "Minor 7 flat 9", true, { 0, 3, 7, 10, 13 }, 5 },
        { "m9b5", "\xc3\xb8" "9", "Half-diminished 9", true, { 0, 3, 6, 10, 14 }, 5 },
        { "5", "5", "Power (root + 5th)", false, { 0, 7, 12 }, 3 },
        { "6/9", "6/9", "6/9", false, { 0, 4, 7, 9, 14 }, 5 },
    };
    return table[std::clamp (type, 0, (int) numChordTypes - 1)];
}

// Chord sizes for the chords in the key: plain triads, 7ths or 9ths.
enum ChordSize { Triads, Sevenths, Ninths, numChordSizes };

// Finds the chord type with exactly these intervals, or -1.
inline int findChordType (const int* intervals, int count)
{
    for (int t = 0; t < numChordTypes; ++t)
    {
        const auto& info = chordTypeInfo (t);
        if (info.count == count && std::equal (intervals, intervals + count, info.intervals.begin()))
            return t;
    }
    return -1;
}

// The chord built on scale degree 0..6 by stacking thirds from the scale.
// Falls back to a smaller chord when the stack has no name in the table.
inline int diatonicChordType (int scaleIndex, int degree, int size)
{
    const auto& steps = scales()[(size_t) std::clamp (scaleIndex, 0, (int) scales().size() - 1)].steps;
    degree = ((degree % 7) + 7) % 7;

    for (int s = std::clamp (size, 0, (int) numChordSizes - 1); s >= 0; --s)
    {
        int iv[5];
        const int n = 3 + s;
        for (int k = 0; k < n; ++k)
        {
            const int idx = degree + 2 * k;
            iv[k] = steps[(size_t) (idx % 7)] + 12 * (idx / 7) - steps[(size_t) degree];
        }
        const int t = findChordType (iv, n);
        // A flat 9 sounds harsh as a default pad chord, so those fall back to the 7th.
        if (t >= 0 && t != Dom7b9 && t != Min7b9)
            return t;
    }
    return Major;
}

inline int scaleDegreeOffset (int scaleIndex, int degree)
{
    const auto& steps = scales()[(size_t) std::clamp (scaleIndex, 0, (int) scales().size() - 1)].steps;
    return steps[(size_t) (((degree % 7) + 7) % 7)];
}

// Scale degree 0..6 for a semitone offset from the tonic, or -1 when it is outside the scale.
inline int degreeOfOffset (int scaleIndex, int offset)
{
    const auto& steps = scales()[(size_t) std::clamp (scaleIndex, 0, (int) scales().size() - 1)].steps;
    offset = ((offset % 12) + 12) % 12;
    for (int d = 0; d < 7; ++d)
        if (steps[(size_t) d] == offset)
            return d;
    return -1;
}

//==============================================================================
// Chords

struct Chord
{
    int offset = 0;       // root, in semitones above the key's tonic (0..11)
    int type = Major;
    int inversion = 0;    // 0 = root position, 1 = first inversion, ...
    int octave = 0;       // -2..2
    double beats = 4.0;   // length in quarter notes
    bool locked = false;  // Shuffle leaves locked chords alone
};

inline std::string chordName (const Chord& c, int keyPc, bool flats)
{
    return std::string (noteName (keyPc + c.offset, flats)) + chordTypeInfo (c.type).suffix;
}

// Roman numeral relative to the key, written against the major scale
// (so a minor key's sixth chord is "bVI"), the way producers usually write them.
inline std::string romanNumeral (const Chord& c)
{
    static const char* upper[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lower[] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    static const int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
    // Spelling for each semitone: degree index and accidental.
    static const int degreeFor[] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };

    const int off = ((c.offset % 12) + 12) % 12;
    const int deg = degreeFor[off];
    const int diff = off - majorSteps[deg];
    const auto& info = chordTypeInfo (c.type);

    std::string s = diff < 0 ? "b" : (diff > 0 ? "#" : "");
    s += info.minorNumeral ? lower[deg] : upper[deg];
    s += info.romanSuffix;
    return s;
}

// Reads one chord written as a roman numeral, such as "vi9", "bVIImaj7",
// "IV/2" (2 beats) or "iio7". Upper case is major, lower case is minor.
// Returns false if it cannot read it.
inline bool parseRoman (const std::string& token, Chord& out, double defaultBeats = 4.0)
{
    std::string t = token;
    double beats = defaultBeats;

    if (auto slash = t.find ('/'); slash != std::string::npos)
    {
        beats = std::atof (t.c_str() + slash + 1);
        t = t.substr (0, slash);
    }

    size_t pos = 0;
    int accidental = 0;
    while (pos < t.size() && (t[pos] == 'b' || t[pos] == '#'))
        accidental += (t[pos++] == 'b' ? -1 : 1);

    static const char* numerals[] = { "vii", "vi", "iv", "v", "iii", "ii", "i" };
    static const int numeralDegree[] = { 6, 5, 3, 4, 2, 1, 0 };
    static const int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };

    int degree = -1;
    bool isUpper = false;
    for (int i = 0; i < 7 && degree < 0; ++i)
    {
        const std::string n = numerals[i];
        if (t.size() - pos < n.size())
            continue;

        bool allLower = true, allUpper = true;
        for (size_t k = 0; k < n.size(); ++k)
        {
            const char ch = t[pos + k];
            allLower &= ch == n[k];
            allUpper &= ch == (char) std::toupper ((unsigned char) n[k]);
        }
        if (allLower || allUpper)
        {
            degree = numeralDegree[i];
            isUpper = allUpper;
            pos += n.size();
        }
    }
    if (degree < 0)
        return false;

    const std::string suffix = t.substr (pos);
    struct Map { const char* s; int type; };
    static const Map upperMap[] = { { "", Major }, { "maj7", Maj7 }, { "7", Dom7 }, { "6", Six }, { "add9", Add9 },
                                    { "maj9", Maj9 }, { "9", Dom9 }, { "sus2", Sus2 }, { "sus4", Sus4 },
                                    { "7sus4", Dom7Sus4 }, { "+", Aug }, { "5", Power }, { "69", SixNine },
                                    { "7b9", Dom7b9 }, { "+maj7", AugMaj7 } };
    static const Map lowerMap[] = { { "", Minor }, { "7", Min7 }, { "6", Min6 }, { "add9", MinAdd9 }, { "9", Min9 },
                                    { "11", Min11 }, { "o", Dim }, { "o7", Dim7 }, { "h7", HalfDim7 },
                                    { "h9", HalfDim9 }, { "maj7", MinMaj7 }, { "7b9", Min7b9 } };

    int type = -1;
    if (isUpper)
    {
        for (const auto& m : upperMap)
            if (suffix == m.s)
                type = m.type;
    }
    else
    {
        for (const auto& m : lowerMap)
            if (suffix == m.s)
                type = m.type;
    }
    if (type < 0)
        return false;

    out = {};
    out.offset = ((majorSteps[degree] + accidental) % 12 + 12) % 12;
    out.type = type;
    out.beats = beats > 0 ? beats : defaultBeats;
    return true;
}

inline std::vector<Chord> parseProgression (const std::string& text, double defaultBeats = 4.0)
{
    std::vector<Chord> chords;
    size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && text[i] == ' ')
            ++i;
        size_t j = i;
        while (j < text.size() && text[j] != ' ')
            ++j;
        if (j > i)
        {
            Chord c;
            if (parseRoman (text.substr (i, j - i), c, defaultBeats))
                chords.push_back (c);
        }
        i = j;
    }
    return chords;
}

//==============================================================================
// Voicing

enum Spread { Close, Open, Wide, numSpreads };

struct VoicingOptions
{
    int spread = Close;
    bool smooth = true;   // pick inversions so chords move as little as possible
    bool bassNote = false; // add the root an octave or two below
    int octave = 0;        // moves every chord, -3..3
    int invert = 0;        // moves every chord up (+) or down (-) by inversions, -4..4
};

// The whole-progression octave and inversion controls, applied to one voiced chord.
inline void applyGlobalVoicing (std::vector<int>& notes, const VoicingOptions& opt)
{
    if (notes.empty())
        return;
    std::sort (notes.begin(), notes.end());
    for (int i = 0; i < opt.invert; ++i)
    {
        notes.front() += 12;
        std::sort (notes.begin(), notes.end());
    }
    for (int i = 0; i > opt.invert; --i)
    {
        notes.back() -= 12;
        std::sort (notes.begin(), notes.end());
    }
    for (auto& n : notes)
        n += 12 * opt.octave;
}

// Notes of one chord, lowest first, with the root placed between C3 (48) and B3 (59).
inline std::vector<int> voiceChord (int rootPc, int type, int inversion, int octave, int spread)
{
    const auto& info = chordTypeInfo (type);
    std::vector<int> notes;
    const int root = 48 + ((rootPc % 12) + 12) % 12;
    for (int i = 0; i < info.count; ++i)
        notes.push_back (root + info.intervals[(size_t) i]);

    const int inv = ((inversion % info.count) + info.count) % info.count;
    for (int i = 0; i < inv; ++i)
        notes[(size_t) i] += 12;
    std::sort (notes.begin(), notes.end());

    if (spread == Open && notes.size() >= 3)
    {
        notes[notes.size() - 2] -= 12; // drop the second-highest note
        std::sort (notes.begin(), notes.end());
    }
    else if (spread == Wide && notes.size() >= 3)
    {
        notes.front() -= 12;
        notes[1] += 12;
        std::sort (notes.begin(), notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    }

    for (auto& n : notes)
        n += 12 * octave;
    return notes;
}

inline double voiceLeadingCost (const std::vector<int>& from, const std::vector<int>& to)
{
    double cost = 0;
    for (int a : to)
    {
        int best = 127;
        for (int b : from)
            best = std::min (best, std::abs (a - b));
        cost += best;
    }
    for (int b : from)
    {
        int best = 127;
        for (int a : to)
            best = std::min (best, std::abs (a - b));
        cost += 0.5 * best;
    }

    // Keep chords in a comfortable middle range.
    double mean = 0;
    for (int a : to)
        mean += a;
    mean /= (double) std::max<size_t> (1, to.size());
    if (mean < 54)
        cost += 2.0 * (54 - mean);
    if (mean > 70)
        cost += 2.0 * (mean - 70);
    return cost;
}

// Voices every chord of a progression. With smooth voicing, each chord after
// the first takes the inversion and octave closest to the chord before it, and
// the chord's own octave setting moves it from there.
inline std::vector<std::vector<int>> voiceProgression (const std::vector<Chord>& chords, int keyPc, const VoicingOptions& opt)
{
    std::vector<std::vector<int>> out;
    std::vector<int> prev;

    for (size_t i = 0; i < chords.size(); ++i)
    {
        const auto& c = chords[i];
        const int rootPc = keyPc + c.offset;

        if (! opt.smooth || i == 0)
        {
            out.push_back (voiceChord (rootPc, c.type, c.inversion, c.octave, opt.spread));
            prev = voiceChord (rootPc, c.type, c.inversion, 0, opt.spread);
            continue;
        }

        const int n = chordTypeInfo (c.type).count;
        std::vector<int> best;
        double bestCost = 1e9;
        for (int oct = -1; oct <= 1; ++oct)
            for (int inv = 0; inv < n; ++inv)
            {
                auto v = voiceChord (rootPc, c.type, inv, oct, opt.spread);
                const double cost = voiceLeadingCost (prev, v);
                if (cost < bestCost - 1e-9)
                {
                    bestCost = cost;
                    best = v;
                }
            }

        prev = best;
        for (auto& note : best)
            note += 12 * c.octave;
        out.push_back (best);
    }

    for (auto& v : out)
        applyGlobalVoicing (v, opt);
    return out;
}

inline int bassNoteFor (int rootPc)
{
    return 36 + ((rootPc % 12) + 12) % 12; // C2..B2
}

//==============================================================================
// Rhythms

struct RhythmHit
{
    double start, length;
    float velocity;
};

enum ArpMode { ArpNone, ArpUp, ArpDown, ArpUpDown };

struct Rhythm
{
    const char* name;
    double patternBeats; // 0 = one hit for the whole chord
    int arp;
    std::vector<RhythmHit> hits;
};

inline const std::vector<Rhythm>& rhythms()
{
    static const std::vector<Rhythm> list = []
    {
        std::vector<Rhythm> r;
        r.push_back ({ "Sustain", 0, ArpNone, { { 0, 1.0e9, 1.0f } } });
        r.push_back ({ "Half notes", 2, ArpNone, { { 0, 1.9, 1.0f } } });
        r.push_back ({ "Quarter notes", 1, ArpNone, { { 0, 0.85, 1.0f } } });
        r.push_back ({ "House offbeat stabs", 1, ArpNone, { { 0.5, 0.25, 1.0f } } });
        r.push_back ({ "Deep house groove", 4, ArpNone,
                      { { 0, 0.4, 1.0f }, { 0.75, 0.25, 0.75f }, { 1.5, 0.4, 0.9f },
                        { 2.25, 0.25, 0.7f }, { 2.75, 0.25, 0.75f }, { 3.5, 0.4, 0.85f } } });
        r.push_back ({ "Pumping eighths", 1, ArpNone, { { 0, 0.35, 0.6f }, { 0.5, 0.45, 1.0f } } });
        r.push_back ({ "Trance gate 16ths", 1, ArpNone,
                      { { 0, 0.17, 1.0f }, { 0.25, 0.17, 0.7f }, { 0.5, 0.17, 0.85f }, { 0.75, 0.17, 0.7f } } });
        r.push_back ({ "Charleston", 4, ArpNone, { { 0, 1.0, 1.0f }, { 1.5, 2.5, 0.85f } } });
        r.push_back ({ "Future bass swells", 4, ArpNone,
                      { { 0, 0.7, 1.0f }, { 0.75, 0.7, 0.8f }, { 1.5, 0.95, 0.9f }, { 2.5, 0.45, 0.75f }, { 3.0, 1.0, 0.85f } } });
        r.push_back ({ "Afro groove", 4, ArpNone,
                      { { 0, 0.5, 1.0f }, { 0.75, 0.4, 0.75f }, { 1.5, 0.4, 0.9f },
                        { 2.0, 0.5, 1.0f }, { 2.75, 0.4, 0.75f }, { 3.5, 0.4, 0.85f } } });
        r.push_back ({ "Piano ballad", 4, ArpNone, { { 0, 1.5, 1.0f }, { 1.5, 0.5, 0.7f }, { 2.0, 2.0, 0.85f } } });

        Rhythm up { "Arp up (8ths)", 4, ArpUp, {} };
        for (int i = 0; i < 8; ++i)
            up.hits.push_back ({ i * 0.5, 0.45, i % 2 == 0 ? 1.0f : 0.8f });
        r.push_back (up);

        Rhythm down { "Arp down (8ths)", 4, ArpDown, up.hits };
        r.push_back (down);

        Rhythm upDown { "Arp up-down (16ths)", 4, ArpUpDown, {} };
        for (int i = 0; i < 16; ++i)
            upDown.hits.push_back ({ i * 0.25, 0.22, i % 4 == 0 ? 1.0f : 0.75f });
        r.push_back (upDown);
        return r;
    }();
    return list;
}

//==============================================================================
// Sequence: a progression turned into notes and timed hits

struct Sequence
{
    static constexpr int maxSlots = 64, maxNotes = 12, maxHits = 4096;

    struct Slot
    {
        int notes[maxNotes] {};
        int count = 0;
        double start = 0, length = 0;
    };

    struct Hit
    {
        double start = 0, end = 0;
        float velocity = 1.0f;
        int slot = 0;
        int arpStep = -1; // -1 = the whole chord
    };

    Slot slots[maxSlots];
    int numSlots = 0;
    Hit hits[maxHits];
    int numHits = 0;
    double totalBeats = 0;
    int arpMode = ArpNone;

    // Notes that one hit plays. Returns how many were written to out.
    int notesFor (const Hit& h, int* out) const
    {
        const auto& s = slots[h.slot];
        if (s.count == 0)
            return 0;

        if (h.arpStep < 0 || arpMode == ArpNone)
        {
            std::copy (s.notes, s.notes + s.count, out);
            return s.count;
        }

        // Arpeggios skip the added bass note: notes are sorted, the bass is the lowest.
        const int first = (s.count > 1 && s.notes[1] - s.notes[0] >= 12) ? 1 : 0;
        const int n = s.count - first;
        int idx = h.arpStep % n;
        if (arpMode == ArpDown)
            idx = n - 1 - idx;
        else if (arpMode == ArpUpDown && n > 1)
        {
            const int period = 2 * n - 2;
            const int i = h.arpStep % period;
            idx = i < n ? i : period - i;
        }
        out[0] = s.notes[first + idx];
        return 1;
    }

    int slotAt (double loopPos) const
    {
        for (int i = 0; i < numSlots; ++i)
            if (loopPos >= slots[i].start && loopPos < slots[i].start + slots[i].length)
                return i;
        return -1;
    }
};

struct RenderOptions
{
    int keyPc = 0;
    VoicingOptions voicing;
    int rhythm = 0;
};

inline Sequence render (const std::vector<Chord>& chords, const RenderOptions& opt)
{
    Sequence seq;
    const auto voiced = voiceProgression (chords, opt.keyPc, opt.voicing);
    const auto& rhythm = rhythms()[(size_t) std::clamp (opt.rhythm, 0, (int) rhythms().size() - 1)];
    seq.arpMode = rhythm.arp;

    double t = 0;
    for (size_t i = 0; i < chords.size() && seq.numSlots < Sequence::maxSlots; ++i)
    {
        auto& slot = seq.slots[seq.numSlots];
        std::vector<int> notes = voiced[i];
        if (opt.voicing.bassNote)
        {
            const int bass = bassNoteFor (opt.keyPc + chords[i].offset) + 12 * opt.voicing.octave;
            if (std::find (notes.begin(), notes.end(), bass) == notes.end())
                notes.insert (notes.begin(), bass);
        }
        for (int n : notes)
            if (slot.count < Sequence::maxNotes && n >= 0 && n < 128)
                slot.notes[slot.count++] = n;

        slot.start = t;
        slot.length = std::max (0.25, chords[i].beats);
        const double end = slot.start + slot.length;

        int step = 0;
        if (rhythm.patternBeats <= 0)
        {
            if (seq.numHits < Sequence::maxHits)
                seq.hits[seq.numHits++] = { slot.start, end, rhythm.hits[0].velocity, seq.numSlots, -1 };
        }
        else
        {
            for (double p = slot.start; p < end - 1e-9; p += rhythm.patternBeats)
                for (const auto& h : rhythm.hits)
                {
                    const double hs = p + h.start;
                    if (hs >= end - 1e-9 || seq.numHits >= Sequence::maxHits)
                        continue;
                    seq.hits[seq.numHits++] = { hs, std::min (end, hs + h.length), h.velocity, seq.numSlots,
                                                rhythm.arp == ArpNone ? -1 : step };
                    ++step;
                }
        }

        t = end;
        ++seq.numSlots;
    }
    seq.totalBeats = t;
    return seq;
}

//==============================================================================
// Player: turns the sequence into note ons and offs, looping it.

class Player
{
public:
    // out (sampleOffset, note, velocity 0..1, isNoteOn)
    template <typename Out>
    void process (const Sequence& seq, double ppqStart, double beatsPerSample, int numSamples, bool jumped, Out&& out)
    {
        if (seq.totalBeats <= 0 || seq.numHits == 0 || numSamples <= 0 || beatsPerSample <= 0)
        {
            stop (out, 0);
            lastSlot = -1;
            return;
        }

        const double total = seq.totalBeats;
        const double ppqEnd = ppqStart + beatsPerSample * numSamples;
        auto toSample = [&] (double beat)
        { return std::clamp ((int) std::floor ((beat - ppqStart) / beatsPerSample + 0.5), 0, numSamples - 1); };

        if (jumped)
        {
            stop (out, 0);

            // Starting in the middle of a hit: play it straight away.
            const double cycle = std::floor (ppqStart / total);
            const double pos = ppqStart - cycle * total;
            for (int i = 0; i < seq.numHits; ++i)
            {
                const auto& h = seq.hits[i];
                if (h.start < pos && h.end > pos + 1.0e-3)
                {
                    start (seq, h, cycle * total, 0, out);
                    break;
                }
            }
        }

        // Every hit that starts inside this block, in time order.
        struct Due { double at; int hit; double cycleStart; };
        Due due[64];
        int numDue = 0;

        for (double cycle = std::floor (ppqStart / total); cycle * total < ppqEnd && numDue < 64; cycle += 1.0)
        {
            const double base = cycle * total;
            for (int i = 0; i < seq.numHits && numDue < 64; ++i)
            {
                const double at = base + seq.hits[i].start;
                if (at >= ppqStart && at < ppqEnd)
                    due[numDue++] = { at, i, base };
            }
        }

        for (int d = 0; d < numDue; ++d)
        {
            if (numActive > 0 && activeEnd <= due[d].at)
                stop (out, toSample (activeEnd));
            stop (out, toSample (due[d].at));
            start (seq, seq.hits[due[d].hit], due[d].cycleStart, toSample (due[d].at), out);
        }

        if (numActive > 0 && activeEnd < ppqEnd)
            stop (out, toSample (activeEnd));

        const double posEnd = ppqEnd - std::floor (ppqEnd / total) * total;
        lastSlot = seq.slotAt (posEnd);
    }

    template <typename Out>
    void stop (Out&& out, int sampleOffset)
    {
        for (int i = 0; i < numActive; ++i)
            out (sampleOffset, active[i], 0.0f, false);
        numActive = 0;
    }

    int currentSlot() const noexcept { return lastSlot; }

private:
    template <typename Out>
    void start (const Sequence& seq, const Sequence::Hit& h, double cycleStart, int sampleOffset, Out&& out)
    {
        numActive = seq.notesFor (h, active);
        activeEnd = cycleStart + h.end;
        for (int i = 0; i < numActive; ++i)
            out (sampleOffset, active[i], h.velocity, true);
    }

    int active[Sequence::maxNotes] {};
    int numActive = 0;
    double activeEnd = 0;
    int lastSlot = -1;
};

//==============================================================================
// Shuffle: writes a new progression that stays inside the key. Locked chords
// stay where they are. "adventure" runs from 0 (the most familiar moves and
// plain chords) to 1 (unexpected moves and richer chords).

// True when every note of the chord is in the scale.
inline bool chordInScale (int scaleIndex, int offset, int type)
{
    const auto& info = chordTypeInfo (type);
    for (int i = 0; i < info.count; ++i)
        if (degreeOfOffset (scaleIndex, offset + info.intervals[(size_t) i]) < 0)
            return false;
    return true;
}

inline bool isMinorFamily (int scaleIndex)
{
    // Minor, Dorian, Phrygian, Harmonic minor and Melodic minor have a minor tonic.
    return scaleDegreeOffset (scaleIndex, 2) == 3;
}

// How likely each scale degree is to follow another, for the familiar end of the slider.
inline double degreeTransition (bool minor, int from, int to)
{
    static const double majorTable[7][7] = {
        // to:  I    ii   iii  IV   V    vi   vii
        { 0.3, 2.0, 1.0, 4.0, 4.0, 4.0, 0.2 }, // from I
        { 0.5, 0.2, 0.3, 1.0, 5.0, 0.5, 0.5 }, // from ii
        { 0.5, 1.0, 0.2, 3.0, 0.5, 4.0, 0.1 }, // from iii
        { 3.0, 1.5, 0.5, 0.2, 4.0, 1.5, 0.3 }, // from IV
        { 5.0, 0.3, 0.5, 1.5, 0.2, 3.0, 0.1 }, // from V
        { 1.0, 3.0, 1.0, 4.0, 2.0, 0.2, 0.1 }, // from vi
        { 4.0, 0.2, 1.0, 0.3, 0.5, 1.0, 0.1 }, // from vii
    };
    static const double minorTable[7][7] = {
        // to:  i    ii   III  iv   v    VI   VII
        { 0.3, 0.5, 2.0, 3.5, 1.5, 4.0, 3.5 }, // from i
        { 2.0, 0.1, 0.3, 0.5, 4.0, 0.5, 0.5 }, // from ii
        { 1.5, 0.3, 0.2, 2.0, 0.5, 3.5, 3.0 }, // from III
        { 3.0, 0.5, 1.0, 0.2, 2.5, 2.0, 2.5 }, // from iv
        { 4.0, 0.2, 0.5, 1.0, 0.2, 2.5, 1.0 }, // from v
        { 1.5, 0.5, 2.5, 2.0, 1.0, 0.2, 4.5 }, // from VI
        { 4.0, 0.1, 2.5, 1.0, 0.5, 2.0, 0.2 }, // from VII
    };
    from = ((from % 7) + 7) % 7;
    to = ((to % 7) + 7) % 7;
    return minor ? minorTable[from][to] : majorTable[from][to];
}

// Chord types that fit on one degree, with how likely each is at this adventure level.
inline std::vector<std::pair<int, double>> chordChoices (int scaleIndex, int degree, double adventure)
{
    const int offset = scaleDegreeOffset (scaleIndex, degree);
    const double a = std::clamp (adventure, 0.0, 1.0);
    std::vector<std::pair<int, double>> out;
    auto add = [&] (int type, double weight)
    {
        if (weight <= 0 || ! chordInScale (scaleIndex, offset, type))
            return;
        for (const auto& o : out)
            if (o.first == type)
                return;
        out.push_back ({ type, weight });
    };

    add (diatonicChordType (scaleIndex, degree, Triads), 3.0 * (1.0 - a) + 0.4);
    add (diatonicChordType (scaleIndex, degree, Sevenths), 0.6 + 1.6 * a);
    add (diatonicChordType (scaleIndex, degree, Ninths), 2.4 * a * a);
    add (Sus2, 1.2 * a);
    add (Sus4, 1.2 * a);
    add (Add9, 1.4 * a);
    add (MinAdd9, 1.4 * a);
    add (Min11, 1.2 * a * a);
    add (Six, 0.6 * a);
    add (Min6, 0.6 * a);
    add (Dom7Sus4, 0.8 * a * a);
    if (out.empty())
        out.push_back ({ diatonicChordType (scaleIndex, degree, Triads), 1.0 });
    return out;
}

struct ShuffleOptions
{
    int scale = 0;
    double adventure = 0.3;
    double totalBeats = 16.0; // up to 32 bars = 128 beats
    double chordBeats = 4.0;  // how often the chord changes
    uint32_t seed = 1;
};

inline std::vector<Chord> shuffleProgression (const std::vector<Chord>& current, const ShuffleOptions& opt)
{
    std::mt19937 rng (opt.seed);
    auto pick = [&rng] (const std::vector<double>& weights)
    {
        double sum = 0;
        for (double w : weights)
            sum += w;
        double r = std::uniform_real_distribution<double> (0.0, sum) (rng);
        for (size_t i = 0; i < weights.size(); ++i)
        {
            r -= weights[i];
            if (r <= 0)
                return (int) i;
        }
        return (int) weights.size() - 1;
    };

    const double a = std::clamp (opt.adventure, 0.0, 1.0);
    const bool minor = isMinorFamily (opt.scale);
    const double chordBeats = std::max (1.0, opt.chordBeats);
    const int count = std::clamp ((int) std::lround (opt.totalBeats / chordBeats), 1, (int) Sequence::maxSlots);

    // Start from the grid, keeping locked chords at their positions.
    std::vector<Chord> out ((size_t) count);
    std::vector<bool> locked ((size_t) count, false);
    for (int i = 0; i < count; ++i)
    {
        out[(size_t) i].beats = chordBeats;
        if (i < (int) current.size() && current[(size_t) i].locked)
        {
            out[(size_t) i] = current[(size_t) i];
            out[(size_t) i].beats = chordBeats; // keeps its chord, follows the new grid
            locked[(size_t) i] = true;
        }
    }

    auto degreeOf = [&] (const Chord& c)
    {
        const int d = degreeOfOffset (opt.scale, c.offset);
        return d < 0 ? 0 : d;
    };

    int prev = -1;
    for (int i = 0; i < count; ++i)
    {
        if (locked[(size_t) i])
        {
            prev = degreeOf (out[(size_t) i]);
            continue;
        }

        // The next locked chord, if it comes right after this one.
        const int nextLocked = (i + 1 < count && locked[(size_t) i + 1]) ? degreeOf (out[(size_t) i + 1]) : -1;
        const bool last = i == count - 1;
        const int first = locked[0] ? degreeOf (out[0]) : 0;

        std::vector<double> weights (7);
        for (int d = 0; d < 7; ++d)
        {
            double w;
            if (prev < 0)
            {
                // Familiar progressions start at home; adventurous ones anywhere.
                w = d == 0 ? 6.0 * (1.0 - a) + 1.0 : 0.3 + 1.5 * a;
            }
            else
            {
                const double familiar = degreeTransition (minor, prev, d);
                w = (1.0 - a) * familiar + a * (d == prev ? 0.15 : 1.5);
                // Adventurous: favour the moves the familiar table rarely makes.
                if (a > 0.5 && familiar < 1.0 && d != prev)
                    w += (a - 0.5) * 2.0;
            }

            if (nextLocked >= 0)
                w *= degreeTransition (minor, d, nextLocked) * (1.0 - a) + 1.0 * a + 0.2;
            if (last && count > 1)
                w *= degreeTransition (minor, d, first) * (1.0 - a) * 0.5 + 1.0;
            if (diatonicChordType (opt.scale, d, Triads) == Dim)
                w *= 0.1 + 0.4 * a; // diminished chords are rare in pop and dance music
            weights[(size_t) d] = std::max (w, 0.0);
        }

        const int degree = pick (weights);
        const auto choices = chordChoices (opt.scale, degree, a);
        std::vector<double> typeWeights;
        for (const auto& c : choices)
            typeWeights.push_back (c.second);

        Chord c;
        c.offset = scaleDegreeOffset (opt.scale, degree);
        c.type = choices[(size_t) pick (typeWeights)].first;
        c.beats = chordBeats;
        out[(size_t) i] = c;
        prev = degree;
    }
    return out;
}

//==============================================================================
// One key plays a whole chord: notes in the scale play the chord built on
// that degree. Returns offsets from the played note (empty = outside the scale).

inline std::vector<int> keyChordOffsets (int keyPc, int scaleIndex, int size, const VoicingOptions& opt, int playedNote)
{
    const int degree = degreeOfOffset (scaleIndex, playedNote - keyPc);
    if (degree < 0)
        return {};

    const int type = diatonicChordType (scaleIndex, degree, size);
    auto notes = voiceChord (0, type, 0, 0, opt.spread);
    applyGlobalVoicing (notes, opt);
    std::vector<int> offsets;
    for (int n : notes)
        offsets.push_back (n - 48);
    if (opt.bassNote)
        offsets.insert (offsets.begin(), -12 + 12 * opt.octave);
    return offsets;
}
} // namespace cruzchords
