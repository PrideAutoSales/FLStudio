#include "Controls.h"

namespace cs::ui
{
juce::String modDragDescription (int source) { return "mod:" + juce::String (source); }

int modSourceFromDescription (const juce::var& d)
{
    const auto s = d.toString();
    return s.startsWith ("mod:") ? s.fromFirstOccurrenceOf ("mod:", false, false).getIntValue() : -1;
}

//==============================================================================
ModKnob::ModKnob (CruzSynthProcessor& p, const juce::String& pid, const juce::String& l)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
      proc (p), paramID (pid), label (l), destination (modDestinationForParam (pid))
{
    getProperties().set ("labelHeight", 15);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramID, *this);

    if (auto* param = proc.apvts.getParameter (paramID))
    {
        setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        setTooltip (param->getName (64) + (destination > 0 ? "\nDrag an LFO, envelope or macro here to modulate it." : ""));
    }
    if (destination > 0)
        startTimerHz (25);
}

ModKnob::~ModKnob() = default;

juce::RangedAudioParameter* ModKnob::slotParam (int slot, const char* name) const
{
    return proc.apvts.getParameter (id::slot (slot, name));
}

void ModKnob::timerCallback()
{
    // Repaint only when routings or the live modulated position change.
    juce::int64 h = dropHover ? 7 : 0;
    const auto slots = proc.slotsForDestination (destination);
    for (auto s : slots)
        h = h * 31 + (juce::int64) (slotParam (s, "amount")->getValue() * 10000.0f) + s * 1000003
            + (juce::int64) proc.apvts.getRawParameterValue (id::slot (s, "src"))->load() * 7919;
    if (! slots.isEmpty() && proc.live.anyVoice.load())
        h = h * 31 + (juce::int64) (proc.live.dstNorm[(size_t) destination].load() * 2000.0f);
    if (h != lastHash)
    {
        lastHash = h;
        repaint();
    }
}

void ModKnob::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);

    auto bounds = getLocalBounds();
    auto labelArea = bounds.removeFromBottom (15);
    const auto area = bounds.toFloat();
    const float r = (juce::jmin (area.getWidth(), area.getHeight()) - 12.0f) * 0.5f;
    const auto c = area.getCentre();
    const auto rp = getRotaryParameters();
    const float span = rp.endAngleRadians - rp.startAngleRadians;

    if (destination > 0)
    {
        auto* param = proc.apvts.getParameter (paramID);
        const float base = param->getValue();
        const auto slots = proc.slotsForDestination (destination);
        float ringR = r + 4.5f;
        for (auto s : slots)
        {
            const int src = juce::roundToInt (proc.apvts.getRawParameterValue (id::slot (s, "src"))->load());
            const float amt = slotParam (s, "amount")->convertFrom0to1 (slotParam (s, "amount")->getValue());
            const bool bip = proc.apvts.getRawParameterValue (id::slot (s, "bipolar"))->load() > 0.5f || modSourceIsBipolar (src);
            const float lo = juce::jlimit (0.0f, 1.0f, bip ? base - std::abs (amt) : juce::jmin (base, base + amt));
            const float hi = juce::jlimit (0.0f, 1.0f, bip ? base + std::abs (amt) : juce::jmax (base, base + amt));
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, rp.startAngleRadians + lo * span, rp.startAngleRadians + hi * span, true);
            g.setColour (modSourceColour (src).withAlpha (s == activeSlot ? 1.0f : 0.75f));
            g.strokePath (arc, juce::PathStrokeType (s == activeSlot ? 2.5f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            ringR = juce::jmax (r + 1.5f, ringR - 2.0f);
        }

        if (! slots.isEmpty() && proc.live.anyVoice.load())
        {
            const float v = proc.live.dstNorm[(size_t) destination].load();
            const auto pt = c.getPointOnCircumference (r, rp.startAngleRadians + v * span);
            g.setColour (colours::text);
            g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
        }

        if (dropHover)
        {
            g.setColour (colours::accent2.withAlpha (0.35f));
            g.fillEllipse (c.x - r - 4.0f, c.y - r - 4.0f, (r + 4.0f) * 2.0f, (r + 4.0f) * 2.0f);
        }
    }

    const bool showValue = isMouseOverOrDragging() || adjustingMod;
    g.setColour (showValue ? colours::accent : colours::textDim);
    g.setFont (font (11.0f, showValue));
    juce::String text = label;
    if (adjustingMod && activeSlot >= 0)
        text = juce::String (juce::roundToInt (slotParam (activeSlot, "amount")->convertFrom0to1 (slotParam (activeSlot, "amount")->getValue()) * 100.0f)) + "%";
    else if (showValue)
        text = getTextFromValue (getValue());
    g.drawFittedText (text, labelArea, juce::Justification::centred, 1, 0.7f);
}

void ModKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    if (e.mods.isAltDown() && destination > 0)
    {
        const auto slots = proc.slotsForDestination (destination);
        if (! slots.isEmpty())
        {
            if (! slots.contains (activeSlot))
                activeSlot = slots.getLast();
            adjustingMod = true;
            auto* p = slotParam (activeSlot, "amount");
            dragStartAmount = p->convertFrom0to1 (p->getValue());
            p->beginChangeGesture();
            repaint();
            return;
        }
    }
    juce::Slider::mouseDown (e);
}

void ModKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (adjustingMod)
    {
        auto* p = slotParam (activeSlot, "amount");
        const float amt = juce::jlimit (-1.0f, 1.0f, dragStartAmount - (float) e.getDistanceFromDragStartY() / 150.0f
                                                         + (float) e.getDistanceFromDragStartX() / 300.0f);
        p->setValueNotifyingHost (p->convertTo0to1 (amt));
        repaint();
        return;
    }
    if (! e.mods.isPopupMenu())
        juce::Slider::mouseDrag (e);
}

void ModKnob::mouseUp (const juce::MouseEvent& e)
{
    if (adjustingMod)
    {
        slotParam (activeSlot, "amount")->endChangeGesture();
        adjustingMod = false;
        repaint();
        return;
    }
    if (! e.mods.isPopupMenu())
        juce::Slider::mouseUp (e);
}

void ModKnob::showMenu()
{
    juce::PopupMenu m;
    const auto slots = destination > 0 ? proc.slotsForDestination (destination) : juce::Array<int>();
    if (! slots.isEmpty())
    {
        m.addSectionHeader ("Modulation");
        for (auto s : slots)
        {
            const int src = juce::roundToInt (proc.apvts.getRawParameterValue (id::slot (s, "src"))->load());
            const int pct = juce::roundToInt (slotParam (s, "amount")->convertFrom0to1 (slotParam (s, "amount")->getValue()) * 100.0f);
            juce::PopupMenu sub;
            sub.addItem (300 + s, "Edit with Option-drag", true, s == activeSlot);
            sub.addItem (200 + s, "Bipolar", true, proc.apvts.getRawParameterValue (id::slot (s, "bipolar"))->load() > 0.5f);
            sub.addItem (100 + s, "Remove");
            m.addSubMenu (modSourceNames()[src] + "  " + juce::String (pct) + "%", sub);
        }
        m.addSeparator();
    }
    m.addItem (1, "Reset to default");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<ModKnob> (this)] (int r)
                     {
                         if (safe == nullptr || r == 0)
                             return;
                         auto& k = *safe;
                         if (r == 1)
                         {
                             if (auto* p = k.proc.apvts.getParameter (k.paramID))
                                 p->setValueNotifyingHost (p->getDefaultValue());
                         }
                         else if (r >= 300) k.activeSlot = r - 300;
                         else if (r >= 200)
                         {
                             auto* p = k.slotParam (r - 200, "bipolar");
                             p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                         }
                         else if (r >= 100)
                             k.proc.clearModulationSlot (r - 100);
                         k.repaint();
                     });
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return destination > 0 && modSourceFromDescription (d.description) > 0;
}

void ModKnob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = modSourceFromDescription (d.description);
    const int slot = proc.addModulation (src, destination, 0.3f);
    if (slot >= 0)
        activeSlot = slot;
    repaint();
}

//==============================================================================
ModSourceChip::ModSourceChip (int s, juce::String t) : source (s), text (std::move (t))
{
    setTooltip ("Drag onto any knob to modulate it with " + modSourceNames()[s] + ".");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    const auto col = modSourceColour (source);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? col.withAlpha (0.22f) : colours::panelHi);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (selected ? col : col.withAlpha (0.5f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    // Four-way arrow hint that this can be dragged
    const float cx = r.getX() + r.getHeight() * 0.55f, cy = r.getCentreY(), a = juce::jmin (5.0f, r.getHeight() * 0.25f);
    g.setColour (col);
    g.drawLine (cx - a, cy, cx + a, cy, 1.4f);
    g.drawLine (cx, cy - a, cx, cy + a, 1.4f);
    g.fillEllipse (cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);

    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (font (11.5f, true));
    g.drawFittedText (text, r.withTrimmedLeft (r.getHeight()).toNearestInt(), juce::Justification::centredLeft, 1);
}

void ModSourceChip::mouseDown (const juce::MouseEvent&) { dragging = false; }

void ModSourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 4)
        return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        c->startDragging (modDragDescription (source), this, juce::ScaledImage (createComponentSnapshot (getLocalBounds())));
    }
}

void ModSourceChip::mouseUp (const juce::MouseEvent&)
{
    if (! dragging && onClick)
        onClick();
    dragging = false;
}

//==============================================================================
PowerButton::PowerButton (juce::AudioProcessorValueTreeState& s, const juce::String& pid)
    : attachment (s, pid, *this)
{
    getProperties().set ("power", true);
    setTooltip ("Turn on or off");
}

ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& pid, const juce::String& t)
    : attachment (s, pid, *this)
{
    setButtonText (t);
    if (auto* p = s.getParameter (pid))
        setTooltip (p->getName (64));
}

ParamCombo::ParamCombo (juce::AudioProcessorValueTreeState& s, const juce::String& pid)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (pid)))
    {
        addItemList (p->choices, 1);
        setTooltip (p->getName (64));
    }
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid, *this);
}

void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items)
{
    if (items.size() == 0)
        return;
    const int w = area.getWidth() / (int) items.size();
    for (auto* c : items)
        if (c != nullptr)
            c->setBounds (area.removeFromLeft (w));
}
} // namespace cs::ui
