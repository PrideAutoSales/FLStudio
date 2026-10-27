# Installing Cruz EQ on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side (or go to `/releases` after the repo address).
2. Under **Latest build**, click **CruzEQ-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzEQ-mac.zip**. You will get a folder called **CruzEQ-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzEQ-mac/install.sh
   ```

3. It asks for your **Mac password** (the one you use to log in), because the
   plugins go in the Mac's main plugin folder, `/Library/Audio/Plug-Ins`, which is
   where FL Studio looks. Type it and press Return. Nothing appears while you type;
   that is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**.
2. Click **Find installed plugins** (or **Find more plugins**) and wait for the scan to finish.
3. Open the **Mixer**, click an empty effect slot on any track, and choose **Cruz EQ**.

If Cruz EQ does not show up, check that the plugin manager's search paths include
`/Library/Audio/Plug-Ins/VST3`, then scan again. If you see both a VST3 and an AU
version, use the VST3 one.

## Updating to a newer build

Repeat steps 1 and 2. If an older **CruzEQ-mac** folder is already in Downloads,
delete it first so the new one keeps the same name. Restart FL Studio afterwards.
