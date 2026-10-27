#include "WavetableEditor.h"

namespace cs::ui
{
namespace
{
    constexpr int N = Wavetable::kSize;
    constexpr int kHarmonics = 64;
}

//==============================================================================
class WavetableEditor::DrawArea : public juce::Component
{
public:
    explicit DrawArea (WavetableEditor& e) : owner (e) { setMouseCursor (juce::MouseCursor::CrosshairCursor); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::display);
        g.fillRoundedRectangle (r, 5.0f);
        r = r.reduced (8.0f);
        g.setColour (colours::outline.withAlpha (0.5f));
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        const float* f = owner.frame (owner.current);
        juce::Path p;
        for (int i = 0; i < 512; ++i)
        {
            const float x = r.getX() + (float) i / 511.0f * r.getWidth();
            const float y = r.getCentreY() - f[i * N / 512] * r.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colours::accent);
        g.strokePath (p, juce::PathStrokeType (2.0f));

        g.setColour (colours::textDim);
        g.setFont (font (12.0f));
        g.drawText ("Draw on the wave with the mouse", getLocalBounds().reduced (10, 6), juce::Justification::topLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override { last = e.position; paintAt (e.position, e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { paintAt (last, e.position); last = e.position; }
    void mouseUp (const juce::MouseEvent&) override    { owner.commit(); }

private:
    void paintAt (juce::Point<float> a, juce::Point<float> b)
    {
        const auto r = getLocalBounds().toFloat().reduced (8.0f);
        auto toIndex = [&] (float x) { return juce::jlimit (0, N - 1, (int) ((x - r.getX()) / r.getWidth() * N)); };
        auto toValue = [&] (float y) { return juce::jlimit (-1.0f, 1.0f, (r.getCentreY() - y) / (r.getHeight() * 0.5f)); };
        int i0 = toIndex (a.x), i1 = toIndex (b.x);
        float v0 = toValue (a.y), v1 = toValue (b.y);
        if (i0 > i1) { std::swap (i0, i1); std::swap (v0, v1); }
        float* f = owner.frame (owner.current);
        // Fill the gap between mouse events, with a little width so strokes feel solid.
        for (int i = juce::jmax (0, i0 - 4); i <= juce::jmin (N - 1, i1 + 4); ++i)
        {
            const float t = i1 > i0 ? juce::jlimit (0.0f, 1.0f, (float) (i - i0) / (float) (i1 - i0)) : 0.0f;
            f[i] = v0 + (v1 - v0) * t;
        }
        owner.edited();
    }

    WavetableEditor& owner;
    juce::Point<float> last;
};

//==============================================================================
class WavetableEditor::HarmonicArea : public juce::Component
{
public:
    explicit HarmonicArea (WavetableEditor& e) : owner (e) {}

    void refresh()
    {
        mags = tables::harmonicMagnitudes (owner.frame (owner.current), kHarmonics);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::display);
        g.fillRoundedRectangle (r, 5.0f);
        r = r.reduced (8.0f, 22.0f);
        const float bw = r.getWidth() / kHarmonics;
        for (int h = 0; h < kHarmonics && h < (int) mags.size(); ++h)
        {
            const float v = juce::jlimit (0.0f, 1.0f, mags[(size_t) h]);
            const float hgt = std::sqrt (v) * r.getHeight();
            g.setColour (h == 0 ? colours::accent : colours::accent2.withAlpha (0.85f));
            g.fillRect (r.getX() + (float) h * bw + 1.0f, r.getBottom() - hgt, bw - 2.0f, hgt);
        }
        g.setColour (colours::textDim);
        g.setFont (font (12.0f));
        g.drawText ("Harmonics (drag bars up or down)", getLocalBounds().reduced (10, 4), juce::Justification::topLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override { setBar (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { setBar (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        tables::setHarmonicMagnitudes (owner.frame (owner.current), mags);
        owner.edited();
        owner.commit();
    }

private:
    void setBar (juce::Point<float> p)
    {
        const auto r = getLocalBounds().toFloat().reduced (8.0f, 22.0f);
        const int h = juce::jlimit (0, kHarmonics - 1, (int) ((p.x - r.getX()) / r.getWidth() * kHarmonics));
        const float v = juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight());
        if ((int) mags.size() == kHarmonics)
            mags[(size_t) h] = v * v;
        repaint();
    }

    WavetableEditor& owner;
    std::vector<float> mags;
};

//==============================================================================
class WavetableEditor::FrameStrip : public juce::Component
{
public:
    explicit FrameStrip (WavetableEditor& e) : owner (e) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::display);
        g.fillRoundedRectangle (r, 5.0f);
        const int count = owner.numFrames();
        const int shown = juce::jmin (count, 64);
        const float w = r.getWidth() / (float) shown;
        for (int k = 0; k < shown; ++k)
        {
            const int f = shown > 1 ? juce::roundToInt ((float) k / (float) (shown - 1) * (float) (count - 1)) : 0;
            const auto cell = juce::Rectangle<float> (r.getX() + (float) k * w, r.getY(), w, r.getHeight()).reduced (1.0f, 6.0f);
            const bool isCurrent = f == owner.current;
            if (isCurrent)
            {
                g.setColour (colours::accent.withAlpha (0.2f));
                g.fillRect (cell);
            }
            juce::Path p;
            const float* d = owner.frame (f);
            for (int i = 0; i <= 24; ++i)
            {
                const float x = cell.getX() + (float) i / 24.0f * cell.getWidth();
                const float y = cell.getCentreY() - d[juce::jmin (N - 1, i * N / 24)] * cell.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (isCurrent ? colours::accent : colours::accent2.withAlpha (0.6f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { pick (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override { pick (e.position.x); }

private:
    void pick (float x)
    {
        const int count = owner.numFrames();
        const int shown = juce::jmin (count, 64);
        const int k = juce::jlimit (0, shown - 1, (int) (x / (float) getWidth() * (float) shown));
        owner.setCurrent (shown > 1 ? juce::roundToInt ((float) k / (float) (shown - 1) * (float) (count - 1)) : 0);
    }

    WavetableEditor& owner;
};

//==============================================================================
WavetableEditor::WavetableEditor (CruzSynthProcessor& p, int o, std::function<void()> close)
    : proc (p), osc (o), onClose (std::move (close))
{
    drawArea = std::make_unique<DrawArea> (*this);
    harmonicArea = std::make_unique<HarmonicArea> (*this);
    strip = std::make_unique<FrameStrip> (*this);
    addAndMakeVisible (*drawArea);
    addAndMakeVisible (*harmonicArea);
    addAndMakeVisible (*strip);
    addAndMakeVisible (frameSlider);
    frameSlider.onValueChange = [this] { setCurrent ((int) frameSlider.getValue()); };

    for (auto* b : { &importButton, &exportButton, &closeButton, &addButton, &deleteButton, &fadeButton, &spectralButton,
                     &normaliseButton, &smoothButton, &invertButton, &reverseButton, &sineButton, &factoryButton })
        addAndMakeVisible (*b);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    importButton.onClick = [this] { importFile(); };
    exportButton.onClick = [this] { exportFile(); };

    addButton.setTooltip ("Copy the current frame and insert it after this one");
    addButton.onClick = [this]
    {
        if (numFrames() >= Wavetable::kMaxFrames) return;
        std::vector<float> copy (frame (current), frame (current) + N);
        frames.insert (frames.begin() + (ptrdiff_t) (current + 1) * N, copy.begin(), copy.end());
        edited(); setCurrent (current + 1); commit();
    };
    deleteButton.onClick = [this]
    {
        if (numFrames() <= 1) return;
        frames.erase (frames.begin() + (ptrdiff_t) current * N, frames.begin() + (ptrdiff_t) (current + 1) * N);
        edited(); setCurrent (juce::jmin (current, numFrames() - 1)); commit();
    };

    fadeButton.setTooltip ("Rebuild the in-between frames as a smooth blend from the first frame to the last");
    fadeButton.onClick = [this]
    {
        const int count = juce::jmax (32, numFrames());
        std::vector<float> first (frame (0), frame (0) + N), last (frame (numFrames() - 1), frame (numFrames() - 1) + N);
        frames.assign ((size_t) count * N, 0.0f);
        for (int f = 0; f < count; ++f)
        {
            const float t = (float) f / (float) (count - 1);
            for (int i = 0; i < N; ++i)
                frame (f)[i] = first[(size_t) i] + t * (last[(size_t) i] - first[(size_t) i]);
        }
        edited(); setCurrent (current); commit();
    };

    spectralButton.setTooltip ("Rebuild the in-between frames by blending harmonics, which keeps the sound fuller than a crossfade");
    spectralButton.onClick = [this]
    {
        const int count = juce::jmax (32, numFrames());
        auto a = tables::harmonicMagnitudes (frame (0), 512);
        auto b = tables::harmonicMagnitudes (frame (numFrames() - 1), 512);
        std::vector<float> base (frame (0), frame (0) + N);
        frames.assign ((size_t) count * N, 0.0f);
        for (int f = 0; f < count; ++f)
        {
            const float t = (float) f / (float) (count - 1);
            std::vector<float> m (512);
            for (int h = 0; h < 512; ++h)
                m[(size_t) h] = a[(size_t) h] + t * (b[(size_t) h] - a[(size_t) h]);
            std::copy (base.begin(), base.end(), frame (f));
            tables::setHarmonicMagnitudes (frame (f), m);
        }
        tables::normalise (frames);
        edited(); setCurrent (current); commit();
    };

    normaliseButton.onClick = [this] { tables::normalise (frames); edited(); commit(); };
    smoothButton.onClick = [this]
    {
        float* f = frame (current);
        for (int pass = 0; pass < 4; ++pass)
        {
            std::vector<float> c (f, f + N);
            for (int i = 0; i < N; ++i)
                f[i] = (c[(size_t) ((i + N - 1) % N)] + 2.0f * c[(size_t) i] + c[(size_t) ((i + 1) % N)]) * 0.25f;
        }
        edited(); commit();
    };
    invertButton.onClick = [this] { juce::FloatVectorOperations::negate (frame (current), frame (current), N); edited(); commit(); };
    reverseButton.onClick = [this] { std::reverse (frame (current), frame (current) + N); edited(); commit(); };
    sineButton.onClick = [this]
    {
        for (int i = 0; i < N; ++i)
            frame (current)[i] = std::sin (juce::MathConstants<float>::twoPi * (float) i / (float) N);
        edited(); commit();
    };
    factoryButton.onClick = [this]
    {
        juce::PopupMenu m;
        const auto names = tables::factoryNames();
        for (int i = 0; i < names.size(); ++i)
            m.addItem (i + 1, names[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&factoryButton), [this] (int r)
        {
            if (r > 0)
            {
                proc.setTable (osc, tables::makeFactory (r - 1));
                reload();
            }
        });
    };

    reload();
}

WavetableEditor::~WavetableEditor() = default;

void WavetableEditor::reload()
{
    auto t = proc.getTable (osc);
    frames = t->getRawFrames();
    name = t->getName();
    current = juce::jmin (current, numFrames() - 1);
    setCurrent (current);
}

void WavetableEditor::setCurrent (int f)
{
    current = juce::jlimit (0, numFrames() - 1, f);
    frameSlider.setRange (0.0, (double) juce::jmax (1, numFrames() - 1), 1.0);
    frameSlider.setEnabled (numFrames() > 1);
    frameSlider.setValue (current, juce::dontSendNotification);
    harmonicArea->refresh();
    repaint();
    drawArea->repaint();
    strip->repaint();
}

void WavetableEditor::edited()
{
    if (! name.endsWith ("*"))
        name << "*";
    drawArea->repaint();
    strip->repaint();
    repaint();
}

void WavetableEditor::commit()
{
    proc.setTable (osc, std::make_shared<const Wavetable> (name, frames));
    harmonicArea->refresh();
}

void WavetableEditor::importFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a wavetable", juce::File(), "*.wav;*.aif;*.aiff");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              juce::String error;
                              auto data = tables::loadFromFile (file, error);
                              if (data.empty())
                              {
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load wavetable", error);
                                  return;
                              }
                              proc.setTable (osc, std::make_shared<const Wavetable> (file.getFileNameWithoutExtension(), std::move (data)));
                              current = 0;
                              reload();
                          });
}

void WavetableEditor::exportFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Save wavetable", juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                                         .getChildFile (name.trimCharactersAtEnd ("*") + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file != juce::File() && ! tables::saveToWav (*proc.getTable (osc), file.withFileExtension ("wav")))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", "The file could not be written.");
                          });
}

void WavetableEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.withAlpha (0.97f));
    drawPanel (g, getLocalBounds().reduced (6), {});
    g.setColour (colours::text);
    g.setFont (font (16.0f, true));
    g.drawText (juce::String ("Wavetable editor  -  Osc ") + (osc == 0 ? "A" : "B"), 20, 14, 400, 22, juce::Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (13.0f));
    g.drawText (name + "   (" + juce::String (numFrames()) + " frames, editing frame " + juce::String (current + 1) + ")",
                20, 36, 600, 18, juce::Justification::centredLeft);
}

void WavetableEditor::resized()
{
    auto r = getLocalBounds().reduced (18);
    auto header = r.removeFromTop (42);
    closeButton.setBounds (header.removeFromRight (80).withSizeKeepingCentre (80, 28));
    header.removeFromRight (8);
    exportButton.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 28));
    header.removeFromRight (8);
    importButton.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 28));
    header.removeFromRight (8);
    factoryButton.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 28));
    r.removeFromTop (8);

    auto buttons = r.removeFromBottom (30);
    r.removeFromBottom (8);
    auto stripArea = r.removeFromBottom (64);
    r.removeFromBottom (6);
    frameSlider.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (6);

    harmonicArea->setBounds (r.removeFromRight (r.getWidth() * 2 / 5));
    r.removeFromRight (10);
    drawArea->setBounds (r);
    strip->setBounds (stripArea);

    const int w = buttons.getWidth() / 9;
    for (auto* b : { &addButton, &deleteButton, &fadeButton, &spectralButton, &normaliseButton, &smoothButton,
                     &invertButton, &reverseButton, &sineButton })
        b->setBounds (buttons.removeFromLeft (w).reduced (3, 0));
}
} // namespace cs::ui
