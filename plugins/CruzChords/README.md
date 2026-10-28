# Cruz Chords

A chord progression instrument for FL Studio 20 and up (VST3 and AU, plugin code `Crch`).

- Pick a key and scale and get the seven chords in it, as triads, 7ths or 9ths.
- 55 factory progressions in House, Deep House, EDM, Future Bass, Melodic Techno, Afro House, Pop, Hip Hop, R&B, Lo-fi and Trap.
  They are written as roman numerals in `Source/Presets.h`, so they play in any key.
- Edit every chord: root, type (27 chord types), length, inversion and octave. Smooth voice leading picks inversions automatically.
- 14 rhythms, from sustained chords to house stabs, deep house grooves, pumping eighths, trance gates and arpeggios.
- Plays through a built-in keys or pad sound, or any VST3 or AU synth loaded inside it (Cruz Synth included).
- Follows FL Studio's tempo and song position while the song plays.
- One key plays a chord: notes in the scale from the piano roll play the chord built on that note.
- Drag the progression into FL Studio as MIDI, or save a `.mid` file.
- Sends its notes out as MIDI too, for FL Studio's MIDI port routing.

Code: `Source/ChordEngine.h` (theory, voicing, rhythms, playback, no JUCE), `Source/Presets.h`,
`Source/BuiltInSynth.h`, `Source/PluginProcessor.*` (hosting, state, MIDI) and `Source/PluginEditor.*`.
Engine tests: `cmake --build build --target CruzChordsEngineTest && build/plugins/CruzChords/CruzChordsEngineTest`.

Install steps are in [INSTALL.md](INSTALL.md).
