#include "PluginEditor.h"

using Palette = ChilltronLookAndFeel::Palette;

namespace chilltron::ui
{
    //==============================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
        : attachment (state, paramId, slider), caption (text.toUpperCase())
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setDoubleClickReturnValue (true, (double) state.getParameter (paramId)->convertFrom0to1 (
                                                    state.getParameter (paramId)->getDefaultValue()));
        slider.addMouseListener (this, false);
        slider.onValueChange = [this] { repaint(); };
        slider.onDragEnd = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    void Knob::paint (juce::Graphics& g)
    {
        const auto showValue = slider.isMouseOverOrDragging();
        const auto text = showValue ? slider.getTextFromValue (slider.getValue()) : caption;

        auto area = getLocalBounds().removeFromBottom (18).toFloat();
        if (showValue)
        {
            const auto pill = area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 2.0f, 70.0f), 16.0f);
            g.setColour (Palette::ink);
            g.fillRoundedRectangle (pill, 3.0f);
            g.setColour (Palette::amber);
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        }
        else
        {
            g.setColour (Palette::ink);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        }
        g.drawText (text, area, juce::Justification::centred);
    }

    void Knob::resized()
    {
        auto r = getLocalBounds();
        r.removeFromBottom (20);
        const auto size = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (size, size));
    }

    //==============================================================================
    TapeRow::TapeRow (PluginProcessor& p, const juce::String& paramId, const juce::String& b)
        : processor (p), badge (b),
          attachment (*p.apvts.getParameter (paramId),
                      [this] (float value) { index = juce::roundToInt (value); repaint(); })
    {
        attachment.sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::Rectangle<float> TapeRow::leftArrow() const
    {
        return getLocalBounds().toFloat().removeFromRight (56.0f).removeFromLeft (28.0f);
    }

    juce::Rectangle<float> TapeRow::rightArrow() const
    {
        return getLocalBounds().toFloat().removeFromRight (28.0f);
    }

    void TapeRow::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        const auto mouse = getMouseXYRelative().toFloat();
        const auto hovering = isMouseOver();

        // Badge: a lit letter in a box.
        const auto badgeBox = r.removeFromLeft (r.getHeight()).reduced (3.0f);
        g.setColour (Palette::amber);
        g.drawRoundedRectangle (badgeBox, 3.0f, 1.2f);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (badge, badgeBox, juce::Justification::centred);
        r.removeFromLeft (10.0f);

        // Arrows.
        for (auto [box, left] : { std::pair { leftArrow(), true }, std::pair { rightArrow(), false } })
        {
            const auto c = box.getCentre();
            juce::Path tri;
            if (left) tri.addTriangle (c.x + 4.0f, c.y - 6.0f, c.x + 4.0f, c.y + 6.0f, c.x - 5.0f, c.y);
            else      tri.addTriangle (c.x - 4.0f, c.y - 6.0f, c.x - 4.0f, c.y + 6.0f, c.x + 5.0f, c.y);
            g.setColour (Palette::amber.withAlpha (hovering && box.contains (mouse) ? 1.0f : 0.45f));
            g.fillPath (tri);
        }
        r.removeFromRight (60.0f);

        // Name, and what the library is doing with it.
        const auto status = processor.library->getStatus (index);
        auto statusArea = r.removeFromRight (78.0f);
        if (status != TapeLibrary::Status::ready)
        {
            const auto missing = status == TapeLibrary::Status::missing;
            g.setColour (missing ? Palette::red.brighter (0.5f) : Palette::amber.withAlpha (0.6f));
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (missing ? "MISSING" : "LOADING", statusArea, juce::Justification::centredRight);
        }

        const auto nameHover = hovering && r.contains (mouse);
        g.setColour (status == TapeLibrary::Status::missing ? Palette::amber.withAlpha (0.4f) : Palette::amber);
        g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
        g.drawText (tapeDisplayNames()[index].toUpperCase(), r, juce::Justification::centredLeft);

        if (nameHover)
        {
            g.setColour (Palette::amber.withAlpha (0.35f));
            g.fillRect (r.removeFromBottom (1.0f));
        }
    }

    void TapeRow::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        if (leftArrow().contains (p))       step (-1);
        else if (rightArrow().contains (p)) step (1);
        else                                showMenu();
    }

    void TapeRow::step (int delta)
    {
        attachment.setValueAsCompleteGesture ((float) ((index + delta + numTapes) % numTapes));
    }

    void TapeRow::showMenu()
    {
        juce::PopupMenu menu;
        const auto names = tapeDisplayNames();
        for (int i = 0; i < names.size(); ++i)
        {
            const auto missing = processor.library->getStatus (i) == TapeLibrary::Status::missing;
            menu.addItem (juce::PopupMenu::Item (names[i]).setID (i + 1).setTicked (i == index).setEnabled (! missing));
        }

        juce::Component::SafePointer<TapeRow> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (220),
                            [safe] (int result)
                            {
                                if (safe != nullptr && result > 0)
                                    safe->attachment.setValueAsCompleteGesture ((float) (result - 1));
                            });
    }

    //==============================================================================
    TapeDisplay::TapeDisplay (PluginProcessor& p)
        : processor (p),
          rowA (p, ParamIDs::tapeA, "A"),
          rowB (p, ParamIDs::tapeB, "B"),
          blendAttachment (p.apvts, ParamIDs::blend, blend)
    {
        blend.setSliderStyle (juce::Slider::LinearHorizontal);
        blend.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        blend.setDoubleClickReturnValue (true, 50.0);
        blend.onValueChange = [this] { repaint(); };

        folderButton.setColour (juce::TextButton::buttonColourId, Palette::glassEdge);
        folderButton.setColour (juce::TextButton::textColourOffId, Palette::amber);
        folderButton.setTooltip ("Choose the folder holding the .sf2 tapes");
        folderButton.onClick = [this] { chooseFolder(); };

        addAndMakeVisible (rowA);
        addAndMakeVisible (rowB);
        addAndMakeVisible (blend);
        addAndMakeVisible (folderButton);
    }

    juce::Rectangle<int> TapeDisplay::stripArea() const
    {
        auto r = getLocalBounds().reduced (14, 12);
        r.removeFromLeft (juce::roundToInt ((float) r.getWidth() * 0.47f) + 20);
        return r;
    }

    void TapeDisplay::resized()
    {
        auto r = getLocalBounds().reduced (14, 12);
        auto left = r.removeFromLeft (juce::roundToInt ((float) r.getWidth() * 0.47f));

        rowA.setBounds (left.removeFromTop (34));
        left.removeFromTop (6);
        rowB.setBounds (left.removeFromTop (34));
        left.removeFromTop (12);

        auto blendRow = left.removeFromTop (28);
        blendRow.removeFromLeft (44);
        blendRow.removeFromRight (40);
        blend.setBounds (blendRow);

        folderButton.setBounds (left.removeFromBottom (22).removeFromLeft (80));
    }

    void TapeDisplay::paint (juce::Graphics& g)
    {
        ChilltronLookAndFeel::paintGlass (g, getLocalBounds().toFloat());

        // Divider between the banks and the strips.
        const auto strips = stripArea();
        g.setColour (Palette::amberDim.withAlpha (0.5f));
        g.fillRect (strips.getX() - 11, strips.getY(), 1, strips.getHeight());

        // Blend labels and readout.
        {
            const auto b = blend.getBounds();
            const auto amount = (float) blend.getValue() / 100.0f;
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.setColour (Palette::amber.withAlpha (1.0f - 0.6f * amount));
            g.drawText ("A", b.withX (b.getX() - 40).withWidth (34), juce::Justification::centredRight);
            g.setColour (Palette::amber.withAlpha (0.4f + 0.6f * amount));
            g.drawText ("B", b.withX (b.getRight() + 6).withWidth (34), juce::Justification::centredLeft);

            g.setColour (Palette::amber.withAlpha (0.55f));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("BLEND  " + juce::String (juce::roundToInt (100.0f - 100.0f * amount)) + " / "
                            + juce::String (juce::roundToInt (100.0f * amount)),
                        b.translated (0, 20).withHeight (14), juce::Justification::centred);
        }

        // Tape folder, beside its button.
        {
            const auto fb = folderButton.getBounds();
            g.setColour (Palette::amber.withAlpha (0.4f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawFittedText (processor.library->getFolder().getFullPathName(),
                              fb.withX (fb.getRight() + 8).withRight (strips.getX() - 20), juce::Justification::centredLeft, 1);
        }

        // One strip per key: the lit length is how much tape is still on the reel side.
        const auto& engine = processor.getEngine();
        const auto label = strips.withTop (strips.getBottom() - 14);
        const auto meters = strips.withTrimmedBottom (18).withTrimmedTop (4).toFloat();
        const auto count = highestKey - lowestKey + 1;
        const auto pitch = meters.getWidth() / (float) count;

        for (int key = lowestKey; key <= highestKey; ++key)
        {
            const auto i = key - lowestKey;
            const auto slot = juce::Rectangle<float> (meters.getX() + pitch * (float) i + pitch * 0.2f, meters.getY(),
                                                      pitch * 0.6f, meters.getHeight());
            const auto used = juce::jlimit (0.0f, 1.0f, engine.getTapePosition (key) / (float) tapeSeconds);
            const auto sounding = engine.isSounding (key);

            g.setColour (Palette::amberDim.withAlpha (0.35f));
            g.fillRoundedRectangle (slot, 1.5f);

            const auto remaining = slot.withTrimmedTop (slot.getHeight() * used);
            g.setColour (sounding ? Palette::amber : Palette::amber.withAlpha (used > 0.0f ? 0.55f : 0.22f));
            g.fillRoundedRectangle (remaining, 1.5f);

            if (sounding)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.fillRect (remaining.withHeight (1.5f));
            }

            if (key % 12 == 0)
            {
                g.setColour (Palette::amber.withAlpha (0.55f));
                g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
                g.drawText ("C" + juce::String (key / 12 - 1),
                            juce::Rectangle<float> (slot.getCentreX() - 15.0f, (float) label.getY(), 30.0f, (float) label.getHeight()),
                            juce::Justification::centred);
            }
        }

        // If neither tape can be found, say where we looked instead of showing a dead window.
        const auto a = processor.library->getStatus (rowA.getIndex());
        const auto b = processor.library->getStatus (rowB.getIndex());
        if (a == TapeLibrary::Status::missing && b == TapeLibrary::Status::missing)
        {
            g.setColour (Palette::glass.withAlpha (0.85f));
            g.fillRect (strips);
            g.setColour (Palette::amber);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawFittedText ("NO TAPES FOUND\nPress TAPES... and choose the folder with the Redtron .sf2 files.",
                              strips.reduced (10), juce::Justification::centred, 4);
        }
    }

    void TapeDisplay::chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose the Chilltron tape folder", processor.library->getFolder());
        juce::Component::SafePointer<TapeDisplay> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  const auto folder = fc.getResult();
                                  if (safe != nullptr && folder.isDirectory())
                                  {
                                      safe->processor.library->setFolder (folder);
                                      safe->repaint();
                                  }
                              });
    }
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      presetBar (p.presets),
      display (p),
      pitch    (p.apvts, ParamIDs::pitch, "Pitch"),
      wow      (p.apvts, ParamIDs::wow, "Wow"),
      click    (p.apvts, ParamIDs::click, "Click"),
      loopAttachment (p.apvts, ParamIDs::loop, loop),
      attack   (p.apvts, ParamIDs::attack, "Attack"),
      release  (p.apvts, ParamIDs::release, "Release"),
      velocityAttachment (p.apvts, ParamIDs::velocity, velocity),
      tone     (p.apvts, ParamIDs::tone, "Tone"),
      spread   (p.apvts, ParamIDs::spread, "Spread"),
      reverb   (p.apvts, ParamIDs::reverb, "Reverb"),
      volume   (p.apvts, ParamIDs::volume, "Volume"),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel.get());

    keyboard.setAvailableRange (chilltron::lowestKey, chilltron::highestKey);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);   // C4 = 60, matching the display
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff4eedf));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, Palette::bakelite);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9c9282));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::amber.withAlpha (0.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::red.withAlpha (0.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, Palette::inkDim);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (display);
    for (auto* k : { &pitch, &wow, &click, &attack, &release, &tone, &spread, &reverb, &volume })
        addAndMakeVisible (*k);
    addAndMakeVisible (loop);
    addAndMakeVisible (velocity);
    velocity.setTooltip ("On: notes play louder or softer with their velocity. Off: every note plays at full level, like the original machine.");
    addAndMakeVisible (keyboard);

    setSize (920, 556);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // Cream fascia, slightly warmer towards the bottom.
    g.setGradientFill (juce::ColourGradient (Palette::cream, 0.0f, 0.0f,
                                             Palette::cream.interpolatedWith (Palette::creamShade, 0.5f),
                                             0.0f, (float) bounds.getHeight(), false));
    g.fillAll();

    // Top strip: logo and preset bar.
    auto top = bounds.withHeight (at::LookAndFeel::headerHeight);
    g.setColour (Palette::topBar);
    g.fillRect (top);
    g.setColour (Palette::red);
    g.fillRect (top.removeFromBottom (2));

    auto logo = top.withTrimmedLeft (cheekWidth + 14);
    g.setColour (Palette::red);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold).withKerningFactor (0.14f));
    g.drawText ("CHILLTRON", logo, juce::Justification::centredLeft);

    logo.removeFromLeft (206);
    g.setColour (Palette::cream.withAlpha (0.55f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
    g.drawText ("TAPE REPLAY KEYBOARD", logo.removeFromTop (logo.getHeight() / 2 + 1), juce::Justification::bottomLeft);
    g.setColour (Palette::cream.withAlpha (0.35f));
    g.setFont (juce::FontOptions (9.5f));
    g.drawText (juce::String ("ANIMAL TESTING  v") + JucePlugin_VersionString, logo, juce::Justification::topLeft);

    // Walnut end cheeks.
    const auto body = bounds.withTrimmedTop (at::LookAndFeel::headerHeight);
    ChilltronLookAndFeel::paintWood (g, body.withWidth (cheekWidth).toFloat(), 7, true);
    ChilltronLookAndFeel::paintWood (g, body.withLeft (body.getRight() - cheekWidth).toFloat(), 11, true);

    // Silkscreened group frames.
    static const char* titles[] = { "TAPE", "VOICE", "OUTPUT" };
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const auto r = groups[i].toFloat();
        g.setColour (Palette::ink.withAlpha (0.55f));
        g.drawRoundedRectangle (r.withTrimmedTop (7.0f), 5.0f, 1.0f);

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.15f));
        const auto title = juce::String (titles[i]);
        const auto w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title) + 16.0f;
        const auto tag = juce::Rectangle<float> (w, 14.0f).withCentre ({ r.getCentreX(), r.getY() + 7.0f });
        g.setColour (Palette::cream);
        g.fillRect (tag);
        g.setColour (Palette::red);
        g.drawText (title, tag, juce::Justification::centred);
    }

    // Maroon key felt.
    g.setColour (Palette::felt);
    g.fillRect (feltArea);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRect (feltArea.withHeight (1));
}

void PluginEditor::resized()
{
    auto top = getLocalBounds().removeFromTop (at::LookAndFeel::headerHeight).withTrimmedBottom (2);
    presetBar.setBounds (top.removeFromRight (380).withTrimmedRight (cheekWidth + 4).withSizeKeepingCentre (346, 26));

    auto area = getLocalBounds().withTrimmedTop (at::LookAndFeel::headerHeight)
                                .withTrimmedLeft (cheekWidth).withTrimmedRight (cheekWidth);

    // Keyboard first, from the bottom: 21 white keys filling the width.
    auto keys = area.removeFromBottom (150);
    feltArea = area.removeFromBottom (12);
    keyboard.setKeyWidth ((float) keys.getWidth() / 21.0f);
    keyboard.setBounds (keys);
    keyboard.setLowestVisibleKey (chilltron::lowestKey);

    area.reduce (16, 0);
    area.removeFromTop (14);
    display.setBounds (area.removeFromTop (178));
    area.removeFromTop (12);
    area.removeFromBottom (12);

    // Three framed groups; each knob slot is the same width.
    const int units = 4 + 3 + 4;
    const int gap = 14;
    const auto unit = (area.getWidth() - 2 * gap - 3 * 12) / units;
    const auto knobsIn = [&] (juce::Rectangle<int> frame, std::initializer_list<juce::Component*> controls)
    {
        auto inner = frame.withTrimmedTop (16).reduced (6, 4);
        for (auto* c : controls)
        {
            const auto slot = inner.removeFromLeft (unit);
            c->setBounds (slot.withSizeKeepingCentre (unit, juce::jmin (slot.getHeight(), unit + 16)));
        }
    };

    auto row = area;
    groups[0] = row.removeFromLeft (unit * 4 + 12);
    row.removeFromLeft (gap);
    groups[1] = row.removeFromLeft (unit * 3 + 12);
    row.removeFromLeft (gap);
    groups[2] = row;

    knobsIn (groups[0], { &pitch, &wow, &click, &loop });
    knobsIn (groups[1], { &attack, &release, &velocity });
    knobsIn (groups[2], { &tone, &spread, &reverb, &volume });
}
