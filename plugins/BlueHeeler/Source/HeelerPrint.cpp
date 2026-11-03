#include "HeelerPrint.h"

namespace
{
    using P = juce::Point<float>;

    // Everything is drawn on a 400 x 170 plate, ground at y = 158, then fitted to the area.
    constexpr float plateW = 400.0f, plateH = 170.0f, ground = 158.0f;

    /** Cuts `cut` out of `shape`: both are filled even-odd, so the cut-outs must not overlap. */
    void cutOut (juce::Path& shape, const juce::Path& cut)
    {
        shape.addPath (cut);
    }

    /** A line drawn in the paper colour, i.e. a stroke cut out of the silhouette. */
    void cutLine (juce::Path& shape, const juce::Path& line, float width)
    {
        juce::Path stroked;
        juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (stroked, line);
        cutOut (shape, stroked);
    }

    juce::Path cowLeg()
    {
        juce::Path p;
        p.startNewSubPath (284.0f, -10.0f);                        // back of the gaskin
        p.cubicTo (280.0f, 14.0f, 272.0f, 34.0f, 276.0f, 48.0f);   // down to the point of the hock
        p.cubicTo (279.0f, 56.0f, 290.0f, 60.0f, 293.0f, 68.0f);
        p.cubicTo (294.0f, 88.0f, 293.0f, 104.0f, 292.0f, 114.0f); // back of the cannon
        p.cubicTo (284.0f, 118.0f, 285.0f, 128.0f, 293.0f, 129.0f); // dewclaw
        p.cubicTo (297.0f, 134.0f, 298.0f, 140.0f, 297.0f, 146.0f); // pastern to the heel bulb
        p.cubicTo (296.0f, 152.0f, 296.0f, 156.0f, 299.0f, ground);
        p.lineTo (344.0f, ground);                                 // sole, toe forward
        p.cubicTo (338.0f, 150.0f, 330.0f, 140.0f, 324.0f, 134.0f); // front wall of the hoof
        p.cubicTo (321.0f, 128.0f, 319.0f, 120.0f, 318.0f, 112.0f); // fetlock
        p.cubicTo (316.0f, 96.0f, 315.0f, 78.0f, 316.0f, 64.0f);   // front of the cannon
        p.cubicTo (320.0f, 40.0f, 336.0f, 14.0f, 352.0f, -10.0f);  // stifle and gaskin
        p.closeSubPath();
        return p;
    }

    juce::Path heeler()
    {
        juce::Path p;
        p.startNewSubPath (74.0f, 60.0f);                          // rump
        p.cubicTo (110.0f, 54.0f, 140.0f, 58.0f, 160.0f, 66.0f);   // topline to the withers
        p.cubicTo (180.0f, 74.0f, 198.0f, 86.0f, 210.0f, 94.0f);   // crest of the neck, head low

        // Far ear, then the near one, big and pricked.
        p.quadraticTo (208.0f, 80.0f, 214.0f, 68.0f);
        p.quadraticTo (218.0f, 80.0f, 221.0f, 92.0f);
        p.quadraticTo (222.0f, 76.0f, 230.0f, 62.0f);
        p.quadraticTo (242.0f, 78.0f, 246.0f, 100.0f);

        p.cubicTo (256.0f, 102.0f, 266.0f, 110.0f, 272.0f, 118.0f); // broad skull to the stop
        p.cubicTo (284.0f, 122.0f, 294.0f, 128.0f, 300.0f, 134.0f); // muzzle
        p.cubicTo (304.0f, 136.0f, 305.0f, 141.0f, 301.0f, 142.0f); // nose

        // Jaws open round the heel.
        p.cubicTo (294.0f, 142.0f, 288.0f, 142.0f, 282.0f, 142.0f);
        p.cubicTo (286.0f, 146.0f, 291.0f, 149.0f, 296.0f, 151.0f);
        p.cubicTo (297.0f, 155.0f, 292.0f, 157.0f, 286.0f, 157.0f);
        p.cubicTo (272.0f, 157.0f, 256.0f, 155.0f, 244.0f, 152.0f);

        p.cubicTo (226.0f, 154.0f, 208.0f, 152.0f, 198.0f, 146.0f); // throat to the chest

        // Near foreleg, reaching and planted.
        p.cubicTo (206.0f, 148.0f, 212.0f, 150.0f, 216.0f, 153.0f);
        p.cubicTo (222.0f, 154.0f, 226.0f, 156.0f, 226.0f, ground);
        p.lineTo (204.0f, ground);
        p.cubicTo (200.0f, 154.0f, 190.0f, 148.0f, 180.0f, 142.0f);

        // Far foreleg, folded under.
        p.cubicTo (174.0f, 148.0f, 168.0f, 154.0f, 168.0f, ground);
        p.lineTo (150.0f, ground);
        p.cubicTo (152.0f, 150.0f, 152.0f, 144.0f, 152.0f, 138.0f);

        p.cubicTo (134.0f, 134.0f, 114.0f, 126.0f, 104.0f, 122.0f); // belly, tucked up

        // Hind leg, driving back.
        p.cubicTo (100.0f, 132.0f, 92.0f, 140.0f, 84.0f, 146.0f);
        p.cubicTo (78.0f, 150.0f, 74.0f, 154.0f, 74.0f, ground);
        p.lineTo (54.0f, ground);
        p.cubicTo (56.0f, 150.0f, 58.0f, 142.0f, 60.0f, 134.0f);
        p.cubicTo (56.0f, 116.0f, 60.0f, 80.0f, 74.0f, 60.0f);     // buttock up to the rump
        p.closeSubPath();
        return p;
    }

    juce::Path dogTail()
    {
        juce::Path p;
        p.startNewSubPath (76.0f, 62.0f);
        p.cubicTo (62.0f, 70.0f, 50.0f, 86.0f, 40.0f, 106.0f);
        p.cubicTo (52.0f, 100.0f, 64.0f, 88.0f, 72.0f, 80.0f);
        p.closeSubPath();
        return p;
    }

    juce::Path tail()
    {
        juce::Path p;
        p.startNewSubPath (278.0f, -10.0f);
        p.cubicTo (276.0f, 12.0f, 270.0f, 28.0f, 262.0f, 40.0f);
        return p;
    }
}

void HeelerPrint::paint (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, juce::Colour paper)
{
    const auto plate = juce::Rectangle<float> (plateW, plateH);
    const auto toArea = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                            .getTransformToFit (plate, area);
    const auto scale = std::sqrt (std::abs (toArea.getDeterminant()));
    const auto frame = plate.transformedBy (toArea);

    g.saveState();
    g.reduceClipRegion (frame.toNearestIntEdges());
    g.setColour (ink);

    // The far hind leg, a step ahead, in outline only.
    const auto farLeg = cowLeg().createPathWithRoundedCorners (2.0f);
    g.strokePath (farLeg, juce::PathStrokeType (1.4f * scale),
                  juce::AffineTransform::translation (44.0f, -6.0f).followedBy (toArea));

    // Tail rope and its switch, swinging.
    const auto rope = tail();
    g.strokePath (rope, juce::PathStrokeType (4.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), toArea);
    juce::Random rng (0xc0);
    for (int i = 0; i < 12; ++i)
    {
        juce::Path strand;
        const auto spread = ((float) i / 11.0f - 0.5f) * 22.0f;
        strand.startNewSubPath (263.0f, 38.0f);
        strand.quadraticTo (259.0f + spread * 0.4f, 54.0f, 254.0f + spread * 0.8f, 68.0f + rng.nextFloat() * 8.0f);
        g.strokePath (strand, juce::PathStrokeType (1.8f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), toArea);
    }

    // The near hind leg, with the coronet and the cleft of the hoof cut back.
    auto leg = cowLeg();
    leg.setUsingNonZeroWinding (false);
    {
        juce::Path coronet;
        coronet.startNewSubPath (297.0f, 137.0f);
        coronet.quadraticTo (310.0f, 140.0f, 323.0f, 134.0f);
        cutLine (leg, coronet, 2.2f);

        juce::Path cleft;
        cleft.startNewSubPath (325.0f, 141.0f);
        cleft.lineTo (331.0f, ground - 2.0f);
        cutLine (leg, cleft, 2.0f);

        juce::Path hock;
        hock.startNewSubPath (288.0f, 26.0f);
        hock.quadraticTo (284.0f, 42.0f, 290.0f, 54.0f);
        cutLine (leg, hock, 1.6f);
    }
    g.fillPath (leg, toArea);

    // The dog, with eye, inner ear, shoulder and elbow cut back, and the coat ticked with flecks.
    auto dog = heeler();
    dog.setUsingNonZeroWinding (false);
    {
        juce::Path eye;
        eye.addEllipse (juce::Rectangle<float> (7.0f, 4.5f).withCentre ({ 256.0f, 114.0f }));
        cutOut (dog, eye);

        juce::Path brow;
        brow.startNewSubPath (248.0f, 107.0f);
        brow.quadraticTo (256.0f, 105.0f, 263.0f, 109.0f);
        cutLine (dog, brow, 1.2f);

        juce::Path innerEar;
        innerEar.startNewSubPath (231.0f, 70.0f);
        innerEar.quadraticTo (238.0f, 82.0f, 240.0f, 96.0f);
        innerEar.lineTo (230.0f, 96.0f);
        innerEar.closeSubPath();
        cutOut (dog, innerEar);

        juce::Path cheek;
        cheek.startNewSubPath (244.0f, 124.0f);
        cheek.quadraticTo (258.0f, 138.0f, 281.0f, 141.0f);
        cutLine (dog, cheek, 1.3f);

        juce::Path shoulder;
        shoulder.startNewSubPath (166.0f, 76.0f);
        shoulder.cubicTo (176.0f, 92.0f, 186.0f, 114.0f, 190.0f, 134.0f);
        cutLine (dog, shoulder, 1.6f);

        juce::Path elbow;
        elbow.startNewSubPath (166.0f, 128.0f);
        elbow.quadraticTo (174.0f, 134.0f, 178.0f, 142.0f);
        cutLine (dog, elbow, 1.4f);

        juce::Path stifle;
        stifle.startNewSubPath (84.0f, 66.0f);
        stifle.cubicTo (100.0f, 78.0f, 104.0f, 100.0f, 96.0f, 124.0f);
        cutLine (dog, stifle, 1.6f);

        // Ticking: a scatter of short flecks along the lie of the coat, over the back.
        juce::Random ticks (0xb1ee);
        for (int i = 0; i < 30; ++i)
        {
            const P at (106.0f + ticks.nextFloat() * 54.0f, 62.0f + ticks.nextFloat() * 40.0f);
            if (at.y > 62.0f && at.y < 108.0f)
            {
                juce::Path fleck;
                fleck.startNewSubPath (at);
                fleck.lineTo (at.translated (-2.4f, -0.5f));
                cutLine (dog, fleck, 1.2f);
            }
        }
    }

    g.fillPath (dogTail(), toArea);

    // A thin gap cut round the dog where it crosses the leg (and its own tail), so the bite reads.
    g.setColour (paper);
    g.strokePath (dog, juce::PathStrokeType (3.0f * scale), toArea);
    g.setColour (ink);
    g.fillPath (dog, toArea);

    // Dust kicked up under the heel, and the ground.
    for (auto [x, y, r] : { std::tuple { 312.0f, 150.0f, 2.4f }, { 318.0f, 143.0f, 1.6f }, { 238.0f, 156.0f, 2.0f },
                            { 352.0f, 154.0f, 2.6f }, { 360.0f, 149.0f, 1.8f } })
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ x, y }).transformedBy (toArea), 1.2f * scale);

    for (auto [x, w] : { std::pair { 8.0f, 30.0f }, { 46.0f, 96.0f }, { 236.0f, 112.0f }, { 356.0f, 40.0f } })
        g.fillRect (juce::Rectangle<float> (x, ground, w, 2.0f).transformedBy (toArea));

    g.restoreState();
    g.setColour (ink);
    g.drawRoundedRectangle (frame.expanded (4.0f), 6.0f, 1.6f);
}
