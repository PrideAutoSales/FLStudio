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
    row1 = { knob ("octave", "Oct"), knob ("semi", "Semi"), knob ("fine", "Fine"), knob ("coarse", "Pitch"),
             knob ("pos", "WT Pos"), knob ("warpAmt", "Warp") };
    row2 = { knob ("unison", "Unison"), knob ("detune", "Detune"), knob ("blend", "Blend"), knob ("width", "Width"),
             knob ("phase", "Phase"), knob ("rand", "Rand"), knob ("pan", "Pan"), knob ("level", "Level") };
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

    r = r.reduced (8, 4);
    auto v = r.removeFromTop (128);
    view->setBounds (v);
    warp->setBounds (v.getX() + 6, v.getY() + 6, 104, 20);
    r.removeFromTop (4);
    const int rowH = r.getHeight() / 2;
    auto a = r.removeFromTop (rowH);
    const int w1 = a.getWidth() / 6;
    for (auto* k : row1) k->setBounds (a.removeFromLeft (w1));
    const int w2 = r.getWidth() / 8;
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
    header.removeFromLeft (60);
    toFilter->setBounds (header.removeFromRight (40));
    header.removeFromRight (6);
    shape->setBounds (header);
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
    header.removeFromLeft (60);
    toFilter->setBounds (header.removeFromRight (40));
    header.removeFromRight (6);
    type->setBounds (header);
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
    auto controls = r.removeFromBottom (84);
    r.removeFromBottom (4);
    envView->setBounds (r);
    lfoView->setBounds (r);

    for (auto& set : envControls)
    {
        auto c = controls;
        const int w = c.getWidth() / (int) set.size();
        for (auto* k : set) k->setBounds (c.removeFromLeft (w));
    }
    for (auto& set : lfoControls)
    {
        auto c = controls;
        auto col1 = c.removeFromLeft (110).reduced (4, 8);
        set[0]->setBounds (col1.removeFromTop (col1.getHeight() / 2).reduced (0, 3));
        set[1]->setBounds (col1.reduced (0, 3));
        auto col2 = c.removeFromLeft (100).reduced (4, 8);
        set[2]->setBounds (col2.removeFromTop (col2.getHeight() / 2).reduced (0, 3));
        set[3]->setBounds (col2.reduced (0, 3));
        const int w = c.getWidth() / 4;
        // When synced the rate knob hides and the division menu (set[3]) is shown instead.
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
    auto r = getLocalBounds().reduced (8, 4);
    r.removeFromTop (kHeader - 4);
    auto grid = r.removeFromTop (196);
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto cell = juce::Rectangle<int> (grid.getX() + (m % 2) * grid.getWidth() / 2, grid.getY() + (m / 2) * 98,
                                          grid.getWidth() / 2, 98);
        macroChips[(size_t) m]->setBounds (cell.removeFromBottom (22).reduced (16, 1));
        knobs[(size_t) m]->setBounds (cell.reduced (10, 0));
    }
    r.removeFromTop (6);
    const int cw = r.getWidth() / 3;
    for (size_t i = 0; i < otherChips.size(); ++i)
        otherChips[i]->setBounds (r.getX() + (int) (i % 3) * cw + 2, r.getY() + (int) (i / 3) * 26, cw - 4, 22);
}

//==============================================================================
OscPage::OscPage (CruzSynthProcessor& p, std::function<void (int)> onEditTable)
    : oscA (p, 0, onEditTable), oscB (p, 1, onEditTable), sub (p), noise (p), filter (p), mods (p), macros (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &oscA, &oscB, &sub, &noise, &filter, &mods, &macros })
        addAndMakeVisible (c);
}

void OscPage::refresh()
{
    oscA.refreshTableName();
    oscB.refreshTableName();
}

void OscPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (r.getHeight() * 53 / 100);
    r.removeFromTop (8);

    const int oscW = (top.getWidth() - 16) * 37 / 100;
    oscA.setBounds (top.removeFromLeft (oscW));
    top.removeFromLeft (8);
    oscB.setBounds (top.removeFromLeft (oscW));
    top.removeFromLeft (8);
    sub.setBounds (top.removeFromTop ((top.getHeight() - 8) / 2));
    top.removeFromTop (8);
    noise.setBounds (top);

    filter.setBounds (r.removeFromLeft (oscW - 60));
    r.removeFromLeft (8);
    macros.setBounds (r.removeFromRight (230));
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
class FxPage::FxPanel : public Panel
{
public:
    FxPanel (CruzSynthProcessor& p, int m, std::function<void (int, int)> move)
        : Panel (fxModuleNames()[m].toUpperCase()), module (m)
    {
        static const char* onIds[] = { "fx_dist_on", "fx_cho_on", "fx_pha_on", "fx_dly_on", "fx_rev_on", "fx_cmp_on", "fx_eq_on" };
        power = &make<PowerButton> (p.apvts, onIds[m]);
        left = &make<juce::TextButton> ("<");
        right = &make<juce::TextButton> (">");
        left->setTooltip ("Move earlier in the chain");
        right->setTooltip ("Move later in the chain");
        left->onClick = [this, move] { move (module, -1); };
        right->onClick = [this, move] { move (module, 1); };

        auto knob = [&] (const char* pid, const char* label) { items.push_back (&make<ModKnob> (p, pid, label)); };
        auto combo = [&] (const char* pid) { items.push_back (&make<ParamCombo> (p.apvts, pid)); };
        auto toggle = [&] (const char* pid, const char* text) { items.push_back (&make<ParamToggle> (p.apvts, pid, text)); };

        switch (m)
        {
            case FxDistortion: combo ("fx_dist_mode"); knob ("fx_dist_drive", "Drive"); knob ("fx_dist_tone", "Tone"); knob ("fx_dist_mix", "Mix"); break;
            case FxChorus: knob ("fx_cho_rate", "Rate"); knob ("fx_cho_depth", "Depth"); knob ("fx_cho_delay", "Delay");
                           knob ("fx_cho_feedback", "Feedback"); knob ("fx_cho_mix", "Mix"); break;
            case FxPhaser: knob ("fx_pha_rate", "Rate"); knob ("fx_pha_depth", "Depth"); knob ("fx_pha_freq", "Freq");
                           knob ("fx_pha_feedback", "Feedback"); knob ("fx_pha_mix", "Mix"); break;
            case FxDelay: toggle ("fx_dly_sync", "SYNC"); combo ("fx_dly_div"); toggle ("fx_dly_pingpong", "PING-PONG");
                          knob ("fx_dly_time", "Time"); knob ("fx_dly_feedback", "Feedback"); knob ("fx_dly_hicut", "Hi Cut");
                          knob ("fx_dly_mix", "Mix"); break;
            case FxReverb: knob ("fx_rev_size", "Size"); knob ("fx_rev_damp", "Damping"); knob ("fx_rev_width", "Width"); knob ("fx_rev_mix", "Mix"); break;
            case FxCompressor: toggle ("fx_cmp_multi", "MULTIBAND"); knob ("fx_cmp_thresh", "Thresh"); knob ("fx_cmp_ratio", "Ratio");
                               knob ("fx_cmp_attack", "Attack"); knob ("fx_cmp_release", "Release"); knob ("fx_cmp_upward", "Upward");
                               knob ("fx_cmp_gain", "Gain"); knob ("fx_cmp_mix", "Mix"); break;
            default: knob ("fx_eq_lowfreq", "Low Freq"); knob ("fx_eq_lowgain", "Low Gain");
                     knob ("fx_eq_highfreq", "High Freq"); knob ("fx_eq_highgain", "High Gain"); break;
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        placePower (*power, r);
        auto header = r.removeFromTop (kHeader).reduced (8, 4);
        right->setBounds (header.removeFromRight (24));
        header.removeFromRight (2);
        left->setBounds (header.removeFromRight (24));
        layoutGrid (r.reduced (6, 2), items, 3, juce::jmin (84, (r.getHeight() - 4) / 3));
    }

    const int module;

private:
    PowerButton* power;
    juce::TextButton* left;
    juce::TextButton* right;
    std::vector<juce::Component*> items;
};

FxPage::FxPage (CruzSynthProcessor& p) : proc (p)
{
    for (int m = 0; m < kNumFx; ++m)
    {
        panels.push_back (std::make_unique<FxPanel> (proc, m, [this] (int module, int dir)
        {
            auto order = proc.getFxOrder();
            const auto it = std::find (order.begin(), order.end(), module);
            const int i = (int) std::distance (order.begin(), it);
            const int j = i + dir;
            if (j < 0 || j >= kNumFx)
                return;
            std::swap (order[(size_t) i], order[(size_t) j]);
            proc.setFxOrder (order);
            resized();
        }));
        addAndMakeVisible (*panels.back());
    }
}

FxPage::~FxPage() = default;

void FxPage::resized()
{
    auto r = getLocalBounds().reduced (8);
    const auto order = proc.getFxOrder();
    const int cols = 4;
    const int w = (r.getWidth() - (cols - 1) * 8) / cols;
    const int h = (r.getHeight() - 8) / 2;
    for (int i = 0; i < kNumFx; ++i)
    {
        auto& panel = *panels[(size_t) order[(size_t) i]];
        panel.setBounds (r.getX() + (i % cols) * (w + 8), r.getY() + (i / cols) * (h + 8), w, h);
    }
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
        "FX  Turn effects on with their round buttons and change the order with the arrows.\n\n"
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
