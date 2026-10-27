# Cruz Echo

A character delay for FL Studio on Mac (VST3 and AU), in the spirit of the classic
"one box, many echo machines" delays, with its own code and its own look.

Install steps are in [INSTALL.md](INSTALL.md).

## The controls

**Style** picks the echo machine. Each one sets its own tone, drive and wobble, and the
knobs work on top of it.

| Style   | What it sounds like |
|---------|---------------------|
| Digital | Clean, full-range repeats. Time changes glide quickly. |
| Analog  | Dark and warm like a bucket-brigade pedal, soft clipping, a little clock wobble. |
| Tape    | Rounded highs, tape drive and wow and flutter. Time changes bend the pitch like tape. |
| Space   | A darker, smeared tape echo with more wobble, in the style of old roller-head machines. |
| Lo-Fi   | Telephone-band, crunchy, low sample rate repeats. |

**Mode**: *Single* is one echo in the middle. *Stereo* echoes each side on its own.
*Ping-Pong* bounces the repeats left, right, left.

| Knob       | What it does |
|------------|--------------|
| Time       | Echo time. With **Sync to tempo** on it picks a note length (T is triplet, D is dotted) and follows FL Studio's tempo; off, it is in milliseconds (1 ms to 4 s). |
| Feel       | Pushes the echoes early or late by up to 30 ms, to sit ahead of or behind the beat. |
| Groove     | Swings every other repeat. 100% is a full triplet shuffle. |
| Feedback   | How many repeats. Above 100% the echoes build up into the saturation for dub-style runaway. |
| Mix        | Dry to wet. Set it to 100% when you use Cruz Echo on a send. |
| Output     | Final level. |
| Saturation | How hard each repeat drives the style's circuit. |
| Wobble     | Wow and flutter amount. |
| Low Cut, High Cut | Filters every repeat, so the tail gets thinner or darker as it goes. The style may already cut further. |
| Width      | Stereo width of the echoes only, from mono to extra wide. |
| Duck       | Lowers the echoes while you play and brings them back in the gaps, so vocals stay clear. |

Double-click any knob to reset it.

## Engine tests

`tests/EngineTest.cpp` checks echo timing, groove, ping-pong routing and that every style
stays stable at 110% feedback. It runs on every build; to run it yourself:
`cmake --build build --target CruzEchoEngineTest && build/plugins/CruzEcho/CruzEchoEngineTest`.
