#include "PluginEditor.h"

using namespace cs::ui;

CruzSynthEditor::CruzSynthEditor (CruzSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      masterKnob (p, "master_vol", "Master"),
      scope (p),
      oscPage (p, [this] (int osc) { openTableEditor (osc); }),
      matrixPage (p), fxPage (p), globalPage (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setClickingTogglesState (false);
        tabs[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabs[i]);
    }

    presetButton.setTooltip ("Choose a preset, or save and load your own");
    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetButton, &prevButton, &nextButton, &saveButton, &masterKnob, &scope })
        addAndMakeVisible (c);

    for (auto* c : std::initializer_list<juce::Component*> { &oscPage, &matrixPage, &fxPage, &globalPage })
        addChildComponent (c);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9d5e3));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1d1b26));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (16.0f);
    addAndMakeVisible (keyboard);

    proc.stateChanged.addChangeListener (this);
    presetButton.setButtonText (proc.getPresetName());
    showPage (0);
    setSize (1120, 790);
    startTimerHz (10);
}

CruzSynthEditor::~CruzSynthEditor()
{
    proc.stateChanged.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void CruzSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Logo
    auto header = getLocalBounds().removeFromTop (52).reduced (16, 0);
    g.setColour (colours::accent);
    g.setFont (font (22.0f, true));
    g.drawText ("CRUZ", header.removeFromLeft (64), juce::Justification::centredLeft);
    g.setColour (colours::text);
    g.setFont (font (22.0f));
    g.drawText ("SYNTH", header.removeFromLeft (80), juce::Justification::centredLeft);

    g.setColour (colours::outline);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());

    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawText (juce::String (proc.live.activeVoices.load()) + " voices", getWidth() - 286, 18, 62, 16, juce::Justification::centred);
}

void CruzSynthEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (52).reduced (16, 10);
    header.removeFromLeft (150);
    for (auto& t : tabs)
        t.setBounds (header.removeFromLeft (76).reduced (2, 0));
    header.removeFromLeft (20);

    scope.setBounds (header.removeFromRight (140));
    header.removeFromRight (6);
    masterKnob.setBounds (header.removeFromRight (56).withTrimmedTop (-8).withTrimmedBottom (-8));
    header.removeFromRight (70);

    saveButton.setBounds (header.removeFromRight (56));
    header.removeFromRight (6);
    nextButton.setBounds (header.removeFromRight (28));
    header.removeFromRight (2);
    presetButton.setBounds (header.removeFromRight (200));
    header.removeFromRight (2);
    prevButton.setBounds (header.removeFromRight (28));

    keyboard.setBounds (r.removeFromBottom (56).reduced (8, 4));
    for (auto* c : std::initializer_list<juce::Component*> { &oscPage, &matrixPage, &fxPage, &globalPage })
        c->setBounds (r);
    if (tableEditor != nullptr)
        tableEditor->setBounds (r);
}

void CruzSynthEditor::showPage (int index)
{
    page = index;
    juce::Component* pages[] = { &oscPage, &fxPage, &matrixPage, &globalPage };
    for (int i = 0; i < 4; ++i)
    {
        pages[i]->setVisible (i == index);
        tabs[i].setToggleState (i == index, juce::dontSendNotification);
    }
    closeTableEditor();
}

void CruzSynthEditor::openTableEditor (int osc)
{
    tableEditor = std::make_unique<WavetableEditor> (proc, osc, [this] { closeTableEditor(); });
    addAndMakeVisible (*tableEditor);
    resized();
}

void CruzSynthEditor::closeTableEditor()
{
    if (tableEditor != nullptr)
    {
        // Defer deletion: this can be called from one of the editor's own buttons.
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CruzSynthEditor> (this)]
        {
            if (safe != nullptr)
                safe->tableEditor.reset();
        });
        tableEditor->setVisible (false);
    }
}

void CruzSynthEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    oscPage.refresh();
    fxPage.refresh();
    presetButton.setButtonText (proc.getPresetName());
    if (tableEditor != nullptr && tableEditor->isVisible())
        tableEditor->reload();
}

void CruzSynthEditor::timerCallback()
{
    const int v = proc.live.activeVoices.load();
    if (v != lastVoices)
    {
        lastVoices = v;
        repaint (getWidth() - 290, 10, 70, 30);
        if (page == 3)
            globalPage.repaint();
    }
}

void CruzSynthEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Factory");
    const auto names = proc.getFactoryPresetNames();
    m.addItem (1, names[0], true, names[0] == proc.getPresetName());

    // One submenu per category, in the order the bank lists them.
    juce::StringArray categories;
    for (int i = 1; i < names.size(); ++i)
        categories.addIfNotAlreadyThere (proc.getFactoryPresetCategory (i));
    for (const auto& cat : categories)
    {
        juce::PopupMenu sub;
        bool holdsCurrent = false;
        for (int i = 1; i < names.size(); ++i)
            if (proc.getFactoryPresetCategory (i) == cat)
            {
                const bool current = names[i] == proc.getPresetName();
                holdsCurrent = holdsCurrent || current;
                sub.addItem (i + 1, names[i], true, current);
            }
        m.addSubMenu (cat, sub, true, nullptr, holdsCurrent);
    }

    const auto folder = CruzSynthProcessor::getUserPresetFolder();
    const auto files = folder.findChildFiles (juce::File::findFiles, false, "*.cruzsynth");
    juce::Array<juce::File> sorted (files);
    sorted.sort();
    if (! sorted.isEmpty())
    {
        m.addSectionHeader ("Your presets");
        for (int i = 0; i < sorted.size(); ++i)
            m.addItem (1000 + i, sorted[i].getFileNameWithoutExtension());
    }
    m.addSeparator();
    m.addItem (900, "Save preset...");
    m.addItem (901, "Load preset from file...");
    m.addItem (902, "Show presets folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this, sorted, folder] (int r)
    {
        if (r <= 0) return;
        if (r < 900) proc.loadFactoryPreset (r - 1);
        else if (r == 900) savePreset();
        else if (r == 901) loadPreset();
        else if (r == 902) { folder.createDirectory(); folder.revealToUser(); }
        else if (r >= 1000) proc.loadUserPreset (sorted[r - 1000]);
    });
}

void CruzSynthEditor::stepPreset (int delta)
{
    const int n = proc.getFactoryPresetNames().size();
    int i = proc.getFactoryPresetNames().indexOf (proc.getPresetName());
    i = i < 0 ? 0 : (i + delta + n) % n;
    proc.loadFactoryPreset (i);
}

void CruzSynthEditor::savePreset()
{
    auto folder = CruzSynthProcessor::getUserPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile (proc.getPresetName() + ".cruzsynth"), "*.cruzsynth");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File())
                                  proc.saveUserPreset (f.withFileExtension ("cruzsynth"));
                              presetButton.setButtonText (proc.getPresetName());
                          });
}

void CruzSynthEditor::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Load preset", CruzSynthProcessor::getUserPresetFolder(), "*.cruzsynth");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File() && ! proc.loadUserPreset (f))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load preset",
                                                                          "That file isn't a Cruz Synth preset.");
                          });
}
