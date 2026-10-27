# Cruz EQ

A visual parametric EQ plugin for FL Studio on Mac (VST3 and AU), aiming for the
feature set and performance of today's top mixing EQs, with its own code and its own look.

This is the first of a set of mixing tools for personal use in FL Studio sessions.

## Getting the plugin

Every change pushed to `main` is built automatically on GitHub's Mac machines and
published under **Releases > Latest build**. See [docs/INSTALL.md](docs/INSTALL.md)
for step-by-step install instructions. Pull requests are built too, and their downloads
appear under the **Artifacts** section of each run on the **Actions** tab.

## Roadmap

1. **Project setup** (done): plugin skeleton, automatic Mac build, install guide.
2. **First playable EQ**: zero-latency bands with the main filter shapes, draggable band
   nodes, and a live spectrum analyzer.
3. **Feature parity**: linear and natural phase modes, dynamic EQ, spectral dynamics,
   mid/side and left/right processing, EQ match, auto gain, and the remaining shapes and slopes.
4. **Polish and benchmark**: CPU, latency and sound compared side by side in real sessions.

## Building it yourself (optional)

Requires CMake 3.22+ and Xcode. JUCE is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Licence note

Cruz EQ is built on [JUCE](https://juce.com), which is free for personal use under
its open-source licence. Check JUCE's licence terms before selling or distributing it.
