#include "PluginEditor.h"

namespace
{
    constexpr float chipHeight = 34.0f, chipGap = 6.0f;

    juce::Font monoFont (float height, bool bold = true)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    /** A raised rubber key: a darker lip underneath, the face on top. */
    void drawKey (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour face, bool down, float corner = 7.0f)
    {
        const auto lip = down ? 1.0f : 3.0f;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), corner);
        g.setColour (face.darker (0.35f));
        g.fillRoundedRectangle (r.withTrimmedTop (lip), corner);

        const auto top = r.withTrimmedBottom (lip).translated (0.0f, down ? 2.0f : 0.0f);
        g.setGradientFill (juce::ColourGradient (face.brighter (0.08f), top.getX(), top.getY(),
                                                 face.darker (0.06f), top.getX(), top.getBottom(), false));
        g.fillRoundedRectangle (top, corner);
    }
}

//==============================================================================
ToyLookAndFeel::ToyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,         Toy::cream);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,    Toy::glow.withAlpha (0.4f));
    setColour (juce::Label::textColourId,                 Toy::cream);
    setColour (juce::ComboBox::backgroundColourId,        Toy::screen);
    setColour (juce::ComboBox::textColourId,              Toy::glow);
    setColour (juce::ComboBox::outlineColourId,           Toy::caseDeep);
    setColour (juce::ComboBox::arrowColourId,             Toy::glow.withAlpha (0.7f));
    setColour (juce::PopupMenu::backgroundColourId,       Toy::screen);
    setColour (juce::PopupMenu::textColourId,             Toy::glow);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Toy::glow);
    setColour (juce::PopupMenu::highlightedTextColourId,  Toy::screen);
    setColour (juce::TextButton::buttonColourId,          Toy::key);
    setColour (juce::TextButton::buttonOnColourId,        Toy::key);
    setColour (juce::TextButton::textColourOffId,         Toy::navy);
    setColour (juce::TextButton::textColourOnId,          Toy::navy);
    setColour (juce::TextEditor::backgroundColourId,      Toy::screen);
    setColour (juce::TextEditor::textColourId,            Toy::glow);
    setColour (juce::TextEditor::outlineColourId,         Toy::caseDeep);
    setColour (juce::TextEditor::focusedOutlineColourId,  Toy::glow);
    setColour (juce::TooltipWindow::backgroundColourId,   Toy::navy);
    setColour (juce::TooltipWindow::textColourId,         Toy::cream);
    setColour (juce::TooltipWindow::outlineColourId,      Toy::navy);
}

void ToyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    // Tick marks printed on the case.
    g.setColour (Toy::cream.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto inner = centre.getPointOnCircumference (radius * 0.86f, a);
        const auto outer = centre.getPointOnCircumference (radius * (i % 5 == 0 ? 1.0f : 0.95f), a);
        g.drawLine ({ inner, outer }, i % 5 == 0 ? 2.0f : 1.2f);
    }

    // The value, as a lit arc just inside the ticks.
    if (slider.isEnabled())
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f, startAngle, angle, true);
        g.setColour (Toy::cream);
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // A cream knob with a shadow and a navy pointer.
    const auto knobR = radius * 0.64f;
    const auto knob = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (knob.translated (0.0f, 3.0f));
    g.setGradientFill (juce::ColourGradient (Toy::key, centre.x, knob.getY(),
                                             Toy::plate.darker (0.12f), centre.x, knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (Toy::plate.darker (0.3f));
    g.drawEllipse (knob, 1.0f);

    const auto tip  = centre.getPointOnCircumference (knobR * 0.82f, angle);
    const auto base = centre.getPointOnCircumference (knobR * 0.2f, angle);
    g.setColour (Toy::navy);
    g.drawLine ({ base, tip }, 3.0f);
}

void ToyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& colour,
                                           bool highlighted, bool down)
{
    auto face = colour;
    if (highlighted && ! down)
        face = face.brighter (0.06f);
    drawKey (g, button.getLocalBounds().toFloat().reduced (1.0f), face, down);
}

void ToyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.5f);

    const auto arrow = juce::Rectangle<float> ((float) width - 20.0f, (float) height * 0.5f - 3.0f, 10.0f, 6.0f);
    juce::Path p;
    p.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (p);
}

//==============================================================================
void SpellDisplay::set (const juce::String& now, const juce::String& next, const juce::String& sounds)
{
    if (now == nowText && next == nextText && sounds == soundText)
        return;
    nowText = now;
    nextText = next;
    soundText = sounds;
    repaint();
}

void SpellDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Bezel and glass.
    g.setColour (Toy::caseDeep);
    g.fillRoundedRectangle (r, 10.0f);
    r = r.reduced (6.0f);
    g.setColour (Toy::screen);
    g.fillRoundedRectangle (r, 6.0f);

    // Faint unlit cells across the glass, like a segment display waiting to light.
    const auto font = monoFont (42.0f);
    const auto cellW = juce::GlyphArrangement::getStringWidth (font, "W") + 4.0f, cellH = 50.0f, cellGap = 3.0f;
    const auto count = juce::jmax (1, (int) ((r.getWidth() - 24.0f) / (cellW + cellGap)));
    const auto rowW = (float) count * (cellW + cellGap) - cellGap;
    const auto row = juce::Rectangle<float> (r.getCentreX() - rowW * 0.5f, r.getY() + 18.0f, rowW, cellH);

    g.setColour (Toy::glow.withAlpha (0.05f));
    for (int i = 0; i < count; ++i)
        g.fillRoundedRectangle (row.getX() + (float) i * (cellW + cellGap), row.getY(), cellW, cellH, 3.0f);

    // The word, one letter per cell, with a soft glow behind it.
    const auto word = nowText.toUpperCase().substring (0, count);
    g.setFont (font);
    for (int i = 0; i < word.length(); ++i)
    {
        const auto cell = juce::Rectangle<float> (row.getX() + (float) i * (cellW + cellGap), row.getY(), cellW, cellH);
        const auto letter = word.substring (i, i + 1);
        g.setColour (Toy::glow.withAlpha (0.14f));
        for (const auto d : { juce::Point<float> (-2.0f, 0.0f), { 2.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f } })
            g.drawText (letter, cell.translated (d.x, d.y), juce::Justification::centred);
        g.setColour (Toy::glow);
        g.drawText (letter, cell, juce::Justification::centred);
    }

    // Small print along the bottom: the sounds, and what comes next.
    auto footer = r.reduced (14.0f, 8.0f).removeFromBottom (16.0f);
    g.setFont (monoFont (12.0f, false));
    g.setColour (Toy::glow.withAlpha (0.7f));
    g.drawText (soundText.toUpperCase(), footer, juce::Justification::centredLeft);
    g.drawText (nextText.isEmpty() ? juce::String() : "NEXT  " + nextText.toUpperCase(), footer,
                juce::Justification::centredRight);

    // A highlight across the top half of the glass.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), 6.0f);
}

//==============================================================================
SyllableStrip::SyllableStrip (PluginProcessor& p) : owner (p)
{
    refresh();
}

void SyllableStrip::refresh()
{
    labels.clear();
    sounds.clear();

    const auto text = owner.getLyrics();
    for (const auto& syl : owner.getSyllables())
    {
        auto label = text.substring (syl.textStart, syl.textEnd).removeCharacters ("-").trim();
        labels.add (label.isEmpty() ? juce::String ("~") : label);
        sounds.add (parrot::describe (syl));
    }

    layout();
    repaint();
}

void SyllableStrip::setPosition (int sung, int next)
{
    if (sung == sungIndex && next == nextIndex)
        return;
    sungIndex = sung;
    nextIndex = next;

    // Keep the next syllable in view.
    if (juce::isPositiveAndBelow (next, (int) chips.size()))
    {
        const auto& chip = chips[(size_t) next];
        if (chip.getBottom() - scroll > (float) getHeight() || chip.getY() - scroll < 0.0f)
            scroll = juce::jmax (0.0f, chip.getY());
    }
    repaint();
}

void SyllableStrip::layout()
{
    chips.clear();
    const auto font = monoFont (15.0f);
    float x = 0.0f, y = 0.0f;
    const auto width = (float) getWidth();

    for (const auto& label : labels)
    {
        const auto w = juce::jmax (34.0f, juce::GlyphArrangement::getStringWidth (font, label.toUpperCase()) + 20.0f);
        if (x > 0.0f && x + w > width)
        {
            x = 0.0f;
            y += chipHeight + chipGap;
        }
        chips.emplace_back (x, y, w, chipHeight);
        x += w + chipGap;
    }
}

void SyllableStrip::paint (juce::Graphics& g)
{
    if (chips.empty())
    {
        g.setColour (Toy::navy.withAlpha (0.7f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Type some lyrics above. Each note sings the next syllable.", getLocalBounds(),
                    juce::Justification::centredLeft);
        return;
    }

    for (size_t i = 0; i < chips.size(); ++i)
    {
        const auto r = chips[i].translated (0.0f, -scroll);
        if (r.getBottom() < 0.0f || r.getY() > (float) getHeight())
            continue;

        const auto index = (int) i;
        const auto sung = index == sungIndex, next = index == nextIndex;

        auto face = sung ? Toy::caseLight : Toy::key;
        if (index == hovered && ! sung)
            face = Toy::plate.brighter (0.05f);
        drawKey (g, r, face, sung, 6.0f);

        const auto top = r.withTrimmedBottom (3.0f).translated (0.0f, sung ? 2.0f : 0.0f);
        if (next)
        {
            g.setColour (Toy::navy);
            g.drawRoundedRectangle (top.reduced (1.5f), 6.0f, 2.0f);
        }

        g.setColour (sung ? Toy::cream : Toy::navy);
        g.setFont (monoFont (15.0f));
        g.drawText (labels[index].toUpperCase(), top, juce::Justification::centred);
    }

    if (juce::isPositiveAndBelow (hovered, sounds.size()))
    {
        const auto r = chips[(size_t) hovered].translated (0.0f, -scroll);
        const auto text = sounds[hovered];
        const auto font = monoFont (12.0f, false);
        const auto w = juce::GlyphArrangement::getStringWidth (font, text) + 12.0f;
        auto tip = juce::Rectangle<float> (r.getX(), r.getBottom() + 2.0f, w, 18.0f);
        if (tip.getBottom() > (float) getHeight())
            tip.setY (r.getY() - 20.0f);
        if (tip.getRight() > (float) getWidth())
            tip.setX ((float) getWidth() - w);
        g.setColour (Toy::navy);
        g.fillRoundedRectangle (tip, 4.0f);
        g.setColour (Toy::cream);
        g.setFont (font);
        g.drawText (text, tip, juce::Justification::centred);
    }
}

void SyllableStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position.translated (0.0f, scroll);
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].contains (p))
            owner.jumpToSyllable ((int) i);
}

void SyllableStrip::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position.translated (0.0f, scroll);
    int found = -1;
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].contains (p))
            found = (int) i;
    setHovered (found);
}

void SyllableStrip::setHovered (int index)
{
    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      owner (p),
      presetBar (p.presets),
      spellingAttachment (p.apvts, ParamIDs::spelling, spellingBox),
      timingAttachment (p.apvts, ParamIDs::timing, timingBox),
      strip (p),
      size    (p.apvts, ParamIDs::size, "Size"),
      tone    (p.apvts, ParamIDs::tone, "Tone"),
      breath  (p.apvts, ParamIDs::breath, "Breath"),
      diction (p.apvts, ParamIDs::diction, "Diction"),
      vibrato (p.apvts, ParamIDs::vibrato, "Vibrato"),
      vibRate (p.apvts, ParamIDs::vibRate, "Rate"),
      glide   (p.apvts, ParamIDs::glide, "Glide"),
      release (p.apvts, ParamIDs::release, "Release"),
      lofi    (p.apvts, ParamIDs::lofi, "Lo-fi"),
      volume  (p.apvts, ParamIDs::volume, "Volume"),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    // The attachments filled the boxes before they had items; give them items and sync again.
    spellingBox.addItemList (p.apvts.getParameter (ParamIDs::spelling)->getAllValueStrings(), 1);
    timingBox.addItemList (p.apvts.getParameter (ParamIDs::timing)->getAllValueStrings(), 1);
    spellingBox.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (ParamIDs::spelling)->load(), juce::dontSendNotification);
    timingBox.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (ParamIDs::timing)->load(), juce::dontSendNotification);
    spellingBox.setTooltip ("English reads normal spelling; Romaji sings Japanese written in Latin letters (ka-ra-o-ke)");
    timingBox.setTooltip ("On the beat starts consonants early so each vowel lands on its note (adds 150 ms latency, "
                          "which the host makes up). Live starts the syllable when you press the key.");

    for (auto* label : { &spellingLabel, &timingLabel })
    {
        label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        label->setColour (juce::Label::textColourId, Toy::cream);
        label->setJustificationType (juce::Justification::centredRight);
    }

    lyrics.setMultiLine (true, true);
    lyrics.setReturnKeyStartsNewLine (true);
    lyrics.setScrollbarsShown (true);
    lyrics.setIndents (10, 8);
    lyrics.setFont (juce::FontOptions (17.0f));
    lyrics.setColour (juce::TextEditor::backgroundColourId, Toy::key);
    lyrics.setColour (juce::TextEditor::textColourId, Toy::navy);
    lyrics.setColour (juce::TextEditor::highlightColourId, Toy::caseLight.withAlpha (0.3f));
    lyrics.setColour (juce::TextEditor::highlightedTextColourId, Toy::navy);
    lyrics.setColour (juce::CaretComponent::caretColourId, Toy::caseLight);
    lyrics.setColour (juce::TextEditor::outlineColourId, Toy::plate.darker (0.25f));
    lyrics.setColour (juce::TextEditor::focusedOutlineColourId, Toy::navy);
    lyrics.setTextToShowWhenEmpty ("Type lyrics here", Toy::navy.withAlpha (0.45f));
    lyrics.setText (owner.getLyrics(), false);
    lyrics.onTextChange = [this]
    {
        owner.setLyrics (lyrics.getText());
        strip.refresh();
    };

    pasteButton.setTooltip ("Paste at the cursor (some hosts keep Cmd+V for themselves)");
    pasteButton.onClick = [this]
    {
        lyrics.grabKeyboardFocus();
        lyrics.paste();
    };
    restartButton.setTooltip ("Back to the first syllable (also happens when the transport starts)");
    restartButton.onClick = [this] { owner.jumpToSyllable (0); };
    backButton.setTooltip ("Back one syllable (MIDI CC 50)");
    backButton.onClick = [this] { owner.stepSyllable (-1); };
    forwardButton.setTooltip ("Skip a syllable (MIDI CC 51)");
    forwardButton.onClick = [this] { owner.stepSyllable (1); };

    hint.setText ("par-rot  hyphen splits syllables    +  holds the vowel over another note",
                  juce::dontSendNotification);
    hint.setFont (juce::FontOptions (12.0f));
    hint.setColour (juce::Label::textColourId, Toy::navy.withAlpha (0.75f));

    for (auto* knob : { &size, &tone, &breath, &diction, &vibrato, &vibRate, &glide, &release, &lofi, &volume })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Toy::cream);
        knob->getSlider().setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob->getSlider().setColour (juce::Slider::textBoxTextColourId, Toy::cream);
    }

    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (48);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, Toy::key);
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, Toy::navy);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Toy::plate.darker (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, Toy::navy.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, Toy::caseDeep.withAlpha (0.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Toy::caseLight.withAlpha (0.85f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Toy::caseLight.withAlpha (0.25f));
    keyboard.setWantsKeyboardFocus (false);

    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBar, &spellingLabel, &spellingBox, &timingLabel, &timingBox, &display, &lyrics, &pasteButton,
             &restartButton, &backButton, &forwardButton, &hint, &strip,
             &size, &tone, &breath, &diction, &vibrato, &vibRate, &glide, &release, &lofi, &volume, &keyboard })
        addAndMakeVisible (c);

    owner.lyricsChanged.addChangeListener (this);
    setSize (760, 660);
    updateDisplay();
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    owner.lyricsChanged.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void PluginEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (const auto text = owner.getLyrics(); text != lyrics.getText())
        lyrics.setText (text, false);
    strip.refresh();
    shownSung = shownNext = -2;
    updateDisplay();
}

void PluginEditor::timerCallback()
{
    strip.setPosition (owner.getSungSyllable(), owner.getNextSyllable());
    updateDisplay();
}

void PluginEditor::updateDisplay()
{
    const auto sung = owner.getSungSyllable(), next = owner.getNextSyllable();
    if (sung == shownSung && next == shownNext)
        return;
    shownSung = sung;
    shownNext = next;

    const auto& labels = strip.getLabels();
    const auto& sounds = strip.getSounds();

    // Before anything is sung, show what the first note will say.
    const auto shown = juce::isPositiveAndBelow (sung, labels.size()) ? sung : next;
    display.set (labels[shown], shown == next ? juce::String() : labels[next], sounds[shown]);
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // The plastic case: a soft top-to-bottom shade and a lighter rim along the top.
    g.setGradientFill (juce::ColourGradient (Toy::caseLight, 0.0f, 0.0f, Toy::caseDark, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (bounds.withHeight (2.0f));

    // Name, moulded into the case.
    auto title = titleArea.toFloat();
    const juce::Font name (juce::FontOptions (34.0f, juce::Font::bold | juce::Font::italic));
    g.setFont (name);
    g.setColour (Toy::caseDeep);
    g.drawText ("Parrot", title.translated (0.0f, 2.0f), juce::Justification::centredLeft);
    g.setColour (Toy::cream);
    g.drawText ("Parrot", title, juce::Justification::centredLeft);

    const auto nameWidth = juce::GlyphArrangement::getStringWidth (name, "Parrot");
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (Toy::cream.withAlpha (0.8f));
    g.drawText ("SINGING SYNTH", title.withTrimmedLeft (nameWidth + 16.0f).withTrimmedTop (6.0f),
                juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (Toy::cream.withAlpha (0.65f));
    g.drawText ("ANIMAL TESTING  v" + juce::String (JucePlugin_VersionString), title, juce::Justification::centredRight);

    // Speaker grille: a recessed panel of holes.
    const auto grille = grilleArea.toFloat();
    g.setColour (Toy::caseDeep.withAlpha (0.55f));
    g.fillRoundedRectangle (grille, 12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (grille.translated (0.0f, 1.0f), 12.0f, 1.0f);

    const auto pitch = 13.0f, hole = 6.5f;
    const auto cols = (int) ((grille.getWidth() - 16.0f) / pitch), rows = (int) ((grille.getHeight() - 16.0f) / pitch);
    const auto origin = grille.getCentre() - juce::Point<float> ((float) (cols - 1) * pitch, (float) (rows - 1) * pitch) * 0.5f;
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const auto c = origin + juce::Point<float> ((float) col * pitch, (float) row * pitch);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (hole, hole).withCentre (c.translated (0.0f, 1.0f)));
            g.setColour (Toy::screen.withAlpha (0.85f));
            g.fillEllipse (juce::Rectangle<float> (hole, hole).withCentre (c));
        }
    }

    // The cream keypad plate the lyrics and syllable keys sit on, with a navy stripe along its top.
    const auto plate = plateArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillRoundedRectangle (plate.translated (0.0f, 2.0f), 14.0f);
    g.setColour (Toy::plate);
    g.fillRoundedRectangle (plate, 14.0f);
    g.setColour (Toy::navy);
    g.fillRoundedRectangle (plate.reduced (14.0f, 0.0f).withHeight (4.0f).translated (0.0f, 8.0f), 2.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    titleArea = area.removeFromTop (42);
    area.removeFromTop (6);

    auto top = area.removeFromTop (124);
    grilleArea = top.removeFromLeft (150);
    top.removeFromLeft (14);
    display.setBounds (top);
    area.removeFromTop (12);

    auto controls = area.removeFromTop (28);
    presetBar.setBounds (controls.removeFromLeft (330));
    timingBox.setBounds (controls.removeFromRight (130));
    timingLabel.setBounds (controls.removeFromRight (62));
    controls.removeFromRight (10);
    spellingBox.setBounds (controls.removeFromRight (100));
    spellingLabel.setBounds (controls.removeFromRight (74));
    area.removeFromTop (12);

    keyboard.setBounds (area.removeFromBottom (64));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f);   // the 43 white keys from C1 to C7 fill the width
    area.removeFromBottom (8);

    auto knobs = area.removeFromBottom (96);
    const auto knobWidth = knobs.getWidth() / 10;
    for (auto* k : { &size, &tone, &breath, &diction, &vibrato, &vibRate, &glide, &release, &lofi, &volume })
        k->setBounds (knobs.removeFromLeft (knobWidth));
    area.removeFromBottom (10);

    plateArea = area;
    auto inside = area.reduced (14, 12).withTrimmedTop (8);

    // The strip is one row of keys; the lyrics box takes whatever height is left.
    strip.setBounds (inside.removeFromBottom (40));
    inside.removeFromBottom (10);

    auto tools = inside.removeFromBottom (28);
    pasteButton.setBounds (tools.removeFromLeft (70));
    tools.removeFromLeft (12);
    restartButton.setBounds (tools.removeFromLeft (38));
    tools.removeFromLeft (4);
    backButton.setBounds (tools.removeFromLeft (38));
    tools.removeFromLeft (4);
    forwardButton.setBounds (tools.removeFromLeft (38));
    tools.removeFromLeft (12);
    hint.setBounds (tools);
    inside.removeFromBottom (8);

    lyrics.setBounds (inside);
}
