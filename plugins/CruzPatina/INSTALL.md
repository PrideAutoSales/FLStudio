# Cruz Patina

Cruz Patina adds vintage and lo-fi colour: noise, tape wobble, distortion, digital
crushing, a small reverb and worn-tape effects, all scaled by one **Magnitude** knob.

## Install it on your Mac

1. Open the repo on GitHub, click **Releases**, and under **Latest build** download
   **CruzPatina-mac.zip**.
2. In Finder, open **Downloads** and double-click the zip. You get a folder called **CruzPatina-mac**.
3. Open **Terminal** (Cmd + Space, type `Terminal`, press Return), paste this line and press Return:

   ```
   bash ~/Downloads/CruzPatina-mac/install.sh
   ```

4. It asks for your **Mac login password**, because it puts the plugin in `/Library/Audio/Plug-Ins`,
   the folder FL Studio scans. Nothing appears while you type, which is normal. Press Return.
5. When it says **Done**, open FL Studio, go to **Options > Manage plugins**,
   click **Find installed plugins**, then load **Cruz Patina** on a mixer track.
   If it doesn't show up, check that the plugin search paths include `/Library/Audio/Plug-Ins/VST3`.

To update later, delete the old **CruzPatina-mac** folder from Downloads, repeat the steps,
and restart FL Studio.

## What the controls do

- **Magnitude** turns every module up or down together. Start at 100% and pull it back to taste.
- **Noise**: Hiss (tape), Vinyl (crackle, pops and rumble) or Hum. **Tone** brightens it,
  **Follow** makes the noise duck when the music is quiet.
- **Wobble**: **Wow** is slow pitch drift, **Rate** is how fast it drifts, **Flutter** is fast
  shimmer, **Stereo** makes left and right wobble differently.
- **Distort**: Tube, Tape, Fuzz or Hard clipping. **Tone** darkens it, **Mix** blends it in.
- **Digital**: lowers the sample **Rate** and the **Bits** for a crunchy sampler sound.
- **Space**: a small room through to a long dark hall. **Size**, **Decay** and **Mix**.
- **Magnetic**: **Saturate** adds tape warmth and low-end bump, **Wear** dulls the top end and
  makes the level drift, **Dropouts** adds moments where the tape loses contact.
- **Input**, **Tone** (tilts dark or bright), **Width**, **Mix** (dry/wet) and **Output**.

Click a module's round light to switch it on or off. Double-click any knob to reset it.
Presets are in the menu at the top.

The plugin delays the sound by about 6 ms, and FL Studio compensates for that automatically.
