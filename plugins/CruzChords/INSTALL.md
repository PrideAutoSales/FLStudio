# Installing Cruz Chords on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side (or go to `/releases` after the repo address).
2. Under **Latest build**, click **CruzChords-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzChords-mac.zip**. You will get a folder called **CruzChords-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzChords-mac/install.sh
   ```

3. It asks for your **Mac login password**, because the plugins go in `/Library/Audio/Plug-Ins`, the folder FL Studio scans. Type it and press Return. Nothing appears while you type, which is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**.
2. Click **Find installed plugins** (or **Find more plugins**) and wait for the scan to finish.
3. Cruz Chords is an instrument, like a synth. In the **Channel rack**, click **+** and choose **Cruz Chords** (use the VST3 version).

If Cruz Chords does not show up, check that the plugin manager's search paths include
`/Library/Audio/Plug-Ins/VST3`, then scan again.

## 4. Make chords

1. Pick a **Key** and **Scale** at the top. The seven pads under them are the chords that fit that key. Click one to hear it.
2. Pick a **Genre** and a **Progression**, or press **Surprise me**. Press **Play** to hear it loop.
3. Build your own: press **+ Add** on any pad. Click a chord in **Your progression** to change its root, type, length, inversion or octave. With a chord selected, clicking a pad swaps it in.
4. **Shuffle** writes a new progression in your key. Slide **Traditional ↔ Adventurous** to choose between familiar moves with plain chords and unexpected moves with richer chords. **Length** sets 1 to 32 bars and **Chord every** sets how often the chord changes.
5. Click the **padlock** on a chord (or select it and press **Lock**) to keep it when you shuffle.
6. **Whole progression: Octave and Invert** move every chord up or down at once.
7. **Feel** changes the rhythm (house stabs, deep house groove, pumping, arpeggios and more) and how the chords are spread out.

## 5. Use your own synth

1. Click the sound menu at the top right. It lists every VST3 plugin on your Mac. Pick a synth, such as **Cruz Synth** or Serum.
2. Click **Open synth** to open that synth's own window and change its sound. Cruz Chords remembers the synth and its sound when you save your FL project.
3. Pick **Built-in keys** or **Built-in pad** to go back to the simple sound inside Cruz Chords.

## 6. Put the chords in your song

Pick whichever suits you:

- **Drag it in:** drag the **Drag MIDI into FL Studio** box onto the playlist or onto a channel. You get the progression as notes you can edit in the piano roll.
- **Save it:** **Save MIDI file** puts a `.mid` file in your **Music > Cruz Chords** folder.
- **Let it play:** with **Play with FL** on, the progression starts and stops with FL Studio's play button and the space bar, and follows FL's tempo and bar position. Turn it off if you also dragged the MIDI into your song, so the chords don't play twice. The **Play** button still lets you hear it while FL is stopped.
- **One note per chord:** with **Keys play chords** on, each note you put in Cruz Chords' piano roll plays the whole chord for that note of the scale.

Advanced: Cruz Chords also sends its notes out as MIDI. To play a synth on another channel, open Cruz Chords'
wrapper settings (the gear icon), set **MIDI output port** to a number, and set the other synth's **MIDI input port** to the same number.

## Updating to a newer build

Repeat steps 1 and 2. If an older **CruzChords-mac** folder is already in Downloads,
delete it first so the new one keeps the same name. Restart FL Studio afterwards.
