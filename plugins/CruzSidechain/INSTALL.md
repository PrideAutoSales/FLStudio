# Installing Cruz Sidechain on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side (or go to `/releases` after the repo address).
2. Under **Latest build**, click **CruzSidechain-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzSidechain-mac.zip**. You will get a folder called **CruzSidechain-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzSidechain-mac/install.sh
   ```

3. It asks for your **Mac login password**, because the plugins go in `/Library/Audio/Plug-Ins`, the folder FL Studio scans. Type it and press Return. Nothing appears while you type, which is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**.
2. Click **Find installed plugins** (or **Find more plugins**) and wait for the scan to finish.
3. Open the **Mixer**, click an empty effect slot on any track, and choose **Cruz Sidechain**.

If Cruz Sidechain does not show up, check that the plugin manager's search paths include
`/Library/Audio/Plug-Ins/VST3`, then scan again.

## Updating to a newer build

Repeat steps 1 and 2. If an older **CruzSidechain-mac** folder is already in Downloads,
delete it first so the new one keeps the same name. Restart FL Studio afterwards.

## 4. Hook up the sidechain (kick ducks the bass)

1. Put **Cruz Sidechain** on the mixer track you want ducked, for example the bass.
2. Click the **kick's** mixer track to select it. Then right-click the small arrow at the
   bottom of the **bass** track and choose **Sidechain to this track**.
3. Open Cruz Sidechain and click **Sidechain** at the top. The lamp in the Waveforms panel
   turns green ("Sidechain signal coming in") when the kick arrives.
4. If the lamp stays orange, open the plugin's wrapper settings (the gear icon at the top
   left of the plugin window), go to the **Processing** tab, and set the plugin's second
   input (the sidechain input) to the kick's mixer track.

No routing needed for these two:

- **Self** ducks the track from its own sound (a normal compressor).
- **Tempo** plays a silent "ghost kick" on every 1/1, 1/2, 1/4, 1/8 or 1/16 note while
  FL Studio plays, for classic pumping without any kick routed in.
