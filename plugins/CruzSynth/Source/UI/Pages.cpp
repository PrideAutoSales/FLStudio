#include "Pages.h"

namespace cs::ui
{
namespace
{
    constexpr int kHeader = 28;

    // Places a power button in the panel's title strip.
    void placePower (juce::Component& c, juce::Rectangle<int> panel)
    {
        c.setBounds (panel.getX() + 8, panel.getY() + 5, 18, 18);
    }

    // Lays items out in a grid; small items (combos, toggles) are centred in their cell.
    void layoutGrid (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int cols, int rowH)
    {
        const int cw = area.getWidth() / cols;
        for (size_t i = 0; i < items.size(); ++i)
        {
            const int col = (int) i % cols, row = (int) i / cols;
            auto cell = juce::Rectangle<int> (area.getX() + col * cw, area.getY() + row * rowH, cw, rowH);
            if (dynamic_cast<ModKnob*> (items[i]) != nullptr)
                items[i]->setBounds (cell.reduced (4, 2));
            else
                items[i]->setBounds (cell.withSizeKeepingCentre (cw - 10, 24));
        }
    }
}

//==============================================================================
OscPanel::OscPanel (CruzSynthProcessor& p, int o, std::function<void (int)> onEdit)
    : Panel (o == 0 ? "OSC A" : "OSC B"), proc (p), osc (o)
{
    power = &make<PowerButton> (proc.apvts, id::osc (o, "on"));
    tableBox = &make<juce::ComboBox>();
    tableBox->addItemList (tables::factoryNames(), 1);
    tableBox->setTooltip ("Choose a wavetable");
    tableBox->onChange = [this]
    {
        const int i = tableBox->getSelectedItemIndex();
        if (i >= 0)
            proc.setTable (osc, tables::makeFactory (i));
    };
    editButton = &make<juce::TextButton> ("Edit");
    editButton->setTooltip ("Open the wavetable editor (draw, morph, import or export)");
    editButton->onClick = [o, onEdit] { onEdit (o); };
    toFilter = &make<ParamToggle> (proc.apvts, id::osc (o, "toFilter"), "FLT");
    toFilter->setTooltip ("Send this oscillator through the filter");
    view = &make<WavetableView> (proc, o);
    warp = &make<ParamCombo> (proc.apvts, id::osc (o, "warp"));
    warp->setTooltip ("Warp mode: bends, squeezes or cross-modulates the wave");

    auto knob = [&] (const char* pid, const char* label) { return &make<ModKnob> (proc, id::osc (o, pid), label); };
    pitch = { knob ("octave", "OCT"), knob ("semi", "SEM"), knob ("fine", "FIN"), knob ("coarse", "CRS") };
    for (auto* k : pitch)
        k->setCompact (true);
    row1 = { knob ("unison", "Unison"), knob ("detune", "Detune"), knob ("blend", "Blend"),
             knob ("phase", "Phase"), knob ("rand", "Rand") };
    row2 = { knob ("pos", "WT Pos"), knob ("warpAmt", "Warp"), knob ("width", "Width"),
             knob ("pan", "Pan"), knob ("level", "Level") };
    refreshTableName();
}

void OscPanel::refreshTableName()
{
    auto t = proc.getTable (osc);
    if (t->getFactoryIndex() >= 0)
        tableBox->setSelectedItemIndex (t->getFactoryIndex(), juce::dontSendNotification);
    else
        tableBox->setText (t->getName(), juce::dontSendNotification);
    view->repaint();
}

void OscPanel::resized()
{
    auto r = getLocalBounds();
    placePower (*power, r);
    auto header = r.removeFromTop (kHeader).reduced (8, 3);
    header.removeFromLeft (70);
    toFilter->setBounds (header.removeFromRight (40));
    header.removeFromRight (4);
    editButton->setBounds (header.removeFromRight (46));
    header.removeFromRight (4);
    tableBox->setBounds (header);

    r = r.reduced (8, 2);
    auto pr = r.removeFromTop (22);
    const int pw = pr.getWidth() / (int) pitch.size();
    for (auto* k : pitch)
        k->setBounds (pr.removeFromLeft (pw).reduced (2, 0));
    r.removeFromTop (5);
    auto v = r.removeFromTop (juce::jmax (90, r.getHeight() - 150));
    view->setBounds (v);
    warp->setBounds (v.getX() + 6, v.getY() + 6, 104, 20);
    r.removeFromTop (2);
    const int rowH = r.getHeight() / 2;
    auto a = r.removeFromTop (rowH);
    const int w1 = a.getWidth() / (int) row1.size();
    for (auto* k : row1) k->setBounds (a.removeFromLeft (w1));
    const int w2 = r.getWidth() / (int) row2.size();
    for (auto* k : row2) k->setBounds (r.removeFromLeft (w2));
}

//==============================================================================
SubPanel::SubPanel (CruzSynthProcessor& p) : Panel ("SUB")
{
    power = &make<PowerButton> (p.apvts, "sub_on");
    shape = &make<ParamCombo> (p.apvts, "sub_shape");
    toFilter = &make<ParamToggle> (p.apvts, "sub_toFilter", "FLT");
    octave = &make<ModKnob> (p, "sub_octave", "Octave");
    level = &make<ModKnob> (p, "sub_level", "Level");
    pan = &make<ModKnob> (p, "sub_pan", "Pan");
}

void SubPanel::resized()
{
    auto r = getLocalBounds();
    placePower (*power, r);
    auto header = r.removeFromTop (kHeader).reduced (8, 3);
    toFilter->setBounds (header.removeFromRight (40));
    shape->setBounds (r.removeFromTop (24).reduced (8, 0));
    layoutRow (r.reduced (6, 4), { octave, level, pan });
}

NoisePanel::NoisePanel (CruzSynthProcessor& p) : Panel ("NOISE")
{
    power = &make<PowerButton> (p.apvts, "noise_on");
    type = &make<ParamCombo> (p.apvts, "noise_type");
    toFilter = &make<ParamToggle> (p.apvts, "noise_toFilter", "FLT");
    color = &make<ModKnob> (p, "noise_color", "Color");
    level = &make<ModKnob> (p, "noise_level", "Level");
    pan = &make<ModKnob> (p, "noise_pan", "Pan");
}

void NoisePanel::resized()
{
    auto r = getLocalBounds();
    placePower (*power, r);
    auto header = r.removeFromTop (kHeader).reduced (8, 3);
    toFilter->setBounds (header.removeFromRight (40));
    type->setBounds (r.removeFromTop (24).reduced (8, 0));
    layoutRow (r.reduced (6, 4), { color, level, pan });
}

//==============================================================================
FilterPanel::FilterPanel (CruzSynthProcessor& p) : Panel ("FILTER")
{
    power = &make<PowerButton> (p.apvts, "flt_on");
    type = &make<ParamCombo> (p.apvts, "flt_type");
    view = &make<FilterView> (p);
    knobs = { &make<ModKnob> (p, "flt_cutoff", "Cutoff"), &make<ModKnob> (p, "flt_res", "Res"),
              &make<ModKnob> (p, "flt_drive", "Drive"), &make<ModKnob> (p, "flt_mix", "Mix"),
              &make<ModKnob> (p, "flt_keytrack", "Key") };
}

void FilterPanel::resized()
{
    auto r = getLocalBounds();
    placePower (*power, r);
    auto header = r.removeFromTop (kHeader).reduced (8, 3);
    header.removeFromLeft (70);
    type->setBounds (header.removeFromLeft (130));
    r = r.reduced (8, 4);
    view->setBounds (r.removeFromTop (r.getHeight() - 84));
    r.removeFromTop (4);
    const int w = r.getWidth() / (int) knobs.size();
    for (auto* k : knobs) k->setBounds (r.removeFromLeft (w));
}

//==============================================================================
ModulatorPanel::ModulatorPanel (CruzSynthProcessor& p) : Panel ({}), proc (p)
{
    for (int i = 0; i < kNumEnvs + kNumLfos; ++i)
    {
        const int src = i < kNumEnvs ? (int) ModSrc::Env1 + i : (int) ModSrc::Lfo1 + (i - kNumEnvs);
        const juce::String text = i < kNumEnvs ? "ENV " + juce::String (i + 1) : "LFO " + juce::String (i - kNumEnvs + 1);
        auto& chip = make<ModSourceChip> (src, text);
        chip.setTooltip ("Click to edit. Drag onto any knob to modulate it.");
        chip.onClick = [this, i] { select (i); };
        chips.push_back (&chip);
    }

    envView = &make<EnvelopeView> (proc, 0);
    lfoView = &make<LfoView> (proc, 0);

    for (int e = 0; e < kNumEnvs; ++e)
    {
        std::vector<juce::Component*> c;
        for (auto [pid, label] : { std::pair { "attack", "Attack" }, { "hold", "Hold" }, { "decay", "Decay" },
                                   { "sustain", "Sustain" }, { "release", "Release" }, { "curve", "Curve" } })
            c.push_back (&make<ModKnob> (proc, id::env (e, pid), label));
        envControls.push_back (c);
    }

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto* shape = &make<ParamCombo> (proc.apvts, id::lfo (l, "shape"));
        auto* mode = &make<ParamCombo> (proc.apvts, id::lfo (l, "mode"));
        mode->setTooltip ("Free: runs on its own. Retrig: restarts with each note. Envelope: plays once per note.");
        auto* sync = &make<ParamToggle> (proc.apvts, id::lfo (l, "sync"), "SYNC");
        sync->setTooltip ("Lock the speed to the song tempo");
        auto* div = &make<ParamCombo> (proc.apvts, id::lfo (l, "div"));
        auto* rate = &make<ModKnob> (proc, id::lfo (l, "rate"), "Rate");
        lfoRate.push_back (rate);
        lfoDiv.push_back (div);
        lfoControls.push_back ({ shape, mode, sync, div, rate,
                                 &make<ModKnob> (proc, id::lfo (l, "rise"), "Rise"),
                                 &make<ModKnob> (proc, id::lfo (l, "smooth"), "Smooth"),
                                 &make<ModKnob> (proc, id::lfo (l, "phase"), "Phase") });
        sync->onStateChange = [this] { select (selected); };
    }
    select (0);
}

void ModulatorPanel::select (int index)
{
    selected = index;
    for (size_t i = 0; i < chips.size(); ++i)
        chips[i]->setSelected ((int) i == index);

    const bool isEnv = index < kNumEnvs;
    envView->setVisible (isEnv);
    lfoView->setVisible (! isEnv);
    for (int e = 0; e < kNumEnvs; ++e)
        for (auto* c : envControls[(size_t) e])
            c->setVisible (isEnv && e == index);
    for (int l = 0; l < kNumLfos; ++l)
    {
        const bool show = ! isEnv && l == index - kNumEnvs;
        for (auto* c : lfoControls[(size_t) l])
            c->setVisible (show);
        if (show)
        {
            const bool synced = proc.apvts.getRawParameterValue (id::lfo (l, "sync"))->load() > 0.5f;
            lfoRate[(size_t) l]->setVisible (! synced);
            lfoDiv[(size_t) l]->setVisible (synced);
        }
    }
    if (isEnv) envView->setEnvelope (index);
    else lfoView->setLfo (index - kNumEnvs);
    resized();
}

void ModulatorPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto chipRow = r.removeFromTop (22);
    const int cw = chipRow.getWidth() / (int) chips.size();
    for (auto* c : chips)
        c->setBounds (chipRow.removeFromLeft (cw).reduced (2, 0));
    r.removeFromTop (6);
    // The graph takes the left side; the selected modulator's controls sit on the right.
    auto controls = r.removeFromRight (r.getWidth() * 42 / 100);
    r.removeFromRight (8);
    envView->setBounds (r);
    lfoView->setBounds (r);

    for (auto& set : envControls)
    {
        const int colW = controls.getWidth() / 3, rowH = controls.getHeight() / 2;
        for (size_t i = 0; i < set.size(); ++i)
            set[i]->setBounds (controls.getX() + (int) (i % 3) * colW, controls.getY() + (int) (i / 3) * rowH, colW, rowH);
    }
    for (auto& set : lfoControls)
    {
        auto c = controls;
        auto row = c.removeFromTop (26);
        set[0]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (3, 1));
        set[1]->setBounds (row.reduced (3, 1));
        row = c.removeFromTop (28).withTrimmedTop (4);
        set[2]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (3, 1));
        // When synced the rate knob hides and the division menu (set[3]) is shown instead.
        set[3]->setBounds (row.reduced (3, 1));
        c.removeFromTop (4);
        const int w = c.getWidth() / 4;
        set[4]->setBounds (c.removeFromLeft (w));
        set[5]->setBounds (c.removeFromLeft (w));
        set[6]->setBounds (c.removeFromLeft (w));
        set[7]->setBounds (c);
    }
}

//==============================================================================
MacroPanel::MacroPanel (CruzSynthProcessor& p) : Panel ("MACROS")
{
    for (int m = 0; m < kNumMacros; ++m)
    {
        knobs.push_back (&make<ModKnob> (p, id::macro (m), "Macro " + juce::String (m + 1)));
        macroChips.push_back (&make<ModSourceChip> ((int) ModSrc::Macro1 + m, "M" + juce::String (m + 1)));
    }
    for (auto [src, text] : { std::pair { ModSrc::Velocity, "VEL" }, { ModSrc::NoteTrack, "NOTE" }, { ModSrc::ModWheel, "MW" },
                              { ModSrc::Aftertouch, "AT" }, { ModSrc::PitchBend, "PB" }, { ModSrc::NoteRandom, "RND" } })
        otherChips.push_back (&make<ModSourceChip> ((int) src, text));
}

void MacroPanel::resized()
{
    // One row: four macros (knob with its drag chip beside it), then the other sources.
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromLeft (64);   // room for the title
    auto others = r.removeFromRight (300);
    const int cw = r.getWidth() / kNumMacros;
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto cell = r.removeFromLeft (cw).reduced (4, 0);
        knobs[(size_t) m]->setBounds (cell.removeFromLeft (juce::jmin (84, cell.getWidth() / 2)));
        macroChips[(size_t) m]->setBounds (cell.withSizeKeepingCentre (juce::jmin (60, cell.getWidth() - 4), 22));
    }
    others = others.withSizeKeepingCentre (others.getWidth(), 52);
    const int ow = others.getWidth() / 3;
    for (size_t i = 0; i < otherChips.size(); ++i)
        otherChips[i]->setBounds (others.getX() + (int) (i % 3) * ow + 2, others.getY() + (int) (i / 3) * 28, ow - 4, 22);
}

//==============================================================================
VoicingPanel::VoicingPanel (CruzSynthProcessor& p) : Panel ("VOICING"), proc (p)
{
    toggles = { &make<ParamToggle> (p.apvts, "mono", "MONO"), &make<ParamToggle> (p.apvts, "legato", "LEGATO") };
    static_cast<ParamToggle*> (toggles[1])->setTooltip ("Mono only: overlapping notes slide without restarting the envelopes");
    knobs = { &make<ModKnob> (p, "poly", "Voices"), &make<ModKnob> (p, "glide", "Glide"),
              &make<ModKnob> (p, "bend_range", "Bend"), &make<ModKnob> (p, "vel_sens", "Velocity") };
    startTimerHz (8);
}

void VoicingPanel::timerCallback()
{
    const int v = proc.live.activeVoices.load();
    if (v != lastVoices)
    {
        lastVoices = v;
        repaint (getLocalBounds().removeFromTop (kHeader));
    }
}

void VoicingPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawText (juce::String (proc.live.activeVoices.load()) + " playing", getLocalBounds().removeFromTop (kHeader).reduced (10, 0),
                juce::Justification::centredRight);
}

void VoicingPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromTop (kHeader - 4);
    auto row = r.removeFromTop (26);
    for (auto* t : toggles)
        t->setBounds (row.removeFromLeft (row.getWidth() / (t == toggles.front() ? 2 : 1)).reduced (3, 1));
    r.removeFromTop (4);
    const int cw = r.getWidth() / 2, ch = r.getHeight() / 2;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (r.getX() + (int) (i % 2) * cw, r.getY() + (int) (i / 2) * ch, cw, ch);
}

//==============================================================================
OscPage::OscPage (CruzSynthProcessor& p, std::function<void (int)> onEditTable)
    : oscA (p, 0, onEditTable), oscB (p, 1, onEditTable), sub (p), noise (p), filter (p), mods (p), voicing (p), macros (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &oscA, &oscB, &sub, &noise, &filter, &mods, &voicing, &macros })
        addAndMakeVisible (c);
}

void OscPage::refresh()
{
    oscA.refreshTableName();
    oscB.refreshTableName();
}

void OscPage::resized()
{
    // Laid out the way Serum users expect: sub and noise on the left, the two
    // oscillators, the filter on the right, modulators below, then macros.
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() * 50 / 100);
    r.removeFromTop (8);
    macros.setBounds (r.removeFromBottom (juce::jmin (100, r.getHeight() / 3)));
    r.removeFromBottom (8);

    auto side = top.removeFromLeft (188);
    sub.setBounds (side.removeFromTop ((side.getHeight() - 8) / 2));
    side.removeFromTop (8);
    noise.setBounds (side);
    top.removeFromLeft (8);
    const int oscW = (top.getWidth() - 16) * 35 / 100;
    oscA.setBounds (top.removeFromLeft (oscW));
    top.removeFromLeft (8);
    oscB.setBounds (top.removeFromLeft (oscW));
    top.removeFromLeft (8);
    filter.setBounds (top);

    voicing.setBounds (r.removeFromRight (230));
    r.removeFromRight (8);
    mods.setBounds (r);
}

//==============================================================================
MatrixPage::MatrixPage (CruzSynthProcessor& p) : proc (p)
{
    rows.resize (kNumModSlots);
    for (int s = 0; s < kNumModSlots; ++s)
    {
        auto& row = rows[(size_t) s];
        row.src = std::make_unique<ParamCombo> (proc.apvts, id::slot (s, "src"));
        row.dst = std::make_unique<ParamCombo> (proc.apvts, id::slot (s, "dst"));
        row.bipolar = std::make_unique<ParamToggle> (proc.apvts, id::slot (s, "bipolar"), "BIPOLAR");
        row.bipolar->setTooltip ("Bipolar: the source swings both ways around the knob's position");
        row.amount = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        row.amount->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        row.amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id::slot (s, "amount"), *row.amount);
        row.amount->setDoubleClickReturnValue (true, 0.0);
        row.clear = std::make_unique<juce::TextButton> ("X");
        row.clear->setTooltip ("Clear this routing");
        row.clear->onClick = [this, s] { proc.clearModulationSlot (s); };
        for (auto* c : std::initializer_list<juce::Component*> { row.src.get(), row.dst.get(), row.bipolar.get(), row.amount.get(), row.clear.get() })
            addAndMakeVisible (c);
    }
}

void MatrixPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (8);
    drawPanel (g, r, {});
    g.setFont (font (12.0f, true));
    g.setColour (colours::textDim);
    auto header = r.reduced (16, 0).removeFromTop (34);
    header.removeFromLeft (36);
    g.drawText ("SOURCE", header.removeFromLeft (190), juce::Justification::centredLeft);
    g.drawText ("", header.removeFromLeft (96), juce::Justification::centredLeft);
    g.drawText ("AMOUNT", header.removeFromLeft (360), juce::Justification::centredLeft);
    g.drawText ("DESTINATION", header.removeFromLeft (260), juce::Justification::centredLeft);

    g.setFont (font (12.0f));
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const auto b = rows[(size_t) s].src->getBounds();
        g.setColour (colours::textDim);
        g.drawText (juce::String (s + 1), b.getX() - 34, b.getY(), 28, b.getHeight(), juce::Justification::centredRight);
        const int src = juce::roundToInt (proc.apvts.getRawParameterValue (id::slot (s, "src"))->load());
        if (src > 0)
        {
            g.setColour (modSourceColour (src));
            g.fillEllipse ((float) b.getX() - 6.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }
    g.setColour (colours::textDim);
    g.drawText ("Tip: drag ENV, LFO or macro chips from the OSC page straight onto a knob to fill a row here.",
                r.removeFromBottom (24).reduced (16, 0), juce::Justification::centredLeft);
}

void MatrixPage::resized()
{
    auto r = getLocalBounds().reduced (24, 8);
    r.removeFromTop (36);
    const int rowH = juce::jmin (34, (r.getHeight() - 26) / kNumModSlots);
    for (auto& row : rows)
    {
        auto line = r.removeFromTop (rowH).reduced (0, 4);
        line.removeFromLeft (20);
        row.src->setBounds (line.removeFromLeft (180));
        line.removeFromLeft (10);
        row.bipolar->setBounds (line.removeFromLeft (86));
        line.removeFromLeft (10);
        row.amount->setBounds (line.removeFromLeft (350));
        line.removeFromLeft (14);
        row.dst->setBounds (line.removeFromLeft (220));
        line.removeFromLeft (10);
        row.clear->setBounds (line.removeFromLeft (28));
    }
    repaint();
}

//==============================================================================
// One module in the rack: name and power on the left, menus and switches in a
// column, a row of knobs, and a live curve for the modules that have one.
class FxPage::Strip : public Panel
{
public:
    Strip (CruzSynthProcessor& p, int m) : Panel ({}), module (m)
    {
        power = &make<PowerButton> (p.apvts, fxOnParamID (m));
        auto knob = [&] (const char* pid, const char* label) { knobs.push_back (&make<ModKnob> (p, pid, label)); };
        auto combo = [&] (const char* pid) { side.push_back (&make<ParamCombo> (p.apvts, pid)); };
        auto toggle = [&] (const char* pid, const char* text) { side.push_back (&make<ParamToggle> (p.apvts, pid, text)); };

        switch (m)
        {
            case FxHyper:
                subtitle = "Unison for the whole sound, plus Widen";
                knob ("fx_hyp_rate", "Rate"); knob ("fx_hyp_detune", "Detune"); knob ("fx_hyp_voices", "Voices");
                knob ("fx_hyp_mix", "Mix"); knob ("fx_hyp_size", "Wide Size"); knob ("fx_hyp_wide", "Wide Mix");
                break;
            case FxDistortion:
                combo ("fx_dist_mode"); combo ("fx_dist_fmode"); combo ("fx_dist_ftype");
                static_cast<ParamCombo*> (side[1])->setTooltip ("Filter before (Pre) or after (Post) the distortion");
                knob ("fx_dist_drive", "Drive"); knob ("fx_dist_tone", "Tone"); knob ("fx_dist_ffreq", "Freq");
                knob ("fx_dist_fres", "Res"); knob ("fx_dist_mix", "Mix");
                curve = &make<FxCurveView> (p, FxCurveView::Distortion);
                break;
            case FxFlanger:
                toggle ("fx_fla_sync", "SYNC"); combo ("fx_fla_div");
                knob ("fx_fla_rate", "Rate"); knob ("fx_fla_depth", "Depth"); knob ("fx_fla_feedback", "Feedback");
                knob ("fx_fla_spread", "Stereo"); knob ("fx_fla_mix", "Mix");
                break;
            case FxPhaser:
                combo ("fx_pha_stages");
                static_cast<ParamCombo*> (side[0])->setTooltip ("Number of phaser stages: more stages, more notches");
                knob ("fx_pha_rate", "Rate"); knob ("fx_pha_depth", "Depth"); knob ("fx_pha_freq", "Freq");
                knob ("fx_pha_feedback", "Feedback"); knob ("fx_pha_spread", "Stereo"); knob ("fx_pha_mix", "Mix");
                break;
            case FxChorus:
                knob ("fx_cho_rate", "Rate"); knob ("fx_cho_depth", "Depth"); knob ("fx_cho_delay", "Delay");
                knob ("fx_cho_feedback", "Feedback"); knob ("fx_cho_hicut", "High Cut"); knob ("fx_cho_spread", "Stereo");
                knob ("fx_cho_mix", "Mix");
                break;
            case FxDelay:
                toggle ("fx_dly_sync", "SYNC"); combo ("fx_dly_div"); toggle ("fx_dly_pingpong", "PING-PONG");
                knob ("fx_dly_time", "Time"); knob ("fx_dly_feedback", "Feedback"); knob ("fx_dly_offset", "Offset R");
                knob ("fx_dly_lowcut", "Low Cut"); knob ("fx_dly_hicut", "High Cut"); knob ("fx_dly_mix", "Mix");
                break;
            case FxReverb:
                combo ("fx_rev_mode");
                knob ("fx_rev_size", "Size"); knob ("fx_rev_decay", "Decay"); knob ("fx_rev_predelay", "Pre-delay");
                knob ("fx_rev_damp", "Damping"); knob ("fx_rev_lowcut", "Low Cut"); knob ("fx_rev_hicut", "High Cut");
                knob ("fx_rev_width", "Width"); knob ("fx_rev_mix", "Mix");
                break;
            case FxCompressor:
                toggle ("fx_cmp_multi", "MULTIBAND");
                knob ("fx_cmp_thresh", "Thresh"); knob ("fx_cmp_ratio", "Ratio"); knob ("fx_cmp_attack", "Attack");
                knob ("fx_cmp_release", "Release"); knob ("fx_cmp_upward", "Upward"); knob ("fx_cmp_gain", "Gain");
                knob ("fx_cmp_mix", "Mix");
                break;
            case FxMultiband:
                subtitle = "Three-band upward and downward squash";
                knob ("fx_mb_depth", "Depth"); knob ("fx_mb_time", "Time"); knob ("fx_mb_down", "Down");
                knob ("fx_mb_up", "Up"); knob ("fx_mb_low", "Low"); knob ("fx_mb_mid", "Mid"); knob ("fx_mb_high", "High");
                knob ("fx_mb_gain", "Output");
                break;
            case FxEq:
                combo ("fx_eq_lowtype"); combo ("fx_eq_hightype");
                knob ("fx_eq_lowfreq", "Low Freq"); knob ("fx_eq_lowq", "Low Q"); knob ("fx_eq_lowgain", "Low Gain");
                knob ("fx_eq_highfreq", "High Freq"); knob ("fx_eq_highq", "High Q"); knob ("fx_eq_highgain", "High Gain");
                curve = &make<FxCurveView> (p, FxCurveView::Eq);
                break;
            case FxFilter:
                combo ("fx_fil_type");
                knob ("fx_fil_cutoff", "Cutoff"); knob ("fx_fil_res", "Res"); knob ("fx_fil_drive", "Drive");
                knob ("fx_fil_stereo", "Stereo"); knob ("fx_fil_mix", "Mix");
                curve = &make<FxCurveView> (p, FxCurveView::Filter);
                break;
            default: break;
        }
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, getLocalBounds(), {});
        g.setColour (colours::text);
        g.setFont (font (14.0f, true));
        g.drawText (fxModuleNames()[module].toUpperCase(), 34, 8, 110, 20, juce::Justification::centredLeft);
        if (subtitle.isNotEmpty())
        {
            g.setColour (colours::textDim);
            g.setFont (font (11.0f));
            g.drawFittedText (subtitle, 12, 34, 116, 40, juce::Justification::topLeft, 3);
        }
        g.setColour (colours::outline);
        g.drawVerticalLine (136, 8.0f, (float) getHeight() - 8.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        power->setBounds (10, 9, 18, 18);
        r.removeFromLeft (142);
        if (curve != nullptr)
            curve->setBounds (r.removeFromRight (220).reduced (8));
        if (! side.empty())
        {
            auto col = r.removeFromLeft (124).reduced (4, 10);
            for (auto* c : side)
            {
                c->setBounds (col.removeFromTop (24));
                col.removeFromTop (6);
            }
        }
        r = r.reduced (2, 6);
        const int w = juce::jmin (78, r.getWidth() / juce::jmax (1, (int) knobs.size()));
        for (auto* k : knobs)
            k->setBounds (r.removeFromLeft (w));
    }

    const int module;

private:
    PowerButton* power;
    FxCurveView* curve = nullptr;
    std::vector<juce::Component*> side;
    std::vector<ModKnob*> knobs;
    juce::String subtitle;
};

//==============================================================================
// A row in the chain list: power, name, and buttons to move it up or down.
class FxPage::ChainRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChainRow (CruzSynthProcessor& p, int m, std::function<void (int, int)> move, std::function<void (int)> select)
        : module (m), onSelect (std::move (select))
    {
        power = std::make_unique<PowerButton> (p.apvts, fxOnParamID (m));
        power->setTooltip ("Switch " + fxModuleNames()[m] + " on or off");
        up.setTooltip ("Move earlier in the chain");
        down.setTooltip ("Move later in the chain");
        up.onClick = [this, move] { move (module, -1); };
        down.onClick = [this, move] { move (module, 1); };
        for (auto* c : std::initializer_list<juce::Component*> { power.get(), &up, &down })
            addAndMakeVisible (c);
        setTooltip ("Click to jump to " + fxModuleNames()[m] + " (switches it on if it's off)");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = power->getToggleState();
        g.setColour (on ? colours::panelHi : colours::panel);
        g.fillRoundedRectangle (r, 4.0f);
        if (on)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (r.removeFromLeft (3.0f), 1.5f);
        }
        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (font (12.5f, on));
        g.drawText (fxModuleNames()[module], 34, 0, getWidth() - 90, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        power->setBounds (10, (getHeight() - 18) / 2, 18, 18);
        auto r = getLocalBounds().reduced (4, 4);
        down.setBounds (r.removeFromRight (22));
        r.removeFromRight (2);
        up.setBounds (r.removeFromRight (22));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked())
            onSelect (module);
    }

    const int module;
    std::unique_ptr<PowerButton> power;

private:
    juce::TextButton up { juce::String::charToString (0x25b2) }, down { juce::String::charToString (0x25bc) };
    std::function<void (int)> onSelect;
};

//==============================================================================
FxPage::FxPage (CruzSynthProcessor& p) : proc (p)
{
    for (int m = 0; m < kNumFx; ++m)
    {
        rows.push_back (std::make_unique<ChainRow> (proc, m, [this] (int mod, int dir) { move (mod, dir); },
                                                    [this] (int mod) { show (mod); }));
        addAndMakeVisible (*rows.back());
        strips.push_back (std::make_unique<Strip> (proc, m));
        rack.addChildComponent (*strips.back());
    }
    viewport.setViewedComponent (&rack, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    startTimerHz (10);
}

FxPage::~FxPage() = default;

juce::uint32 FxPage::onMask() const
{
    juce::uint32 mask = 0;
    for (int m = 0; m < kNumFx; ++m)
        if (proc.apvts.getRawParameterValue (fxOnParamID (m))->load() > 0.5f)
            mask |= 1u << m;
    return mask;
}

void FxPage::timerCallback()
{
    const auto mask = onMask();
    if (mask != lastMask)
    {
        lastMask = mask;
        layoutRack();
        for (auto& r : rows)
            r->repaint();
    }
}

void FxPage::refresh()
{
    resized();
    repaint();
}

void FxPage::move (int module, int dir)
{
    auto order = proc.getFxOrder();
    const auto it = std::find (order.begin(), order.end(), module);
    const int i = (int) std::distance (order.begin(), it);
    const int j = i + dir;
    if (j < 0 || j >= kNumFx)
        return;
    std::swap (order[(size_t) i], order[(size_t) j]);
    proc.setFxOrder (order);
    focus = module;
    resized();
}

void FxPage::show (int module)
{
    auto* on = proc.apvts.getParameter (fxOnParamID (module));
    if (on->getValue() < 0.5f)
        on->setValueNotifyingHost (1.0f);
    focus = module;
    timerCallback();
    layoutRack();
}

void FxPage::paint (juce::Graphics& g)
{
    drawPanel (g, listArea, "FX CHAIN");
    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawFittedText ("Top to bottom is the order the sound goes through. Click a name to jump to it.",
                      listArea.reduced (12, 10).removeFromBottom (44), juce::Justification::bottomLeft, 3);
    if (lastMask == 0)
    {
        g.setFont (font (15.0f));
        g.drawText ("Switch on an effect in the chain on the left to add it to the rack.", rackArea,
                    juce::Justification::centred);
    }
}

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    listArea = r.removeFromLeft (210);
    r.removeFromLeft (8);
    rackArea = r;
    viewport.setBounds (r);

    auto list = listArea.reduced (8, 0).withTrimmedTop (32);
    const auto order = proc.getFxOrder();
    for (auto m : order)
        rows[(size_t) m]->setBounds (list.removeFromTop (34).reduced (0, 2));
    layoutRack();
}

void FxPage::layoutRack()
{
    const auto order = proc.getFxOrder();
    const int w = viewport.getWidth() - viewport.getScrollBarThickness() - 4;
    const int h = 128;
    int y = 0;
    juce::Component* target = nullptr;
    for (auto m : order)
    {
        auto& s = *strips[(size_t) m];
        const bool on = proc.apvts.getRawParameterValue (fxOnParamID (m))->load() > 0.5f;
        s.setVisible (on);
        if (! on)
            continue;
        s.setBounds (0, y, w, h);
        if (m == focus)
            target = &s;
        y += h + 8;
    }
    rack.setSize (juce::jmax (1, w), juce::jmax (1, y - 8));
    if (target != nullptr)
        viewport.setViewPosition (0, juce::jlimit (0, juce::jmax (0, rack.getHeight() - viewport.getHeight()), target->getY()));
    focus = -1;
    repaint();
}

//==============================================================================
GlobalPage::GlobalPage (CruzSynthProcessor& p) : proc (p)
{
    auto add = [&] (std::unique_ptr<juce::Component> c)
    {
        addAndMakeVisible (*c);
        voicing.push_back (c.get());
        owned.push_back (std::move (c));
    };
    add (std::make_unique<ModKnob> (proc, "master_vol", "Master"));
    add (std::make_unique<ModKnob> (proc, "poly", "Voices"));
    add (std::make_unique<ParamToggle> (proc.apvts, "mono", "MONO"));
    add (std::make_unique<ParamToggle> (proc.apvts, "legato", "LEGATO"));
    add (std::make_unique<ModKnob> (proc, "glide", "Glide"));
    add (std::make_unique<ModKnob> (proc, "bend_range", "Bend Range"));
    add (std::make_unique<ModKnob> (proc, "vel_sens", "Velocity"));
}

void GlobalPage::paint (juce::Graphics& g)
{
    drawPanel (g, voicingArea, "VOICING");
    drawPanel (g, helpArea, "QUICK GUIDE");

    const juce::String help =
        "Play notes from FL Studio's piano roll, a MIDI keyboard, or the keys at the bottom of this window.\n\n"
        "OSCILLATORS  Pick a wavetable from the menu, then turn WT Pos to sweep through it. Unison stacks detuned copies "
        "for a wide sound. Warp bends the wave in different ways (try Sync or Bend). Edit opens the wavetable editor, where "
        "you can draw your own waves, edit harmonics, or import a WAV. You can also drop a WAV straight onto the wave display.\n\n"
        "MODULATION  Drag an ENV, LFO, macro or the VEL / NOTE / MW chips onto any knob. A coloured ring shows the range "
        "it will move. Hold Option and drag the knob to change how much, or right-click the knob to remove it. "
        "The MATRIX page lists every routing.\n\n"
        "LFOs  Click inside the LFO display to draw your own shape: click to add a point, drag to move it, double-click to "
        "delete it, Option-drag a line to curve it, Shift snaps to the grid.\n\n"
        "ENV 1 always shapes the volume. ENV 2 and ENV 3 are free to use anywhere.\n\n"
        "FX  The chain on the left lists all eleven effects. Switch one on with its round button and it appears in the "
        "rack; the arrows change the order. Effect knobs take modulation too: drag a macro or LFO onto them.\n\n"
        "PRESETS  Pick one from the menu at the top, or save your own with Save.";

    g.setColour (colours::text);
    g.setFont (font (14.0f));
    g.drawFittedText (help, helpArea.reduced (20, 34), juce::Justification::topLeft, 40, 1.0f);

    g.setColour (colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("Active voices: " + juce::String (proc.live.activeVoices.load()), voicingArea.reduced (14, 8).removeFromBottom (20),
                juce::Justification::centredLeft);
}

void GlobalPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    voicingArea = r.removeFromLeft (360);
    r.removeFromLeft (8);
    helpArea = r;
    layoutGrid (voicingArea.reduced (10, 0).withTrimmedTop (36).withHeight (3 * 96), voicing, 3, 96);
}
} // namespace cs::ui
