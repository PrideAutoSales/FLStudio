#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
inline const juce::Colour background { 0xff0f1115 };
inline const juce::Colour panel { 0xff171b21 };
inline const juce::Colour display { 0xff12161b };
inline const juce::Colour gridMinor { 0xff1b2028 };
inline const juce::Colour gridMajor { 0xff262d37 };
inline const juce::Colour zeroLine { 0xff3a4250 };
inline const juce::Colour text { 0xffe6e8eb };
inline const juce::Colour dimText { 0xff7d8590 };
inline const juce::Colour accent { 0xff4fc3a1 };
inline const juce::Colour curve { 0xfff2f4f7 };
inline const juce::Colour analyzerPost { 0xff3d8db5 };
inline const juce::Colour analyzerPre { 0xff4c5563 };

inline juce::Colour band (int index)
{
    return juce::Colour::fromHSV (std::fmod (0.52f + (float) index * 0.137f, 1.0f), 0.6f, 0.98f, 1.0f);
}
} // namespace Theme
