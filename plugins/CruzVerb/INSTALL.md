# Installing Cruz Verb on your Mac

You only need to do this once per new build. No coding tools are needed.

## 1. Download the latest build

1. Open the repo on GitHub and click **Releases** on the right side.
2. Under **Latest build**, click **CruzVerb-mac.zip** to download it.
3. In Finder, open your **Downloads** folder and double-click **CruzVerb-mac.zip**. You will get a folder called **CruzVerb-mac**.

## 2. Install it

1. Open **Terminal** (press Cmd + Space, type `Terminal`, press Return).
2. Paste this line and press Return:

   ```
   bash ~/Downloads/CruzVerb-mac/install.sh
   ```

3. When it says **Cruz Verb is installed**, you can close Terminal.

## 3. Load it in FL Studio

1. In FL Studio, open **Options > Manage plugins**, click **Find installed plugins**, and wait for the scan.
2. Open the **Mixer**, click an empty effect slot, and choose **Cruz Verb**.

The usual way to use a reverb is on a send: route a few tracks to an empty mixer
track, put Cruz Verb on it and turn **Mix** to 100 %. Directly on a track, keep Mix low.

## Updating to a newer build

Delete the old **CruzVerb-mac** folder in Downloads, then repeat steps 1 and 2 and restart FL Studio.
