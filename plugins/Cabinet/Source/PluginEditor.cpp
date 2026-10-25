#include "PluginEditor.h"

namespace
{
    using Palette = CabLookAndFeel::Palette;

    const auto impulseWildcard = juce::String ("*.wav;*.aif;*.aiff;*.flac");

    bool isImpulseFile (const juce::String& path)
    {
        return juce::File (path).hasFileExtension ("wav;aif;aiff;flac");
    }

    /** Mic distance in centimetres, matching the model's direct path length. */
    int distanceCm (float distance)
    {
        return juce::roundToInt (100.0f * (0.03f + 1.2f * distance));
    }
}

//==============================================================================
MicPad::MicPad (juce::AudioProcessorValueTreeState& state)
    : positionParameter (*state.getParameter (ParamIDs::position)),
      distanceParameter (*state.getParameter (ParamIDs::distance)),
      positionAttachment (positionParameter, [this] (float v) { position = v / 100.0f; repaint(); }),
      distanceAttachment (distanceParameter, [this] (float v) { distance = v / 100.0f; repaint(); })
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    positionAttachment.sendInitialUpdate();
    distanceAttachment.sendInitialUpdate();
}

juce::Rectangle<float> MicPad::speakerArea() const
{
    auto area = getLocalBounds().toFloat().reduced (14.0f);
    area.removeFromRight (34.0f);   // distance scale
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    return area.withSizeKeepingCentre (size, size);
}

juce::Point<float> MicPad::micPoint() const
{
    // Sideways is cap to edge; the height on the pad stands for distance, near at the bottom.
    const auto speaker = speakerArea();
    const auto radius = speaker.getWidth() * 0.5f;
    return { speaker.getCentreX() + position * radius * 0.92f,
             speaker.getBottom() - 10.0f - distance * (speaker.getHeight() - 20.0f) };
}

void MicPad::moveTo (juce::Point<float> p)
{
    const auto speaker = speakerArea();
    const auto radius = speaker.getWidth() * 0.5f;
    const auto newPosition = juce::jlimit (0.0f, 1.0f, (p.x - speaker.getCentreX()) / (radius * 0.92f));
    const auto newDistance = juce::jlimit (0.0f, 1.0f, (speaker.getBottom() - 10.0f - p.y) / (speaker.getHeight() - 20.0f));
    positionAttachment.setValueAsPartOfGesture (newPosition * 100.0f);
    distanceAttachment.setValueAsPartOfGesture (newDistance * 100.0f);
}

void MicPad::mouseDown (const juce::MouseEvent& e)
{
    positionAttachment.beginGesture();
    distanceAttachment.beginGesture();
    moveTo (e.position);
}

void MicPad::mouseDrag (const juce::MouseEvent& e)
{
    moveTo (e.position);
}

void MicPad::mouseUp (const juce::MouseEvent&)
{
    positionAttachment.endGesture();
    distanceAttachment.endGesture();
}

void MicPad::mouseDoubleClick (const juce::MouseEvent&)
{
    positionAttachment.setValueAsCompleteGesture (positionParameter.convertFrom0to1 (positionParameter.getDefaultValue()));
    distanceAttachment.setValueAsCompleteGesture (distanceParameter.convertFrom0to1 (distanceParameter.getDefaultValue()));
}

void MicPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    CabLookAndFeel::drawGrille (g, bounds, 6.0f);

    const auto speaker = speakerArea();
    const auto centre = speaker.getCentre();
    const auto radius = speaker.getWidth() * 0.5f;
    const auto dim = ! isEnabled();

    // Basket rim, paper cone with a few ridges, then the dust cap.
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (speaker);
    const auto cone = speaker.reduced (radius * 0.07f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2a28), centre,
                                             juce::Colour (0xff4a4640), cone.getTopLeft(), true));
    g.fillEllipse (cone);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (auto r : { 0.82f, 0.66f, 0.5f })
        g.drawEllipse (cone.withSizeKeepingCentre (cone.getWidth() * r, cone.getHeight() * r), 1.0f);
    const auto cap = speaker.withSizeKeepingCentre (radius * 0.6f, radius * 0.6f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a655c), cap.getTopLeft(),
                                             juce::Colour (0xff23211e), cap.getBottomRight(), false));
    g.fillEllipse (cap);

    // Distance scale on the right.
    auto scale = bounds.reduced (10.0f).removeFromRight (40.0f);
    g.setColour (Palette::silkscreen.withAlpha (0.7f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (auto d : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const auto y = speaker.getBottom() - 10.0f - d * (speaker.getHeight() - 20.0f);
        g.drawLine (scale.getX(), y, scale.getX() + 5.0f, y, 1.0f);
        g.drawText (juce::String (distanceCm (d)) + "cm", juce::Rectangle<float> (scale.getX() + 9.0f, y - 6.0f, 34.0f, 12.0f),
                    juce::Justification::centredLeft);
    }

    // The mic: a round dynamic mic head seen end-on, smaller the further back it is.
    const auto mic = micPoint();
    const auto micR = juce::jmap (distance, 13.0f, 7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (juce::Rectangle<float> (micR * 2.4f, micR * 2.4f).withCentre (mic.translated (2.0f, 4.0f)));
    g.setColour (dim ? Palette::silkscreen.withAlpha (0.4f) : Palette::cream);
    g.fillEllipse (juce::Rectangle<float> (micR * 2.0f, micR * 2.0f).withCentre (mic));
    g.setColour (juce::Colour (0xff222222));
    g.drawEllipse (juce::Rectangle<float> (micR * 1.3f, micR * 1.3f).withCentre (mic), 1.5f);
    g.fillEllipse (juce::Rectangle<float> (micR * 0.5f, micR * 0.5f).withCentre (mic));

    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    const auto where = position < 0.15f ? "CAP" : position < 0.6f ? "CAP EDGE" : position < 0.85f ? "CONE" : "CONE EDGE";
    g.drawText (juce::String (where) + "  /  " + juce::String (distanceCm (distance)) + " CM",
                bounds.reduced (10.0f, 6.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
}

//==============================================================================
void ResponseView::setResponse (const std::array<float, PluginProcessor::numResponsePoints>& newResponse)
{
    response = newResponse;
    hasResponse = true;
    repaint();
}

void ResponseView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = bounds.reduced (30.0f, 14.0f).withTrimmedBottom (6.0f);
    constexpr float topDb = 12.0f, bottomDb = -36.0f;
    const auto xFor = [&] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); };
    const auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (bottomDb, topDb, db), topDb, bottomDb, plot.getY(), plot.getBottom()); };

    g.setFont (juce::FontOptions (9.5f));
    for (auto hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const auto x = xFor (hz);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (Palette::silkscreen.withAlpha (0.5f));
        g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 2.0f, 32.0f, 12.0f), juce::Justification::centred);
    }
    for (auto db = topDb; db >= bottomDb; db -= 12.0f)
    {
        const auto y = yFor (db);
        g.setColour (juce::Colours::white.withAlpha (juce::exactlyEqual (db, 0.0f) ? 0.16f : 0.07f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (Palette::silkscreen.withAlpha (0.5f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (bounds.getX() + 2.0f, y - 6.0f, 24.0f, 12.0f),
                    juce::Justification::centredRight);
    }

    if (! hasResponse)
        return;

    juce::Path curve;
    for (int i = 0; i < PluginProcessor::numResponsePoints; ++i)
    {
        const juce::Point<float> p { xFor (PluginProcessor::responseFrequency (i)), yFor (response[(size_t) i]) };
        i == 0 ? curve.startNewSubPath (p) : curve.lineTo (p);
    }

    juce::Path fill (curve);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::trace.withAlpha (0.25f), 0.0f, plot.getY(),
                                             Palette::trace.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    g.setColour (Palette::trace.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Palette::trace);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void JewelSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight() - 16.0f) - 4.0f;
    const auto jewel = juce::Rectangle<float> (size, size).withCentre ({ bounds.getCentreX(), bounds.getY() + 2.0f + size * 0.5f })
                                                          .translated (0.0f, down ? 1.0f : 0.0f);
    const auto on = ! getToggleState();

    // Chrome bezel, faceted red jewel, glow when on.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8d8), jewel.getTopLeft(),
                                             juce::Colour (0xff5a5a5a), jewel.getBottomRight(), false));
    g.fillEllipse (jewel);
    const auto glass = jewel.reduced (size * 0.14f);
    if (on)
    {
        g.setColour (Palette::jewelOn.withAlpha (0.3f));
        g.fillEllipse (jewel.expanded (size * 0.2f));
    }
    g.setGradientFill (juce::ColourGradient (on ? Palette::jewelOn.brighter (0.4f) : Palette::jewelOff.brighter (0.2f), glass.getCentre(),
                                             on ? Palette::jewelOn.darker (0.4f) : Palette::jewelOff.darker (0.5f), glass.getTopLeft(), true));
    g.fillEllipse (glass);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 6; ++i)
    {
        const auto a = (float) i * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine ({ glass.getCentre(), glass.getCentre().getPointOnCircumference (glass.getWidth() * 0.5f, a) }, 0.8f);
    }
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.2f));
    g.fillEllipse (glass.withSizeKeepingCentre (glass.getWidth() * 0.3f, glass.getHeight() * 0.3f).translated (-size * 0.12f, -size * 0.12f));

    g.setColour (Palette::silkscreen.withAlpha (highlighted ? 1.0f : 0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (on ? "ON" : "OFF", bounds.removeFromBottom (14.0f), juce::Justification::centred);
}

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p),
      cabProcessor (p),
      micPad (p.apvts),
      position (p.apvts, ParamIDs::position, "Position"),
      distance (p.apvts, ParamIDs::distance, "Distance"),
      room     (p.apvts, ParamIDs::room,     "Room"),
      push     (p.apvts, ParamIDs::push,     "Push"),
      lowCut   (p.apvts, ParamIDs::lowCut,   "Low Cut"),
      highCut  (p.apvts, ParamIDs::highCut,  "High Cut"),
      output   (p.apvts, ParamIDs::output,   "Output"),
      jewelAttachment (p.apvts, ParamIDs::bypass, jewel)
{
    setLookAndFeel (&lookAndFeel);

    // Items first: the attachments select by index.
    modelBox.addItemList (cab::modelNames, 1);
    micBox.addItemList (cab::micNames, 1);
    modelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::model, modelBox);
    micAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::mic, micBox);
    for (auto* box : { &modelBox, &micBox })
        addAndMakeVisible (*box);
    modelBox.setTooltip ("Cabinet");
    micBox.setTooltip ("Microphone");

    loadButton.setTooltip ("Load an impulse response file (or drop one anywhere on the window)");
    loadButton.onClick = [this] { chooseImpulseFile(); };
    addAndMakeVisible (loadButton);

    impulseLabel.setFont (juce::FontOptions (12.0f));
    impulseLabel.setColour (juce::Label::textColourId, Palette::silkscreen.withAlpha (0.75f));
    impulseLabel.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (impulseLabel);

    addAndMakeVisible (micPad);
    addAndMakeVisible (responseView);

    for (auto* knob : { &position, &distance, &room, &push, &lowCut, &highCut, &output })
    {
        knob->getLabel().setColour (juce::Label::textColourId, Palette::silkscreen);
        knob->getSlider().setColour (juce::Slider::textBoxTextColourId, Palette::cream.withAlpha (0.8f));
        knob->getSlider().setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        knob->getSlider().setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*knob);
    }

    jewel.setTooltip ("Bypass");
    addAndMakeVisible (jewel);

    updateImpulseLabel();
    setSize (820, 520);
    startTimerHz (20);
    timerCallback();
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    if (const auto version = cabProcessor.getResponseVersion(); version != lastResponseVersion)
    {
        lastResponseVersion = version;
        responseView.setResponse (cabProcessor.getResponse());
    }

    // A loaded IR already has its own mic and placement, so those controls step aside.
    if (const auto model = modelBox.getSelectedItemIndex(); model != lastModel)
    {
        lastModel = model;
        const auto builtIn = model != cab::userIr;
        micBox.setEnabled (builtIn);
        position.setEnabled (builtIn);
        micPad.setEnabled (builtIn);
        micPad.repaint();
        updateImpulseLabel();
    }
}

void PluginEditor::updateImpulseLabel()
{
    const auto name = cabProcessor.getUserImpulseName();
    if (name.isEmpty())
        impulseLabel.setText (modelBox.getSelectedItemIndex() == cab::userIr ? "No IR loaded: playing the 4x12 Vintage"
                                                                            : "Drop an IR file here",
                              juce::dontSendNotification);
    else
        impulseLabel.setText (name, juce::dontSendNotification);
}

void PluginEditor::chooseImpulseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load impulse response", juce::File(), impulseWildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              if (const auto file = fc.getResult(); file.existsAsFile())
                                  loadImpulse (file);
                          });
}

void PluginEditor::loadImpulse (const juce::File& file)
{
    if (! cabProcessor.loadImpulseFile (file))
    {
        impulseLabel.setText ("Couldn't read " + file.getFileName(), juce::dontSendNotification);
        return;
    }
    updateImpulseLabel();
}

bool PluginEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && isImpulseFile (files[0]);
}

void PluginEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void PluginEditor::fileDragExit (const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void PluginEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    loadImpulse (juce::File (files[0]));
}

void PluginEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    CabLookAndFeel::drawTolex (g, bounds);

    // Brass-piped control panel inset into the tolex.
    const auto panel = panelArea.toFloat();
    g.setGradientFill (juce::ColourGradient (Palette::panel.brighter (0.08f), panel.getTopLeft(),
                                             Palette::panel.darker (0.2f), panel.getBottomLeft(), false));
    g.fillRoundedRectangle (panel, 6.0f);
    g.setColour (Palette::piping);
    g.drawRoundedRectangle (panel.expanded (3.0f), 8.0f, 2.0f);

    // Logo in the top-left of the tolex.
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic).withHorizontalScale (0.92f));
    g.drawText ("Cabinet", juce::Rectangle<float> (22.0f, 12.0f, 140.0f, 34.0f), juce::Justification::centredLeft);
    g.setColour (Palette::piping);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("ANIMAL TESTING  v" JucePlugin_VersionString, juce::Rectangle<float> (24.0f, 42.0f, 200.0f, 12.0f),
                juce::Justification::centredLeft);

    if (dragHover)
    {
        g.setColour (Palette::piping.withAlpha (0.15f));
        g.fillRect (bounds);
        g.setColour (Palette::piping);
        g.drawRect (bounds, 3.0f);
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (18, 0);

    // Top strip, on the tolex: logo, cab, mic, load button and the IR name.
    auto top = area.removeFromTop (66).withTrimmedTop (16).withTrimmedBottom (14);
    top.removeFromLeft (170);
    modelBox.setBounds (top.removeFromLeft (190));
    top.removeFromLeft (8);
    micBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (8);
    loadButton.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (10);
    impulseLabel.setBounds (top);

    panelArea = area.withTrimmedBottom (18);
    auto inner = panelArea.reduced (14);

    auto knobRow = inner.removeFromBottom (128);
    jewel.setBounds (knobRow.removeFromRight (56).withSizeKeepingCentre (48, 64));
    knobRow.removeFromRight (6);
    const auto knobWidth = knobRow.getWidth() / 7;
    for (auto* knob : { &position, &distance, &room, &push, &lowCut, &highCut, &output })
        knob->setBounds (knobRow.removeFromLeft (knobWidth).reduced (2, 0));

    inner.removeFromBottom (12);
    micPad.setBounds (inner.removeFromLeft (300));
    inner.removeFromLeft (12);
    responseView.setBounds (inner);
}
