#pragma once

#include "DeckLookAndFeel.h"
#include "PluginProcessor.h"

/** The tape window: a supply and a take-up reel (one red apple, one green), the tape (a pink
    worm) hanging between them, and a counter. The reels turn while the host plays, at the tape's
    real speed (slowed by Speed, stopped by Tape Stop, racing during Fast-Forward), and each turns
    at the rate its pack needs. The packs stay put, a third of the way through the side, so the
    supply turns slower than the take-up. */
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
        DeckLookAndFeel::drawBoard (g, bounds);

        const auto reelR = juce::jmin (bounds.getHeight() * 0.37f, bounds.getWidth() * 0.22f);
        const auto cy = bounds.getCentreY();
        const auto midX = bounds.getCentreX();
        const auto spread = juce::jmin (bounds.getWidth() * 0.27f, reelR * 1.45f);
        const juce::Point<float> supply { midX - spread, cy }, takeUp { midX + spread, cy };

        const auto supplyPack = supplyPackRadius * reelR;
        const auto takeUpPack = takeUpPackRadius * reelR;

        // Each reel is an apple seen from above, drawn in layers: the flesh, the worm hanging between
        // the reels (it leaves the bottom of the supply pack, droops, and comes up level onto the
        // bottom of the take-up pack, so both reels turn anticlockwise), the worm wound on each,
        // then the skin with bites out of it and the stem and leaf on top.
        constexpr auto half = wormWidth * 0.5f;
        const juce::Point<float> leaveSupply { supply.x, supply.y + supplyPack - half },
                                 reachTakeUp { takeUp.x, takeUp.y + takeUpPack - half };
        const auto reach = (reachTakeUp.x - leaveSupply.x) * 0.25f;
        const juce::Point<float> low { midX, juce::jmin (bounds.getBottom() - half - 8.0f,
                                                         juce::jmax (leaveSupply.y, reachTakeUp.y) + reelR * 0.3f) };
        g.setColour (ink.withAlpha (0.3f));
        for (auto reel : { supply, takeUp })
            g.fillEllipse (juce::Rectangle<float> (reelR * 2.0f, reelR * 2.0f).withCentre (reel.translated (3.0f, 5.0f)));
        g.setColour (appleFlesh);
        for (auto reel : { supply, takeUp })
            g.fillEllipse (juce::Rectangle<float> (reelR * 1.9f, reelR * 1.9f).withCentre (reel));

        juce::Path worm;
        worm.startNewSubPath (leaveSupply);
        worm.cubicTo (leaveSupply.translated (reach, 0.0f), low.translated (-reach, 0.0f), low);
        worm.cubicTo (low.translated (reach, 0.0f), reachTakeUp.translated (-reach, 0.0f), reachTakeUp);
        drawWorm (g, worm, tapeTravel * reelR);

        drawPack (g, supply, reelR, supplyPack, -supplyAngle);
        drawPack (g, takeUp, reelR, takeUpPack, -takeUpAngle);
        drawSkin (g, supply, reelR, -supplyAngle, redApple);
        drawSkin (g, takeUp, reelR, -takeUpAngle, greenApple);

        // Counter and run lamp between the reels: a paper tag and a cherry.
        const auto counter = juce::Rectangle<float> (66.0f, 24.0f).withCentre ({ midX, bounds.getY() + 28.0f });
        g.setColour (ink.withAlpha (0.2f));
        g.fillRoundedRectangle (counter.translated (2.0f, 3.0f), 6.0f);
        g.setColour (Palette::paper);
        g.fillRoundedRectangle (counter, 6.0f);
        g.setColour (ink);
        g.drawRoundedRectangle (counter, 6.0f, 2.0f);
        const auto seconds = (int) std::fmod (juce::jmax (0.0, processor.hostSeconds.load()), sideSeconds);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold));
        g.drawText (juce::String::formatted ("%02d:%02d", seconds / 60, seconds % 60), counter,
                    juce::Justification::centred);

        const auto running = turningSpeed > 0.01f;
        const auto lamp = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ midX, counter.getBottom() + 15.0f });
        if (running)
        {
            g.setGradientFill (juce::ColourGradient (Palette::cherry.withAlpha (0.5f), lamp.getCentre(),
                                                     Palette::cherry.withAlpha (0.0f), lamp.getCentre().translated (12.0f, 0.0f), true));
            g.fillEllipse (lamp.expanded (7.0f));
        }
        g.setColour (running ? Palette::cherry : Palette::cream.darker (0.2f));
        g.fillEllipse (lamp);
        if (running)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (lamp.getCentre().translated (-1.5f, -1.5f)));
        }
        g.setColour (ink);
        g.drawEllipse (lamp, 1.5f);
        g.setFont (DeckLookAndFeel::hand (12.0f));
        g.drawText ("RUN", lamp.withSizeKeepingCentre (40.0f, 14.0f).translated (0.0f, 14.0f), juce::Justification::centred);
    }

private:
    static constexpr double sideSeconds = 30.0 * 60.0;  // one side of a C60
    static constexpr float hubRadius = 0.27f, fullRadius = 0.9f;  // relative to the flange
    static constexpr float wormWidth = 13.0f, ribSpacing = 9.0f;

    // Cartoon colours: everything gets the same dark outline.
    static inline const juce::Colour ink { 0xff2b1a1e }, wormPink { 0xfff4a2b8 }, wormRib { 0xffd8778f },
                                     wormShine { 0xffffdce5 }, appleFlesh { 0xfffff3d4 }, stemBrown { 0xff7a4a26 },
                                     leafGreen { 0xff3f9a2c };

    // Sizes are relative to the flange, angles in degrees from straight up, turning with the reel.
    struct Leaf { float angle, length, width, bend, shade; };
    struct Stem { float angle, length, width, curl; };
    struct Apple
    {
        juce::Colour light, dark;
        int bites;  // picks the shape of its bites
        Stem stem;
        std::array<Leaf, 2> leaves;  // a length of 0 leaves one out
    };
    static inline const Apple redApple { juce::Colour (0xffff5a4a), juce::Colour (0xffa81c1c), 1,
                                         { -20.0f, 0.3f, 0.075f, 0.35f },
                                         { { { 55.0f, 0.62f, 0.25f, 0.22f, 1.0f }, {} } } };
    static inline const Apple greenApple { juce::Colour (0xffa8e05a), juce::Colour (0xff4c8f22), 7,
                                           { 160.0f, 0.22f, 0.09f, -0.6f },
                                           { { { -115.0f, 0.55f, 0.21f, -0.18f, 0.85f },
                                               { -55.0f, 0.36f, 0.15f, 0.25f, 1.1f } } } };

    /** A pack holding the given share of the tape: its area grows with the length wound on. */
    static float packRadius (float share)
    {
        return std::sqrt (hubRadius * hubRadius + share * (fullRadius * fullRadius - hubRadius * hubRadius));
    }

    static inline const float supplyPackRadius = packRadius (2.0f / 3.0f), takeUpPackRadius = packRadius (1.0f / 3.0f);

    /** The worm wound on a reel in rings, each ribbed across, turning with it. */
    static void drawPack (juce::Graphics& g, juce::Point<float> centre, float flangeR, float packR, float angle)
    {
        const auto circle = [centre] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };

        // The pack: wraps of worm, outermost outlined, each ribbed across.
        const auto hubR = flangeR * hubRadius;
        g.setColour (wormPink);
        g.fillEllipse (circle (packR));
        juce::Path grooves, ribs;
        for (auto r = packR - wormWidth; r > hubR; r -= wormWidth)
            grooves.addEllipse (circle (r));
        for (auto outer = packR; outer > hubR + 2.0f; outer -= wormWidth)
        {
            const auto inner = juce::jmax (hubR, outer - wormWidth), mid = (outer + inner) * 0.5f;
            const auto count = juce::jmax (3, (int) (juce::MathConstants<float>::twoPi * mid / ribSpacing));
            for (int i = 0; i < count; ++i)
            {
                const auto a = angle + (float) i * juce::MathConstants<float>::twoPi / (float) count;
                ribs.startNewSubPath (centre.getPointOnCircumference (inner + 1.5f, a));
                ribs.lineTo (centre.getPointOnCircumference (outer - 1.5f, a));
            }
        }
        g.setColour (wormRib);
        g.strokePath (ribs, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ink);
        g.strokePath (grooves, juce::PathStrokeType (1.5f));
        g.drawEllipse (circle (packR), 2.0f);
    }

    /** A reel's front flange, as an apple's skin seen from above: three bites out of it for
        windows onto the pack, and the stem and leaf in the middle. Everything but the shine turns. */
    static void drawSkin (juce::Graphics& g, juce::Point<float> centre, float flangeR, float angle, const Apple& apple)
    {
        const auto circle = [centre] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };

        juce::Path flange;
        flange.setUsingNonZeroWinding (false);
        flange.addEllipse (circle (flangeR));
        for (int i = 0; i < 3; ++i)
            flange.addPath (bite (flangeR, apple.bites * 3 + i),
                            juce::AffineTransform::rotation (angle + (float) i * juce::MathConstants<float>::twoPi / 3.0f)
                                .translated (centre));
        const auto disc = circle (flangeR);
        g.setGradientFill (juce::ColourGradient (apple.light, disc.getTopLeft(), apple.dark, disc.getBottomRight(), false));
        g.fillPath (flange);
        g.setColour (ink);
        g.strokePath (flange, juce::PathStrokeType (2.5f));

        // Cartoon shine, which stays put while the reel turns.
        juce::Path shine;
        shine.addCentredArc (centre.x, centre.y, flangeR * 0.93f, flangeR * 0.93f, 0.0f,
                             juce::degreesToRadians (-75.0f), juce::degreesToRadians (-30.0f), true);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.strokePath (shine, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // The stem in its dimple, and a leaf.
        const auto dimple = circle (flangeR * 0.2f);
        g.setGradientFill (juce::ColourGradient (apple.dark.darker (0.3f), centre, apple.dark.withAlpha (0.0f),
                                                 centre.translated (dimple.getWidth() * 0.5f, 0.0f), true));
        g.fillEllipse (dimple);

        const auto turn = juce::AffineTransform::rotation (angle).translated (centre);
        for (const auto& l : apple.leaves)
        {
            if (l.length <= 0.0f)
                continue;

            // A leaf from the stem out to a tip that bends off to one side, with a curved vein.
            const auto length = flangeR * l.length, width = flangeR * l.width, bend = length * l.bend;
            juce::Path leaf, vein;
            leaf.startNewSubPath (0.0f, 0.0f);
            leaf.cubicTo (width * 1.1f, -length * 0.2f, width + bend * 0.6f, -length * 0.75f, bend, -length);
            leaf.cubicTo (-width + bend * 0.4f, -length * 0.8f, -width * 1.1f, -length * 0.25f, 0.0f, 0.0f);
            leaf.closeSubPath();
            vein.startNewSubPath (0.0f, -length * 0.08f);
            vein.quadraticTo (bend * 0.15f, -length * 0.55f, bend * 0.85f, -length * 0.88f);
            const auto place = juce::AffineTransform::rotation (juce::degreesToRadians (l.angle)).followedBy (turn);
            leaf.applyTransform (place);
            vein.applyTransform (place);

            g.setColour (leafGreen.withMultipliedBrightness (l.shade));
            g.fillPath (leaf);
            g.setColour (ink);
            g.strokePath (leaf, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved));
            g.setColour (leafGreen.withMultipliedBrightness (l.shade).brighter (0.6f));
            g.strokePath (vein, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // The stem: a stalk that curls over, its cut end showing.
        const auto& st = apple.stem;
        const auto stemLength = flangeR * st.length, stemWidth = flangeR * st.width;
        const juce::Point<float> tip { stemLength * st.curl, -stemLength };
        juce::Path stalk;
        stalk.startNewSubPath (0.0f, 0.0f);
        stalk.quadraticTo (0.0f, -stemLength * 0.75f, tip.x, tip.y);
        stalk.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (st.angle)).followedBy (turn));
        const auto end = tip.transformedBy (juce::AffineTransform::rotation (juce::degreesToRadians (st.angle)).followedBy (turn));
        const auto rounded = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (ink);
        g.strokePath (stalk, rounded (stemWidth + 5.0f));
        g.setColour (stemBrown);
        g.strokePath (stalk, rounded (stemWidth));
        g.setColour (stemBrown.brighter (0.5f));
        g.fillEllipse (juce::Rectangle<float> (stemWidth * 0.6f, stemWidth * 0.6f).withCentre (end));
    }

    /** Pseudo-random 0 to 1 for a pair of numbers, the same every time. */
    static float hash (int a, int b)
    {
        const auto h = std::sin ((float) (a * 127 + b * 311) * 12.9898f) * 43758.5453f;
        return h - std::floor (h);
    }

    /** One bite out of a reel's skin, above its centre (rotate it into place): a lumpy blob whose
        edge is a run of uneven tooth marks cut back into the skin. `seed` picks its shape. */
    static juce::Path bite (float flangeR, int seed)
    {
        // The blob: an ellipse, wider along the rim than across, with a few random lumps.
        constexpr int points = 160;
        juce::Array<juce::Point<float>> outline;
        const juce::Point<float> middle { (hash (seed, 1) - 0.5f) * flangeR * 0.08f, -flangeR * (0.54f + 0.03f * hash (seed, 2)) };
        const auto across = flangeR * (0.42f + 0.05f * hash (seed, 3)), deep = flangeR * (0.27f + 0.03f * hash (seed, 4));
        for (int i = 0; i < points; ++i)
        {
            const auto theta = juce::MathConstants<float>::twoPi * (float) i / (float) points;
            auto lump = 1.0f;
            for (int h = 2; h <= 4; ++h)
                lump += (0.03f + 0.07f * hash (seed, h * 10)) * std::sin ((float) h * theta + 6.28f * hash (seed, h * 10 + 1));
            outline.add (middle + juce::Point<float> (std::cos (theta) * across, std::sin (theta) * deep) * lump);
        }

        // Tooth marks of uneven widths and depths, each a scallop pushed out into the skin.
        juce::Array<float> lengths, ends;
        auto perimeter = 0.0f;
        for (int i = 0; i < points; ++i)
            perimeter += outline[i].getDistanceFrom (outline[(i + 1) % points]);
        auto total = 0.0f;
        for (int t = 0; total < perimeter; ++t)
        {
            lengths.add (9.0f + 14.0f * hash (seed, 100 + t));
            total += lengths.getLast();
        }
        auto end = 0.0f;
        for (auto& length : lengths)
            ends.add (end += length * perimeter / total);

        juce::Path result;
        auto walked = 0.0f;
        int tooth = 0;
        for (int i = 0; i < points; ++i)
        {
            const auto a = outline[i], b = outline[(i + 1) % points];
            const auto length = a.getDistanceFrom (b);
            if (length <= 0.0f)
                continue;
            const auto along = (b - a) / length;
            const juce::Point<float> outward { along.y, -along.x };  // the blob runs clockwise on screen
            for (auto s = 0.0f; s < length; s += 1.0f)
            {
                const auto at = walked + s;
                while (tooth < ends.size() - 1 && at > ends[tooth])
                    ++tooth;
                const auto start = tooth == 0 ? 0.0f : ends[tooth - 1];
                const auto shape = std::sqrt (std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, (at - start) / (ends[tooth] - start))));
                const auto p = a + along * s + outward * ((1.5f + 4.0f * hash (seed, 200 + tooth)) * shape);
                if (result.isEmpty() && i == 0 && s == 0.0f) result.startNewSubPath (p); else result.lineTo (p);
            }
            walked += length;
        }
        result.closeSubPath();
        return result;
    }

    /** The worm along `centreLine`: outlined pink tube with ribs and a shine, the ribs shifted
        along by `travel` pixels so they flow as the tape runs. */
    static void drawWorm (juce::Graphics& g, const juce::Path& centreLine, float travel)
    {
        const auto joint = juce::PathStrokeType::curved;
        const auto cap = juce::PathStrokeType::rounded;
        g.setColour (ink);
        g.strokePath (centreLine, juce::PathStrokeType (wormWidth + 3.0f, joint, cap));
        g.setColour (wormRib.darker (0.2f));
        g.strokePath (centreLine, juce::PathStrokeType (wormWidth, joint, cap));
        g.setColour (wormPink);
        g.strokePath (centreLine, juce::PathStrokeType (wormWidth - 3.0f, joint, cap));

        // Ribs across the tube, each bowed a little as if it wraps round it.
        juce::Path ribs;
        const auto half = wormWidth * 0.5f - 1.5f;
        auto next = std::fmod (travel, ribSpacing), walked = 0.0f;
        for (juce::PathFlatteningIterator it (centreLine); it.next();)
        {
            const juce::Point<float> a { it.x1, it.y1 }, b { it.x2, it.y2 };
            const auto length = a.getDistanceFrom (b);
            if (length <= 0.0f)
                continue;
            const auto along = (b - a) / length;
            const juce::Point<float> across { -along.y, along.x };
            for (; next <= walked + length; next += ribSpacing)
            {
                const auto p = a + along * (next - walked);
                ribs.startNewSubPath (p - across * half);
                ribs.quadraticTo (p + along * 2.5f, p + across * half);
            }
            walked += length;
        }
        g.setColour (wormRib);
        g.strokePath (ribs, juce::PathStrokeType (2.0f, joint, juce::PathStrokeType::rounded));

        // Shine running along the top of the tube.
        g.setColour (wormShine.withAlpha (0.7f));
        g.strokePath (centreLine, juce::PathStrokeType (2.0f, joint, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-1.0f, -wormWidth * 0.22f));
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
        const auto travel = turningSpeed * tapeRate * dt;
        supplyAngle = std::fmod (supplyAngle + travel / supplyPackRadius, juce::MathConstants<float>::twoPi);
        takeUpAngle = std::fmod (takeUpAngle + travel / takeUpPackRadius, juce::MathConstants<float>::twoPi);
        tapeTravel = std::fmod (tapeTravel + travel, 1000.0f);

        const auto second = (int) processor.hostSeconds.load();
        if (turningSpeed > 0.0f || second != shownSecond || (turningSpeed > 0.01f) != wasRunning)
            repaint();
        shownSecond = second;
        wasRunning = turningSpeed > 0.01f;
    }

    PluginProcessor& processor;
    double lastTime = juce::Time::getMillisecondCounterHiRes();
    float turningSpeed = 0.0f, supplyAngle = 0.0f, takeUpAngle = 0.4f, tapeTravel = 0.0f;
    int shownSecond = -1;
    bool wasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReelDeck)
};
