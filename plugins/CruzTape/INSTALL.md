# Installing Cruz Tape on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest build**, click **CruzTape-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzTape-mac.zip**. You will get a folder called **CruzTape-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzTape-mac/install.sh
   ```

3. It asks for your Mac login password, because the plugin goes in the Mac's main
   plugin folder (`/Library/Audio/Plug-Ins`), which is where FL Studio looks.
   Type the password and press Return. Nothing appears while you type, which is normal.
4. When it says **Done**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**.
2. Click **Find installed plugins** and wait for the scan to finish.
3. Open the **Mixer**, click an empty effect slot on any track, and choose **Cruz Tape**.

If Cruz Tape does not show up, check that the plugin manager's search paths include
`/Library/Audio/Plug-Ins/VST3`, then scan again.

## Updating to a newer build

Repeat steps 1 and 2. If an older **CruzTape-mac** folder is already in Downloads,
delete it first so the new one keeps the same name. Restart FL Studio afterwards.

## What the controls do

- **Speed**: 7.5, 15 or 30 inches per second. Slower is darker with a bigger, lower
  bass bump and more wobble. 30 is the cleanest and most open.
- **Tape**: four tape stocks. Vintage saturates first and sounds warmest, Classic is the
  all-rounder, Hot takes more level, Modern is the cleanest with the most headroom.
- **Calibration**: how hot the machine records. +9 drives the tape harder (more
  saturation, less hiss), +3 keeps it cleaner.
- **EQ curve**: NAB (the American standard, a touch more low end weight) or CCIR
  (the European standard, slightly brighter saturation).
- **Input**: how hard you hit the tape. This is the main "more tape sound" control.
- **Auto Gain**: when on, turning Input up adds saturation without making it louder.
- **Bias**: left (under bias) is brighter and grittier, right (over bias) is darker and smoother.
- **Wow** and **Flutter**: slow and fast pitch wobble from the tape transport.
- **Hiss**: tape noise. 50% is about what a real machine makes.
- **Mix**: blends the dry signal back in for parallel tape.
- **Output**: final level.

Cruz Tape adds about 3 ms of latency. FL Studio compensates for it automatically.
