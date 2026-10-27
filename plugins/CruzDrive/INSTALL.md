# Cruz Drive

Analog-style saturation with a crush compressor. Use it to add warmth and
grit to vocals, drums, bass, synths and buses.

## Install on your Mac

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest build**, click **CruzDrive-mac.zip** to download it.
3. In Finder, open **Downloads** and double-click **CruzDrive-mac.zip**. You get a folder called **CruzDrive-mac**.
4. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return), paste this line and press Return:

   ```
   bash ~/Downloads/CruzDrive-mac/install.sh
   ```

5. When it says **Cruz Drive is installed**, close Terminal.
6. In FL Studio, open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
7. In the **Mixer**, click an empty effect slot and choose **Cruz Drive**.

To update later, delete the old **CruzDrive-mac** folder in Downloads, repeat
steps 1 to 5, and restart FL Studio.

## The controls

**Character (left)**
- **Warm, Glass, Iron, Valve, Fuzz** pick the saturation style. The display shows the curve your signal goes through, and the white dot shows where your audio is hitting it.
  - **Warm**: smooth and round, like tape.
  - **Glass**: open and bright, like a clean console preamp pushed hard.
  - **Iron**: thick lows, like a transformer. Great on bass and kick.
  - **Valve**: lopsided, rich and forward, like a tube stage.
  - **Fuzz**: aggressive and buzzy.

**Drive & Tone (middle)**
- **Drive** sets how hard the signal hits the saturation.
- **Punish** adds 20 dB more drive for extreme distortion.
- **Low Cut** removes lows before the distortion, so bass does not get muddy. **Thump** adds a bump right at the cutoff.
- **Tone** tilts the sound darker (left) or brighter (right) going into the distortion.
- **High Cut** rolls off harsh highs after the distortion. **Steep** makes the roll-off sharper.

**Crush (top right)**
- **Crush** squashes the signal with a very fast, heavy compressor before it is distorted. Turn it up for smashed drums and rooms. The **GR** bar shows how much it is squashing.
- **Darkness** rolls the top end off after everything, from a little to very muffled.

**Output (bottom right)**
- **Mix** blends the distorted sound with the original. Around 30 to 60% is great for "parallel" distortion that keeps the punch.
- **Output** sets the final level.
- **Auto Gain** (on by default) keeps the volume about the same as you turn up Drive or Crush, so you hear the tone change and not just "louder".

**Presets** are in the menu at the top right, and FL Studio's own preset menu shows them too.

Tip: double-click any knob to reset it.
