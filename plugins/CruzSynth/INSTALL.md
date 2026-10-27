# Installing Cruz Synth on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest builds**, click **CruzSynth-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzSynth-mac.zip**. You will get a folder called **CruzSynth-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzSynth-mac/install.sh
   ```

3. When it says **Cruz Synth is installed**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
2. Cruz Synth is an instrument, not an effect. Open the **Channel rack**, click the **+** at the bottom, and choose **Cruz Synth** (or drag it from the plugin browser into the channel rack).
3. Open the piano roll for that channel and draw some notes, or click the keys at the bottom of the Cruz Synth window to hear it.

The **GLOBAL** page inside the synth has a short guide to every section.

## Updating to a newer build

Delete the old **CruzSynth-mac** folder in Downloads, then repeat steps 1 and 2 and restart FL Studio.
Your songs keep their Cruz Synth settings, and presets you saved stay in **Music > Cruz Synth Presets**.
