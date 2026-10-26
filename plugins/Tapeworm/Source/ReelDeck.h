#pragma once

#include "DeckLookAndFeel.h"
#include "PluginProcessor.h"

/** The tape window: a supply and a take-up reel, the tape running between them over two guides,
    the head and the capstan, and a counter. The reels turn while the host plays, at the tape's
    real speed (slowed by Speed, stopped by Tape Stop, racing during Fast-Forward), and each turns
    at the rate its pack needs: slow when full, fast when nearly empty. The packs follow the
    song position around one 30-minute side. */
class ReelDeck final : public juce::Component, private juce::Timer
{
public:
    explicit ReelDeck (PluginProcessor& p) : processor (p)
    {
        setOpaque (false);
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override
    {
        using Palette = DeckLookAndFeel::Palette;
        const auto bounds = getLocalBounds().toFloat();
        DeckLookAndFeel::drawRecess (g, bounds, juce::Colour (0xff1b1c20));

        const auto reelR = juce::jmin (bounds.getHeight() * 0.37f, bounds.getWidth() * 0.22f);
        const auto cy = bounds.getY() + 12.0f + reelR;
        const auto midX = bounds.getCentreX();
        const auto spread = juce::jmin (bounds.getWidth() * 0.27f, reelR * 1.45f);
        const juce::Point<float> supply { midX - spread, cy }, takeUp { midX + spread, cy };

        const auto taken = takenUp();
        const auto supplyPack = packRadius (1.0 - taken) * reelR;
        const auto takeUpPack = packRadius (taken) * reelR;

        // Tape path: off the supply pack, round a guide, across the head and capstan, round the
        // other guide and onto the take-up pack. The reels' front flanges hide the ends.
        const auto tapeY = bounds.getBottom() - 30.0f;
        constexpr auto guideR = 7.0f;
        const juce::Point<float> guideL { supply.x + reelR * 0.62f, tapeY + guideR },
                                 guideRt { takeUp.x - reelR * 0.62f, tapeY + guideR };
        const auto leaveSupply = innerTangent (supply, supplyPack, guideL.withY (tapeY), true);
        const auto reachTakeUp = innerTangent (takeUp, takeUpPack, guideRt.withY (tapeY), false);

        juce::Path tape;
        tape.startNewSubPath (leaveSupply);
        tape.lineTo (guideL.x, tapeY);
        tape.lineTo (guideRt.x, tapeY);
        tape.lineTo (reachTakeUp);
        g.setColour (tapeBrown.brighter (0.25f));
        g.strokePath (tape, juce::PathStrokeType (2.5f));

        for (auto guide : { guideL, guideRt })
            drawRoller (g, guide, guideR, Palette::metalLight, Palette::metalDark);

        // Head: a chrome block with its gap, the tape riding over the top.
        const auto head = juce::Rectangle<float> (46.0f, 22.0f).withCentre ({ midX - 14.0f, tapeY + 11.0f });
        auto chrome = juce::ColourGradient::horizontal (Palette::metalDark, head.getX(), Palette::metalDark, head.getRight());
        chrome.addColour (0.4, Palette::metalLight.brighter (0.3f));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (head, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (head, 4.0f, 1.0f);
        g.fillRect (juce::Rectangle<float> (1.5f, 6.0f).withCentre ({ head.getCentreX(), head.getY() + 3.5f }));

        // Capstan below the tape and the rubber pinch roller pressing down on it.
        const juce::Point<float> capstan { midX + 34.0f, tapeY + 4.0f }, pinch { midX + 34.0f, tapeY - 10.0f };
        drawRoller (g, capstan, 3.5f, Palette::metalLight, Palette::metalDark);
        drawRoller (g, pinch, 9.0f, juce::Colour (0xff3a3c42), juce::Colour (0xff111214));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (pinch.getPointOnCircumference (5.5f, pinchAngle)));
        g.setColour (tapeBrown.brighter (0.25f));
        g.drawLine (pinch.x - 9.0f, tapeY, pinch.x + 9.0f, tapeY, 2.5f);

        drawReel (g, supply, reelR, supplyPack, supplyAngle);
        drawReel (g, takeUp, reelR, takeUpPack, takeUpAngle);

        // Counter and run lamp between the reels.
        const auto counter = juce::Rectangle<float> (64.0f, 22.0f).withCentre ({ midX, bounds.getY() + 26.0f });
        g.setColour (Palette::window);
        g.fillRoundedRectangle (counter, 3.0f);
        DeckLookAndFeel::drawBevel (g, counter, 3.0f, false);
        const auto seconds = (int) std::fmod (juce::jmax (0.0, processor.hostSeconds.load()), sideSeconds);
        g.setColour (Palette::amber);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold));
        g.drawText (juce::String::formatted ("%02d:%02d", seconds / 60, seconds % 60), counter,
                    juce::Justification::centred);

        const auto running = turningSpeed > 0.01f;
        const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ midX, counter.getBottom() + 14.0f });
        if (running)
        {
            g.setGradientFill (juce::ColourGradient (Palette::amber.withAlpha (0.5f), lamp.getCentre(),
                                                     Palette::amber.withAlpha (0.0f), lamp.getCentre().translated (9.0f, 0.0f), true));
            g.fillEllipse (lamp.expanded (6.0f));
        }
        g.setColour (running ? Palette::amber : Palette::lampOff);
        g.fillEllipse (lamp);
        g.setColour (Palette::metalDark);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("RUN", lamp.withSizeKeepingCentre (40.0f, 12.0f).translated (0.0f, 12.0f), juce::Justification::centred);
    }

private:
    static constexpr double sideSeconds = 30.0 * 60.0;  // one side of a C60
    static constexpr float hubRadius = 0.27f, fullRadius = 0.9f;  // relative to the flange
    static inline const juce::Colour tapeBrown { 0xff4a3326 };

    /** How much of the side is on the take-up reel, 0 to 1. */
    double takenUp() const
    {
        return std::fmod (juce::jmax (0.0, processor.hostSeconds.load()), sideSeconds) / sideSeconds;
    }

    /** A pack holding the given share of the tape: its area grows with the length wound on. */
    static float packRadius (double share)
    {
        return std::sqrt (hubRadius * hubRadius + (float) share * (fullRadius * fullRadius - hubRadius * hubRadius));
    }

    /** Where a line from `from` touches the pack on the side facing the middle of the deck. */
    static juce::Point<float> innerTangent (juce::Point<float> centre, float radius, juce::Point<float> from, bool rightSide)
    {
        const auto distance = juce::jmax (radius + 0.01f, centre.getDistanceFrom (from));
        const auto towards = std::atan2 (from.y - centre.y, from.x - centre.x);
        const auto offset = std::acos (radius / distance);
        const juce::Point<float> a { centre.x + radius * std::cos (towards + offset), centre.y + radius * std::sin (towards + offset) };
        const juce::Point<float> b { centre.x + radius * std::cos (towards - offset), centre.y + radius * std::sin (towards - offset) };
        return (a.x > b.x) == rightSide ? a : b;
    }

    static void drawRoller (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour light, juce::Colour dark)
    {
        const auto r = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (light, r.getTopLeft(), dark, r.getBottomRight(), false));
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (r, 1.0f);
    }

    static void drawReel (juce::Graphics& g, juce::Point<float> centre, float flangeR, float packR, float angle)
    {
        using Palette = DeckLookAndFeel::Palette;
        const auto circle = [centre] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (circle (flangeR).translated (2.0f, 4.0f));

        // Tape pack, with the faint rings of the wraps.
        g.setGradientFill (juce::ColourGradient (tapeBrown.brighter (0.3f), centre, tapeBrown.darker (0.3f),
                                                 centre.translated (packR, 0.0f), true));
        g.fillEllipse (circle (packR));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (auto r = flangeR * hubRadius + 3.0f; r < packR; r += 3.0f)
            g.drawEllipse (circle (r), 0.6f);
        g.setColour (tapeBrown.brighter (0.6f).withAlpha (0.6f));
        g.drawEllipse (circle (packR), 1.0f);

        // Front flange with three windows onto the pack; the windows turn, the light doesn't.
        juce::Path flange;
        flange.setUsingNonZeroWinding (false);
        flange.addEllipse (circle (flangeR));
        const auto halfWidth = juce::degreesToRadians (34.0f);
        for (int i = 0; i < 3; ++i)
        {
            const auto a = angle + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            const auto inner = flangeR * 0.36f, outer = flangeR * 0.86f;
            flange.addCentredArc (centre.x, centre.y, outer, outer, 0.0f, a - halfWidth, a + halfWidth, true);
            flange.addCentredArc (centre.x, centre.y, inner, inner, 0.0f, a + halfWidth * 0.7f, a - halfWidth * 0.7f, false);
            flange.closeSubPath();
        }
        const auto disc = circle (flangeR);
        g.setGradientFill (juce::ColourGradient (Palette::metalLight.brighter (0.15f), disc.getTopLeft(),
                                                 Palette::metalDark.darker (0.25f), disc.getBottomRight(), false));
        g.fillPath (flange);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (auto r : { 0.95f, 0.9f, 0.3f })
            g.drawEllipse (circle (flangeR * r), 0.8f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (flange, juce::PathStrokeType (1.0f));

        // Hub with three drive keys and the spindle.
        const auto hubR = flangeR * 0.2f;
        g.setColour (juce::Colour (0xff2a2c31));
        g.fillEllipse (circle (hubR));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (circle (hubR), 1.0f);
        for (int i = 0; i < 3; ++i)
        {
            const auto a = angle + juce::MathConstants<float>::pi / 3.0f + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
            juce::Path key;
            key.addRoundedRectangle (-hubR * 0.14f, -hubR * 0.95f, hubR * 0.28f, hubR * 0.5f, 1.0f);
            key.applyTransform (juce::AffineTransform::rotation (a).translated (centre));
            g.setColour (Palette::metalDark);
            g.fillPath (key);
        }
        drawRoller (g, centre, hubR * 0.35f, Palette::metalLight.brighter (0.3f), Palette::metalDark);
    }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto dt = (float) juce::jlimit (0.0, 0.1, (now - lastTime) * 0.001);
        lastTime = now;

        // The motors take a moment to get going and to stop.
        const auto target = processor.hostPlaying.load() ? processor.tapeSpeed.load() : 0.0f;
        turningSpeed += (target - turningSpeed) * (1.0f - std::exp (-dt / 0.12f));
        if (turningSpeed < 0.001f && target == 0.0f)
            turningSpeed = 0.0f;

        // Tape runs past at a fixed rate; each reel turns at that rate over its own pack radius.
        // At normal speed a nearly empty reel goes round about once a second.
        constexpr auto tapeRate = juce::MathConstants<float>::twoPi * hubRadius;
        const auto taken = takenUp();
        const auto travel = turningSpeed * tapeRate * dt;
        supplyAngle = std::fmod (supplyAngle + travel / packRadius (1.0 - taken), juce::MathConstants<float>::twoPi);
        takeUpAngle = std::fmod (takeUpAngle + travel / packRadius (taken), juce::MathConstants<float>::twoPi);
        pinchAngle = std::fmod (pinchAngle + travel * 6.0f, juce::MathConstants<float>::twoPi);

        const auto second = (int) processor.hostSeconds.load();
        if (turningSpeed > 0.0f || second != shownSecond || (turningSpeed > 0.01f) != wasRunning)
            repaint();
        shownSecond = second;
        wasRunning = turningSpeed > 0.01f;
    }

    PluginProcessor& processor;
    double lastTime = juce::Time::getMillisecondCounterHiRes();
    float turningSpeed = 0.0f, supplyAngle = 0.0f, takeUpAngle = 0.4f, pinchAngle = 0.0f;
    int shownSecond = -1;
    bool wasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReelDeck)
};
