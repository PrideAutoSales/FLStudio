# Installing Cruz Opto on your Mac

Cruz Opto is an optical leveling compressor (the smooth, slow, "set it and forget it" kind). You only need to do this once per new build. No coding tools are needed.

## 1. Download

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest build**, click **CruzOpto-mac.zip** to download it.
3. In Finder, open **Downloads** and double-click **CruzOpto-mac.zip**. You get a folder called **CruzOpto-mac**.

## 2. Install

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzOpto-mac/install.sh
   ```

3. It asks for your Mac login password, because the plugin goes in `/Library/Audio/Plug-Ins`, the folder FL Studio scans. Type it and press Return. Nothing appears while you type, which is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. Open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
2. In the **Mixer**, click an empty effect slot and choose **Cruz Opto**.

To update later, delete the old **CruzOpto-mac** folder in Downloads, then repeat steps 1 and 2 and restart FL Studio.

## Quick guide

- **Peak Reduction** sets how much it compresses. Turn it up until the meter shows 3 to 6 dB on vocals or bass.
- **Gain** brings the level back up after compressing.
- **Compress / Limit**: Compress is gentle (about 3:1). Limit holds peaks more firmly.
- **HF Emphasis** makes it react more to bright sounds like sibilance and cymbals.
- **Mix** blends in the uncompressed signal for parallel compression.
- The meter shows gain reduction (GR) or output level.

If the plugin does not show up, check that FL Studio's plugin search paths include `/Library/Audio/Plug-Ins/VST3`, then scan again.
