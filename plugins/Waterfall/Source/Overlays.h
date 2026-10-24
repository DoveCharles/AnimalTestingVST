#pragma once

#include "Chords.h"
#include "WaterfallLookAndFeel.h"

/** The notes of the current chord in thin type, each drifting gently on its own. When the chord
    changes, the old notes fade out as the new ones fade in. */
class ChordReadout final : public juce::Component
{
public:
    ChordReadout() { setInterceptsMouseClicks (false, false); }

    void setChord (int key, int chord)
    {
        if (key == currentKey && chord == currentChord)
            return;

        previous = current;
        current = noteNames (key, chord);
        currentKey = key;
        currentChord = chord;
        changeTime = time;
    }

    void setTime (float t)
    {
        time = t;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto fade = juce::jlimit (0.0f, 1.0f, (time - changeTime) / 0.8f);
        drawNotes (g, previous, 1.0f - fade);
        drawNotes (g, current, fade);
    }

private:
    static juce::StringArray noteNames (int key, int chord)
    {
        juce::StringArray names;
        for (auto interval : Chords::all[(size_t) chord].intervals)
            names.addIfNotAlreadyThere (Chords::keyNames[(key + interval) % 12]);
        return names;
    }

    void drawNotes (juce::Graphics& g, const juce::StringArray& notes, float alpha) const
    {
        if (alpha <= 0.0f || notes.isEmpty())
            return;

        const auto font = WaterfallLookAndFeel::thinFont (24.0f);
        const auto dot = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));
        const auto gap = juce::GlyphArrangement::getStringWidth (font, "  " + dot + "  ");

        auto total = gap * (float) (notes.size() - 1);
        for (const auto& n : notes)
            total += juce::GlyphArrangement::getStringWidth (font, n);

        auto x = ((float) getWidth() - total) * 0.5f;
        const auto midY = (float) getHeight() * 0.5f;
        g.setFont (font);

        auto drawAt = [&] (const juce::String& text, float left, float dy, float a)
        {
            const auto w = juce::GlyphArrangement::getStringWidth (font, text) + 2.0f;
            const juce::Rectangle<float> r (left, midY - 16.0f + dy, w, 32.0f);
            WaterfallLookAndFeel::drawHaloText (g, text, r, juce::Justification::centredLeft,
                                                juce::Colours::white.withAlpha (a));
        };

        for (int i = 0; i < notes.size(); ++i)
        {
            const auto phase = (float) i * 1.7f;
            const auto dy = 2.5f * std::sin (time * 0.5f + phase);
            const auto a = alpha * (0.85f + 0.15f * std::sin (time * 0.33f + phase * 1.3f));
            drawAt (notes[i], x, dy, a);
            x += juce::GlyphArrangement::getStringWidth (font, notes[i]);

            if (i < notes.size() - 1)
            {
                drawAt ("  " + dot, x, 0.0f, alpha * 0.5f);
                x += gap;
            }
        }
    }

    juce::StringArray current, previous;
    int currentKey = -1, currentChord = -1;
    float time = 0.0f, changeTime = -10.0f;
};

/** Soft dark pools under the floating knobs, so the controls read over the visual without a panel. */
class KnobShadows final : public juce::Component
{
public:
    KnobShadows() { setInterceptsMouseClicks (false, false); }

    void setKnobBounds (std::vector<juce::Rectangle<int>> b)
    {
        knobs = std::move (b);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (const auto& knob : knobs)
        {
            const auto r = knob.toFloat();
            const auto centre = r.getCentre().translated (0.0f, 4.0f);
            const auto radius = r.getWidth() * 0.55f;

            juce::ColourGradient pool (WaterfallLookAndFeel::shadow.withAlpha (0.45f), centre,
                                       WaterfallLookAndFeel::shadow.withAlpha (0.0f), centre.translated (radius, 0.0f), true);
            pool.addColour (0.5, WaterfallLookAndFeel::shadow.withAlpha (0.24f));
            g.setGradientFill (pool);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, r.getHeight() * 1.05f).withCentre (centre));
        }
    }

private:
    std::vector<juce::Rectangle<int>> knobs;
};
