#pragma once

// Makes sure a plugin window is drawn on the first open.
//
// JUCE only draws a Mac plugin window when the screen it is on sends its
// refresh signal. Some hosts (FL Studio on macOS) show the window in a way
// that does not get those signals at first, so the editor stays blank until
// the plugin is opened a second time. This guard asks macOS to draw the whole
// editor directly for its first moments on screen, and keeps doing so for as
// long as the refresh signals are missing.
//
// Add it as the last member of the editor:
//     cruz::EditorRedrawGuard redrawGuard { *this };

#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_MAC
 #include <objc/message.h>
 #include <objc/runtime.h>
#endif

namespace cruz
{
class EditorRedrawGuard final : private juce::Timer
{
public:
    explicit EditorRedrawGuard (juce::Component& editorToDraw)
        : editor (editorToDraw),
          vblank (&editorToDraw, [this] { lastVBlankMs = juce::Time::getMillisecondCounter(); })
    {
       #if JUCE_MAC
        startTimerHz (20);
       #endif
    }

private:
    void timerCallback() override
    {
        if (! windowIsVisible())
        {
            shownAtMs = 0;
            return;
        }

        const auto now = juce::Time::getMillisecondCounter();

        if (shownAtMs == 0)
            shownAtMs = now;

        const bool justShown = now - shownAtMs < 1500;
        const bool noRefreshSignal = now - lastVBlankMs > 250;

        if (justShown || noRefreshSignal)
            drawNow();
    }

   #if JUCE_MAC
    id nativeView() const
    {
        auto* peer = editor.getPeer();
        return peer != nullptr ? (id) peer->getNativeHandle() : nil;
    }

    template <typename Result, typename... Args>
    static Result send (id target, const char* selector, Args... args)
    {
        return reinterpret_cast<Result (*) (id, SEL, Args...)> (objc_msgSend) (target, sel_registerName (selector), args...);
    }

    bool windowIsVisible() const
    {
        if (auto view = nativeView())
            if (auto window = send<id> (view, "window"))
                return send<BOOL> (window, "isVisible");

        return false;
    }

    void drawNow()
    {
        if (auto view = nativeView())
        {
            send<void> (view, "setNeedsDisplay:", (BOOL) YES);
            send<void> (view, "displayIfNeeded");
        }
    }
   #else
    bool windowIsVisible() const { return false; }
    void drawNow() {}
   #endif

    [[maybe_unused]] juce::Component& editor; // only drawn directly on macOS
    juce::VBlankAttachment vblank;
    juce::uint32 lastVBlankMs = 0, shownAtMs = 0;

    JUCE_DECLARE_NON_COPYABLE (EditorRedrawGuard)
};
} // namespace cruz
