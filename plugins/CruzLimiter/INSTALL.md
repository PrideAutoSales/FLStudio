# Installing Cruz Limiter on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest builds**, click **CruzLimiter-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzLimiter-mac.zip**. You will get a folder called **CruzLimiter-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzLimiter-mac/install.sh
   ```

3. When it says **Cruz Limiter is installed**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
2. Open the **Mixer**, select the **Master** track, and put **Cruz Limiter** in the last effect slot.

## 4. Quick start

1. Leave **Ceiling** at -1.0 dB and **True Peak** on. That is safe for streaming.
2. Play the loudest part of your song and raise the **Gain** fader on the left of the display.
   The red area at the top shows how much the limiter is turning down.
3. Watch **Integrated** in the meter panel while the whole song plays (press **Reset** first).
   Around -14 LUFS suits streaming; -9 to -7 LUFS is loud club or rap mastering.
4. Try the **Style** menu: Allround is a good start, Punch or Dynamic keep drums lively,
   Loud and Modern are denser, Clean and Safe are the most transparent.
5. Turn on **Audition** to hear only what is being removed. If you hear the music clearly in it,
   you are pushing too hard.
6. Only turn on **Dither** (16 bit) when exporting a WAV for CD.

Hover over any control to see a short explanation.

## Updating to a newer build

Delete the old **CruzLimiter-mac** folder in Downloads, then repeat steps 1 and 2 and restart FL Studio.
