# Installing Cruz FET on your Mac

Cruz FET is a FET peak compressor (the fast, punchy, aggressive kind). You only need to do this once per new build. No coding tools are needed.

## 1. Download

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest build**, click **CruzFET-mac.zip** to download it.
3. In Finder, open **Downloads** and double-click **CruzFET-mac.zip**. You get a folder called **CruzFET-mac**.

## 2. Install

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzFET-mac/install.sh
   ```

3. It asks for your Mac login password, because the plugin goes in `/Library/Audio/Plug-Ins`, the folder FL Studio scans. Type it and press Return. Nothing appears while you type, which is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. Open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
2. In the **Mixer**, click an empty effect slot and choose **Cruz FET**.

To update later, delete the old **CruzFET-mac** folder in Downloads, then repeat steps 1 and 2 and restart FL Studio.

## Quick guide

- **Input** pushes the signal into the compressor. More input means more compression.
- **Output** sets the final level.
- **Attack** and **Release** go from 1 (slowest) to 7 (fastest), like the hardware.
- **Ratio** 4, 8, 12, 20, or **ALL** for the "all buttons in" sound: very aggressive, great on drum rooms and parallel smash.
- **Mix** blends in the dry signal for parallel compression.
- The meter shows gain reduction (GR) or output level.

If the plugin does not show up, check that FL Studio's plugin search paths include `/Library/Audio/Plug-Ins/VST3`, then scan again.
