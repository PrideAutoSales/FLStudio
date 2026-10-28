#pragma once

// Factory progressions, grouped by genre. Each one is written in roman
// numerals against the major scale (see parseRoman), so it plays in whatever
// key is picked. "/8" after a chord makes it 8 beats long (default 4).

#include "ChordEngine.h"

namespace cruzchords
{
enum RhythmIndex
{
    rSustain, rHalf, rQuarter, rHouseStabs, rDeepGroove, rPumping, rTranceGate, rCharleston,
    rFutureBass, rAfro, rBallad, rArpUp, rArpDown, rArpUpDown
};

enum ScaleIndex { sMajor, sMinor, sDorian, sPhrygian, sLydian, sMixolydian, sHarmonicMinor, sMelodicMinor };

struct Preset
{
    const char* genre;
    const char* name;
    int scale;
    int rhythm;
    int size; // chord size for the pads: Triads, Sevenths or Ninths
    const char* progression;
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        // House
        { "House", "Garage Lights", sMinor, rHouseStabs, Sevenths, "i7 bVImaj7 bIIImaj7 bVII" },
        { "House", "Piano Anthem", sMajor, rCharleston, Triads, "I V vi IV" },
        { "House", "Saturday Organ", sMinor, rHouseStabs, Sevenths, "i7 iv7 bVII7 bIIImaj7" },
        { "House", "Warehouse Sunrise", sMajor, rCharleston, Sevenths, "IVmaj7 V7sus4 iii7 vi7" },
        { "House", "Disco Strings", sMajor, rQuarter, Sevenths, "ii7 V7 Imaj7 vi7" },
        { "House", "Soulful Uplift", sMajor, rDeepGroove, Ninths, "IVmaj9 iii7 ii7 Imaj9" },
        { "House", "Classic Stab", sMinor, rHouseStabs, Sevenths, "i7/8 iv7/8" },
        { "House", "Club Jack", sMinor, rHouseStabs, Triads, "i7 bVII bVI bVII" },

        // Deep house
        { "Deep House", "Midnight Pool", sMinor, rDeepGroove, Ninths, "i9 bVImaj7 iv9 bVIIadd9" },
        { "Deep House", "Velvet Rooftop", sMinor, rDeepGroove, Ninths, "i11/8 iv9/8" },
        { "Deep House", "Late Night Drive", sMinor, rDeepGroove, Ninths, "i9 bIIImaj7 bVImaj9 v7" },
        { "Deep House", "Lagoon", sDorian, rDeepGroove, Ninths, "i9/8 IV9/8" },
        { "Deep House", "Smoke and Satin", sMinor, rDeepGroove, Ninths, "iv9 i9 bVImaj7 v9" },
        { "Deep House", "Sunday Dub", sMinor, rHouseStabs, Sevenths, "i7/8 bVImaj7/8" },
        { "Deep House", "Neon Swim", sMajor, rDeepGroove, Ninths, "ii9 iii7 IVmaj9 iii7" },
        { "Deep House", "Blue Hour", sMinor, rDeepGroove, Ninths, "i9 bVIImaj7 bVImaj7 v7" },

        // EDM
        { "EDM", "Festival Lift", sMinor, rPumping, Triads, "bVI bVII i i" },
        { "EDM", "Main Stage", sMajor, rPumping, Triads, "vi IV I V" },
        { "EDM", "Big Room Drop", sMinor, rPumping, Triads, "I5/8 bVI5 bVII5" },
        { "EDM", "Euphoria", sMinor, rTranceGate, Triads, "i bVI bIII bVII" },
        { "EDM", "Supersaw Hands", sMinor, rPumping, Triads, "bVIadd9 bVIIadd9 iadd9 iadd9" },
        { "EDM", "Progressive Glow", sMajor, rSustain, Triads, "I/2 V/2 vi/2 iii/2 IV/2 I/2 IV/2 V/2" },
        { "EDM", "Gate Runner", sMinor, rTranceGate, Triads, "i bVII bVI bVII" },
        { "EDM", "Sunset Anthem", sMajor, rPumping, Triads, "IV V vi vi" },

        // Future bass
        { "Future Bass", "Cotton Clouds", sMajor, rFutureBass, Ninths, "IVmaj9 iii7 vi9 Vadd9" },
        { "Future Bass", "Glass Hearts", sMinor, rFutureBass, Ninths, "bVImaj9 bVII6 i9 v7" },
        { "Future Bass", "Pastel Drop", sMajor, rFutureBass, Sevenths, "Imaj7 iii7 vi9 IVmaj7" },

        // Melodic techno
        { "Melodic Techno", "Afterhours", sMinor, rArpUpDown, Triads, "i/8 bVI/8" },
        { "Melodic Techno", "Cathedral", sMinor, rArpDown, Triads, "i bVII bVI v" },
        { "Melodic Techno", "Monolith", sPhrygian, rSustain, Triads, "i/8 bII/8" },

        // Afro house
        { "Afro House", "Savanna Night", sMinor, rAfro, Sevenths, "i7 iv7 bVII bIIImaj7" },
        { "Afro House", "Sun Dance", sDorian, rAfro, Sevenths, "i7 IV9" },
        { "Afro House", "Tribal Glow", sMinor, rAfro, Ninths, "i9 bVII bVImaj7 bVII" },

        // Pop
        { "Pop", "Four Chord Classic", sMajor, rQuarter, Triads, "I V vi IV" },
        { "Pop", "Sensitive", sMajor, rQuarter, Triads, "vi IV I V" },
        { "Pop", "Doo-wop", sMajor, rBallad, Triads, "I vi IV V" },
        { "Pop", "Radio Ballad", sMajor, rBallad, Triads, "I iii vi IV ii V I I" },
        { "Pop", "Bittersweet", sMajor, rQuarter, Triads, "IV I V vi" },
        { "Pop", "Royal Road", sMajor, rBallad, Sevenths, "IVmaj7 V7 iii7 vi" },

        // Hip hop
        { "Hip Hop", "Boom Bap Jazz", sMinor, rSustain, Sevenths, "iih7 V7 i7 i7" },
        { "Hip Hop", "Smooth Cruise", sMajor, rBallad, Sevenths, "IVmaj7 iii7 ii7 Imaj7" },
        { "Hip Hop", "Dusty Keys", sMinor, rSustain, Ninths, "i9 iv9 v7 i9" },
        { "Hip Hop", "West Coast", sDorian, rQuarter, Sevenths, "i7 IV9" },

        // R&B and neo soul
        { "R&B / Soul", "Velour", sMajor, rBallad, Ninths, "IVmaj9 iii7 vi9 ii9 V7sus4" },
        { "R&B / Soul", "Silk Sheets", sMajor, rSustain, Ninths, "ii9 V9 Imaj9 vi9" },
        { "R&B / Soul", "Late Text", sMinor, rBallad, Ninths, "i9 bVImaj7 iv9 V7b9" },
        { "R&B / Soul", "Gold Chain", sMajor, rSustain, Sevenths, "Imaj7 vi9 ii9 V7sus4" },
        { "R&B / Soul", "Gospel Turn", sMajor, rBallad, Sevenths, "IV/2 iii7/2 vi7/2 ii7/2 V7sus4/2 V7/2 Imaj7" },

        // Lo-fi
        { "Lo-fi", "Rainy Window", sMajor, rBallad, Sevenths, "IVmaj7 iii7 vi7 Imaj7" },
        { "Lo-fi", "Cassette", sMajor, rSustain, Ninths, "ii9 V7 Imaj9 vi9" },
        { "Lo-fi", "Study Session", sMinor, rBallad, Ninths, "i9 iv9 bVIImaj7 bIIImaj7" },
        { "Lo-fi", "Tea Steam", sMajor, rSustain, Ninths, "Imaj9 IVmaj9 iii7 vi7" },

        // Trap
        { "Trap", "Dark Bells", sHarmonicMinor, rArpUpDown, Triads, "i iv bVI V" },
        { "Trap", "Night Mode", sMinor, rSustain, Triads, "i bVI iv v" },
        { "Trap", "Menace", sPhrygian, rSustain, Triads, "i bII i bVII" },
    };
    return list;
}

inline std::vector<std::string> presetGenres()
{
    std::vector<std::string> genres;
    for (const auto& p : presets())
        if (std::find (genres.begin(), genres.end(), p.genre) == genres.end())
            genres.push_back (p.genre);
    return genres;
}
} // namespace cruzchords
