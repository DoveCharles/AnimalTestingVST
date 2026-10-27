#include "CourtPainting.h"
#include "PedalLookAndFeel.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;
    using P = juce::Point<float>;

    const juce::Colour ink       { 0xff2b1d14 };
    const juce::Colour vermilion { 0xffb3402f };
    const juce::Colour lacquer   { 0xff7a2a1e };
    const juce::Colour malachite { 0xff6a957a };
    const juce::Colour azurite   { 0xff5a7c9c };
    const juce::Colour lilac     { 0xff9d92a8 };
    const juce::Colour skin      { 0xfff3e6d0 };
    const juce::Colour faceInk   { 0xff8a4a32 };
    const juce::Colour hair      { 0xff1d1714 };
    const juce::Colour hatBlack  { 0xff241d1a };
    const juce::Colour cane      { 0xffa08454 };
    const juce::Colour sage      { 0xff8e9f86 };
    const juce::Colour mist      { 0xffaebdc0 };
    const juce::Colour blush     { 0xffdcb4a6 };
    const juce::Colour ivory     { 0xffe8e0c8 };
    const juce::Colour plum      { 0xffb4a4b6 };
    const juce::Colour skirtRed  { 0xffc0533d };
    const juce::Colour gold      { 0xffc9a24a };
    const juce::Colour slate     { 0xff4b5552 };
    const juce::Colour marble    { 0xffd8cfb9 };
    const juce::Colour bronze    { 0xff6f7050 };
    const juce::Colour pine      { 0xff2f5e45 };
    const juce::Colour bark      { 0xff6b4a32 };

    //==============================================================================
    /** The fine, even "iron-wire" ink line that outlines everything in gongbi. */
    void stroke (juce::Graphics& g, const juce::Path& path, float width = 0.7f, float alpha = 0.9f)
    {
        g.setColour (ink.withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Layered washes the gongbi way: a flat ground of colour, then the same colour deepened in a soft
        band just inside the contour (fen ran) so forms turn without any light or cast shadow, then
        the ink line (none if `outline` is 0). */
    void fill (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float outline = 0.7f)
    {
        g.setColour (colour);
        g.fillPath (path);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (path);
            const auto b = path.getBounds();
            const auto band = juce::jmin (7.0f, juce::jmin (b.getWidth(), b.getHeight()) * 0.35f);
            for (auto [width, alpha] : { std::pair { 2.0f, 0.04f }, { 1.2f, 0.05f }, { 0.6f, 0.07f } })
            {
                g.setColour (colour.darker (0.7f).withAlpha (alpha));
                g.strokePath (path, juce::PathStrokeType (band * width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        if (outline > 0.0f)
            stroke (g, path, outline);
    }

    juce::Path rect (juce::Rectangle<float> r)
    {
        juce::Path path;
        path.addRectangle (r);
        return path;
    }

    void fill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float outline = 0.9f)
    {
        fill (g, rect (r), colour, outline);
    }

    juce::Path curve (P a, P control, P b)
    {
        juce::Path path;
        path.startNewSubPath (a);
        path.quadraticTo (control, b);
        return path;
    }

    /** A strip along `spine` whose width at each fraction t of its length is `widthAt (t)`. */
    template <typename Width>
    juce::Path sweep (const juce::Path& spine, Width widthAt)
    {
        constexpr int steps = 40;
        const auto length = spine.getLength();
        juce::Array<P> left, right;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = (float) i / (float) steps;
            const auto p = spine.getPointAlongPath (length * t);
            auto d = spine.getPointAlongPath (juce::jmin (length, length * t + 0.5f))
                   - spine.getPointAlongPath (juce::jmax (0.0f, length * t - 0.5f));
            d /= juce::jmax (0.001f, d.getDistanceFromOrigin());
            const auto n = P (-d.y, d.x) * (widthAt (t) * 0.5f);
            left.add (p + n);
            right.add (p - n);
        }

        juce::Path out;
        out.startNewSubPath (left.getFirst());
        for (int i = 1; i < left.size(); ++i)
            out.lineTo (left[i]);
        for (int i = right.size(); --i >= 0;)
            out.lineTo (right[i]);
        out.closeSubPath();
        return out;
    }

    /** A stroke that thins from `startWidth` to `endWidth` along `spine`, for branches and ribbons. */
    juce::Path taper (const juce::Path& spine, float startWidth, float endWidth)
    {
        return sweep (spine, [=] (float t) { return juce::jmap (t, startWidth, endWidth); });
    }

    void softEllipse (juce::Graphics& g, P centre, float width, float height, juce::Colour colour)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::scale (1.0f, height / width, centre.x, centre.y));
        g.setGradientFill (juce::ColourGradient (colour, centre, colour.withAlpha (0.0f), centre.translated (width * 0.5f, 0.0f), true));
        g.fillEllipse (juce::Rectangle<float> (width, width).withCentre (centre));
    }

    //==============================================================================
    // Silk, mount and age.

    void paintMount (juce::Graphics& g, juce::Rectangle<float> box)
    {
        g.setColour (Palette::enclosureDark);
        g.fillRoundedRectangle (box, 14.0f);

        // A brocade of small four-petalled gold flowers.
        g.setColour (gold.withAlpha (0.28f));
        for (auto y = box.getY() + 4.0f, row = 0.0f; y < box.getBottom(); y += 7.0f, row += 1.0f)
            for (auto x = box.getX() + 4.0f + std::fmod (row, 2.0f) * 3.5f; x < box.getRight(); x += 7.0f)
            {
                g.fillEllipse (x - 1.6f, y - 0.6f, 3.2f, 1.2f);
                g.fillEllipse (x - 0.6f, y - 1.6f, 1.2f, 3.2f);
            }
    }

    void paintSilk (juce::Graphics& g, juce::Rectangle<float> face)
    {
        g.setGradientFill (juce::ColourGradient (Palette::enclosure.brighter (0.08f), face.getCentre().translated (-30.0f, -60.0f),
                                                 Palette::enclosure.darker (0.12f), face.getTopLeft(), true));
        g.fillRect (face);

        // Warp and weft: faint threads both ways.
        juce::Random random (0x7085);
        for (int i = 0; i < 160; ++i)
        {
            const auto y = face.getY() + random.nextFloat() * face.getHeight();
            g.setColour ((random.nextBool() ? juce::Colours::white : ink).withAlpha (0.012f + random.nextFloat() * 0.02f));
            g.fillRect (juce::Rectangle<float> (face.getX(), y, face.getWidth(), 0.5f));
        }
        for (int i = 0; i < 110; ++i)
        {
            const auto x = face.getX() + random.nextFloat() * face.getWidth();
            g.setColour (ink.withAlpha (0.008f + random.nextFloat() * 0.014f));
            g.fillRect (juce::Rectangle<float> (x, face.getY(), 0.5f, face.getHeight()));
        }

        // Foxing.
        for (int i = 0; i < 18; ++i)
        {
            const auto r = 5.0f + random.nextFloat() * 26.0f;
            softEllipse (g, { face.getX() + random.nextFloat() * face.getWidth(), face.getY() + random.nextFloat() * face.getHeight() },
                         r * 2.0f, r * (1.2f + random.nextFloat() * 0.8f), juce::Colour (0xff8a6a3a).withAlpha (0.08f + random.nextFloat() * 0.05f));
        }
    }

    /** Pigment flaked back to the silk, a fine craquelure and smoke-darkened edges. */
    void paintWear (juce::Graphics& g, juce::Rectangle<float> face)
    {
        juce::Random random (0x0a9e);
        g.setColour (Palette::enclosure.withAlpha (0.4f));
        for (int i = 0; i < 1600; ++i)
        {
            const auto s = 0.4f + random.nextFloat() * 1.2f;
            g.fillEllipse (face.getX() + random.nextFloat() * face.getWidth(), face.getY() + random.nextFloat() * face.getHeight(), s, s * 0.8f);
        }

        g.setColour (ink.withAlpha (0.07f));
        for (int i = 0; i < 90; ++i)
        {
            P p { face.getX() + random.nextFloat() * face.getWidth(), face.getY() + random.nextFloat() * face.getHeight() };
            auto angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
            juce::Path crack;
            crack.startNewSubPath (p);
            for (int k = 0; k < 7; ++k)
            {
                angle += (random.nextFloat() - 0.5f) * 1.4f;
                p += P (std::cos (angle), std::sin (angle)) * (3.0f + random.nextFloat() * 5.0f);
                crack.lineTo (p);
            }
            g.strokePath (crack, juce::PathStrokeType (0.5f));
        }

        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, face.getCentre().translated (0.0f, -20.0f),
                                                 juce::Colour (0xff5a3d1e).withAlpha (0.32f), face.getTopLeft(), true));
        g.fillRect (face);
    }

    //==============================================================================
    // Landscape.

    void paintHills (juce::Graphics& g, juce::Rectangle<float> face)
    {
        // Far hills in blue-green wash, fading into mist at the foot of the silk.
        for (int layer = 0; layer < 2; ++layer)
        {
            juce::Random random (0x4111 + layer);
            const auto top = layer == 0 ? 520.0f : 560.0f;
            const auto colour = layer == 0 ? azurite : malachite;

            juce::Path hills;
            auto x = face.getX() - 10.0f;
            hills.startNewSubPath (x, face.getBottom() + 2.0f);
            hills.lineTo (x, top + 30.0f);
            while (x < face.getRight())
            {
                const auto step = 50.0f + random.nextFloat() * 60.0f;
                hills.quadraticTo (x + step * 0.5f, top - 10.0f - random.nextFloat() * 24.0f,
                                   x + step, top + 16.0f + random.nextFloat() * 30.0f);
                x += step;
            }
            hills.lineTo (x, face.getBottom() + 2.0f);
            hills.closeSubPath();

            g.setGradientFill (juce::ColourGradient (colour.withAlpha (layer == 0 ? 0.16f : 0.2f), { 0.0f, top - 20.0f },
                                                     colour.withAlpha (0.0f), { 0.0f, top + 70.0f }, false));
            g.fillPath (hills);
            g.setGradientFill (juce::ColourGradient (ink.withAlpha (0.16f), { 0.0f, top - 20.0f },
                                                     ink.withAlpha (0.0f), { 0.0f, top + 50.0f }, false));
            g.strokePath (hills, juce::PathStrokeType (0.8f));
        }
    }

    void paintGrass (juce::Graphics& g, P base, float size)
    {
        juce::Random random ((juce::int64) (base.x * 31.0f + base.y));
        for (int i = 0; i < 7; ++i)
        {
            const auto lean = (random.nextFloat() - 0.5f) * 1.4f;
            const auto len = size * (0.6f + random.nextFloat() * 0.6f);
            juce::Path blade;
            blade.startNewSubPath (base.translated (-1.1f, 0.0f));
            blade.quadraticTo (base.translated (lean * len * 0.2f, -len * 0.6f), base.translated (lean * len * 0.8f, -len));
            blade.quadraticTo (base.translated (lean * len * 0.25f + 1.0f, -len * 0.5f), base.translated (1.1f, 0.0f));
            blade.closeSubPath();
            g.setColour (malachite.darker (0.2f + random.nextFloat() * 0.4f).withAlpha (0.85f));
            g.fillPath (blade);
        }
    }

    void paintPine (juce::Graphics& g)
    {
        juce::Path limb;
        limb.startNewSubPath (470.0f, 22.0f);
        limb.cubicTo (440.0f, 44.0f, 406.0f, 30.0f, 378.0f, 46.0f);
        limb.quadraticTo (362.0f, 56.0f, 342.0f, 54.0f);

        juce::Path twig;
        twig.startNewSubPath (414.0f, 36.0f);
        twig.quadraticTo (418.0f, 52.0f, 408.0f, 62.0f);

        juce::Random random (0x9172);

        // Needles in the wheel form: each cluster a fan of fine strokes radiating from the tip of
        // a short twig on the branch, laid over a flat wash of pale green, the fan opening away
        // from the twig so the needles read as growing out of it.
        const auto cluster = [&] (P base, P tip, float radius)
        {
            g.fillPath (taper (curve (base, (base + tip) * 0.5f + P (0.0f, -1.5f), tip), 2.0f, 1.0f));

            const auto heading = std::atan2 (tip.y - base.y, tip.x - base.x);
            const auto spread = juce::degreesToRadians (95.0f);
            juce::Path wash;
            wash.startNewSubPath (tip);
            for (int i = 0; i <= 16; ++i)
            {
                const auto angle = heading - spread + spread * 2.0f * (float) i / 16.0f;
                wash.lineTo (tip + P (std::cos (angle), std::sin (angle)) * radius * 0.92f);
            }
            wash.closeSubPath();
            g.setColour (pine.brighter (0.6f).withAlpha (0.45f));
            g.fillPath (wash);

            for (auto [count, colour, reach, offset] : { std::tuple { 19, pine.brighter (0.3f), 0.95f, 0.5f }, { 19, pine, 1.0f, 0.0f } })
                for (int i = 0; i < count; ++i)
                {
                    const auto angle = heading - spread + spread * 2.0f * ((float) i + offset) / (float) (count - 1);
                    const auto len = radius * reach * (0.82f + random.nextFloat() * 0.18f);
                    g.setColour (colour);
                    g.drawLine ({ tip, tip + P (std::cos (angle), std::sin (angle)) * len }, 0.45f);
                }

            g.setColour (bark.darker (0.3f));
            g.fillEllipse (juce::Rectangle<float> (1.6f, 1.6f).withCentre (tip));
            g.setColour (bark);
        };

        // Clusters first, mostly held up and out from the branch, so the limb crosses the foot of their twigs.
        g.setColour (bark);
        for (const auto& [t, dx, dy, radius] : { std::tuple { 0.18f, -2.0f, -11.0f, 10.0f }, { 0.36f, -6.0f, -12.0f, 11.0f },
                                                 { 0.5f, 3.0f, 12.0f, 9.0f }, { 0.62f, -7.0f, -11.0f, 11.0f },
                                                 { 0.8f, -4.0f, -12.0f, 10.0f }, { 1.0f, -10.0f, -3.0f, 10.0f } })
        {
            const auto base = limb.getPointAlongPath (limb.getLength() * t);
            cluster (base, base + P (dx, dy), radius);
        }
        cluster (twig.getPointAlongPath (twig.getLength() * 0.5f), twig.getPointAlongPath (twig.getLength() * 0.5f) + P (9.0f, 3.0f), 9.0f);
        cluster (twig.getPointAlongPath (twig.getLength()), { 400.0f, 68.0f }, 10.0f);

        fill (g, taper (twig, 4.0f, 1.5f), bark, 0.9f);
        fill (g, taper (limb, 11.0f, 2.5f), bark, 0.9f);

        // Scaly bark.
        for (int i = 0; i < 14; ++i)
        {
            const auto p = limb.getPointAlongPath (limb.getLength() * (0.05f + 0.6f * random.nextFloat()));
            stroke (g, curve (p.translated (-2.0f, -1.5f), p.translated (0.0f, 1.0f), p.translated (2.5f, -1.0f)), 0.7f, 0.45f);
        }
    }

    //==============================================================================
    // The pavilion: tiled eave, painted brackets, red pillars, a bamboo blind half rolled over a
    // lattice screen, and a balustrade with green panels on a marble platform.

    void paintPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        fill (g, r, malachite, 0.7f);
        const auto inner = r.reduced (2.0f);
        g.setColour (gold.withAlpha (0.85f));
        g.drawRect (inner, 0.8f);
        juce::Path diamond;
        diamond.addQuadrilateral (inner.getCentreX(), inner.getY(), inner.getRight(), inner.getCentreY(),
                                  inner.getCentreX(), inner.getBottom(), inner.getX(), inner.getCentreY());
        g.strokePath (diamond, juce::PathStrokeType (0.8f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (inner.getCentre()));
    }

    void paintPavilion (juce::Graphics& g, float left)
    {
        const auto right = left + 104.0f;

        // Back wall.
        fill (g, juce::Rectangle<float> (left, 56.0f, right - left, 100.0f), juce::Colour (0xffe6cf98), 0.0f);

        // Bamboo blind, half rolled, with red ties.
        const juce::Rectangle<float> blind (left + 14.0f, 56.0f, right - left - 26.0f, 38.0f);
        fill (g, blind, juce::Colour (0xffc9ae72), 0.8f);
        g.setColour (ink.withAlpha (0.2f));
        for (auto y = blind.getY() + 2.0f; y < blind.getBottom(); y += 2.0f)
            g.fillRect (juce::Rectangle<float> (blind.getX(), y, blind.getWidth(), 0.5f));
        juce::Path roll;
        roll.addRoundedRectangle (blind.getX() - 1.0f, blind.getBottom() - 2.0f, blind.getWidth() + 2.0f, 5.0f, 2.5f);
        fill (g, roll, juce::Colour (0xffb39558), 0.8f);
        for (auto x : { blind.getX() + 12.0f, blind.getRight() - 12.0f })
        {
            g.setColour (vermilion);
            g.drawLine (x, blind.getY(), x, blind.getBottom() + 9.0f, 1.2f);
            g.fillEllipse (x - 2.0f, blind.getBottom() + 7.0f, 4.0f, 6.0f);
        }

        // Lattice screen.
        const juce::Rectangle<float> screen (left + 16.0f, 106.0f, right - left - 30.0f, 44.0f);
        fill (g, screen, juce::Colour (0xfff0dfb2), 0.0f);
        g.setColour (vermilion.withAlpha (0.8f));
        for (auto x = screen.getX() + 5.5f; x < screen.getRight(); x += 5.5f)
            g.drawLine (x, screen.getY(), x, screen.getBottom(), 0.9f);
        for (auto y = screen.getY() + 5.5f; y < screen.getBottom(); y += 5.5f)
            g.drawLine (screen.getX(), y, screen.getRight(), y, 0.9f);
        g.setColour (gold);
        g.drawRect (screen, 1.5f);

        // Carpet with gold roundels.
        fill (g, juce::Rectangle<float> (left, 150.0f, right - left, 10.0f), juce::Colour (0xffa13a2b), 0.6f);
        g.setColour (gold.withAlpha (0.9f));
        for (auto x = left + 3.0f; x < right; x += 12.0f)
            g.fillEllipse (x, 152.5f, 5.0f, 5.0f);

        // Pillars, lit from the left.
        for (auto x : { left + 3.0f, right - 11.0f })
        {
            fill (g, rect ({ x, 56.0f, 8.0f, 134.0f }), vermilion);
            fill (g, juce::Rectangle<float> (x - 1.0f, 56.0f, 10.0f, 4.0f), gold, 0.7f);
            fill (g, juce::Rectangle<float> (x - 1.5f, 185.0f, 11.0f, 5.0f), marble, 0.7f);
        }

        // Lintel with gold swirls, a band of green brackets on blue, and the rafter ends.
        fill (g, juce::Rectangle<float> (left - 2.0f, 48.0f, right - left + 14.0f, 8.0f), vermilion, 0.8f);
        g.setColour (gold);
        for (auto x = left + 2.0f; x < right + 8.0f; x += 11.0f)
            g.drawEllipse (x, 49.5f, 8.0f, 5.0f, 0.9f);
        fill (g, juce::Rectangle<float> (left - 2.0f, 39.0f, right - left + 16.0f, 9.0f), azurite, 0.8f);
        for (auto x = left; x < right + 12.0f; x += 11.0f)
        {
            juce::Path bracket;
            bracket.addQuadrilateral (x + 1.0f, 48.0f, x + 3.0f, 40.0f, x + 8.0f, 40.0f, x + 10.0f, 48.0f);
            fill (g, bracket, malachite, 0.6f);
            g.setColour (gold);
            g.drawLine (x + 3.0f, 40.5f, x + 8.0f, 40.5f, 1.0f);
        }
        fill (g, juce::Rectangle<float> (left - 2.0f, 33.0f, right - left + 20.0f, 6.0f), malachite.darker (0.2f), 0.6f);
        for (auto x = left; x < right + 16.0f; x += 6.0f)
        {
            g.setColour (azurite);
            g.fillEllipse (x - 2.2f, 33.8f, 4.4f, 4.4f);
            g.setColour (gold);
            g.fillEllipse (x - 0.8f, 35.2f, 1.6f, 1.6f);
        }

        // Tiled eave with an upturned corner.
        juce::Path eave;
        eave.startNewSubPath (left - 4.0f, 10.0f);
        eave.lineTo (right - 6.0f, 10.0f);
        eave.quadraticTo (right + 8.0f, 20.0f, right + 26.0f, 14.0f);
        eave.quadraticTo (right + 18.0f, 28.0f, right + 6.0f, 34.0f);
        eave.lineTo (left - 4.0f, 34.0f);
        eave.closeSubPath();
        fill (g, eave, slate);
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (eave);
            g.setColour (slate.darker (0.5f));
            for (auto x = left - 2.0f; x < right + 20.0f; x += 5.0f)
                g.drawLine (x, 10.0f, x + 1.5f, 34.0f, 1.0f);
        }
        for (auto x = left; x < right + 8.0f; x += 5.5f)
        {
            juce::Path tileEnd;
            tileEnd.addEllipse (x - 2.2f, 31.0f, 4.4f, 4.4f);
            fill (g, tileEnd, slate.brighter (0.4f), 0.6f);
        }
        stroke (g, curve ({ right + 26.0f, 14.0f }, { right + 31.0f, 9.0f }, { right + 28.0f, 6.0f }), 1.6f, 0.8f);

        // Balustrade.
        const auto railTop = 158.0f, railBottom = 186.0f;
        for (auto x = left; x < right; x += 20.0f)
            paintPanel (g, { x + 4.5f, railTop + 4.0f, juce::jmin (15.0f, right + 2.0f - x - 4.5f), railBottom - railTop - 7.0f });
        fill (g, juce::Rectangle<float> (left - 2.0f, railTop, right - left + 6.0f, 4.0f), vermilion, 0.7f);
        fill (g, juce::Rectangle<float> (left - 2.0f, railBottom - 3.0f, right - left + 6.0f, 4.0f), vermilion, 0.7f);
        for (auto x = left - 1.0f; x < right + 6.0f; x += 20.0f)
        {
            juce::Path post;
            post.addRoundedRectangle (x, railTop - 5.0f, 5.0f, railBottom - railTop + 5.0f, 1.5f);
            fill (g, post, vermilion.darker (0.1f), 0.7f);
            juce::Path finial;
            finial.addEllipse (x - 1.0f, railTop - 11.0f, 7.0f, 7.5f);
            fill (g, finial, gold, 0.7f);
        }

        // Marble platform and steps.
        fill (g, juce::Rectangle<float> (left - 2.0f, 190.0f, right - left + 10.0f, 13.0f), marble, 0.8f);
        g.setColour (ink.withAlpha (0.25f));
        g.drawLine (left, 194.0f, right + 6.0f, 194.0f, 0.7f);
        g.drawLine (left, 199.5f, right + 6.0f, 199.5f, 0.7f);
        fill (g, juce::Rectangle<float> (right - 34.0f, 203.0f, 42.0f, 6.0f), marble.darker (0.06f), 0.8f);
        fill (g, juce::Rectangle<float> (right - 28.0f, 209.0f, 38.0f, 6.0f), marble.darker (0.12f), 0.8f);
    }

    //==============================================================================
    // Figures. Each is drawn in its own unit frame: feet at the origin, the top of the head at
    // y = -1, facing +x, so one set of shapes serves for either direction and any size. Contours
    // are smooth curves through hand-placed points, laid in flat colour and outlined in short,
    // overlapping brush strokes that press in and lift off, the way the painters drew them.

    /** A Catmull-Rom curve from `points[first]` across `segments` spans, wrapping round if closed. */
    juce::Path smoothSpan (const std::vector<P>& points, int first, int segments, bool closed)
    {
        const auto n = (int) points.size();
        const auto at = [&] (int i) { return points[(size_t) (closed ? (i % n + n) % n : juce::jlimit (0, n - 1, i))]; };

        juce::Path path;
        path.startNewSubPath (at (first));
        for (int i = first; i < first + segments; ++i)
        {
            const auto p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            path.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
        }
        return path;
    }

    juce::Path smooth (const std::vector<P>& points, bool closed)
    {
        auto path = smoothSpan (points, 0, (int) points.size() - (closed ? 0 : 1), closed);
        if (closed)
            path.closeSubPath();
        return path;
    }

    /** One brush line along `spine`: pressed in at the start, even through the middle, lifted to a fine tail. */
    void brush (juce::Graphics& g, const juce::Path& spine, float width = 0.42f, juce::Colour colour = ink)
    {
        g.setColour (colour);
        g.fillPath (sweep (spine, [width] (float t)
        {
            const auto press = t < 0.12f ? 0.6f * std::sin (juce::MathConstants<float>::pi * t / 0.12f) : 0.0f;
            const auto lift = t > 0.65f ? juce::jmap (t, 0.65f, 1.0f, 1.0f, 0.2f) : 1.0f;
            return width * (1.0f + press) * lift;
        }));
    }

    /** A flat-washed shape outlined in overlapping brush strokes, so the contour breaks and
        swells as a hand-drawn one does. */
    void drape (juce::Graphics& g, const std::vector<P>& points, juce::Colour colour, float line = 0.42f, juce::Colour lineColour = ink)
    {
        fill (g, smooth (points, true), colour, 0.0f);
        for (int i = 0; i < (int) points.size(); i += 3)
            brush (g, smoothSpan (points, i, 4, true), line, lineColour);
    }

    struct Frame
    {
        P feet;
        float h, facing;

        P operator() (float x, float y) const { return { feet.x + x * h * facing, feet.y + y * h }; }
        P operator() (P p) const { return (*this) (p.x, p.y); }
        float len (float v) const { return v * h; }

        std::vector<P> map (const std::vector<P>& unit, P offset = {}) const
        {
            std::vector<P> out;
            for (auto p : unit)
                out.push_back ((*this) (p + offset));
            return out;
        }

        juce::Path line (const std::vector<P>& unit) const { return smooth (map (unit), false); }
    };

    juce::Rectangle<float> around (P centre, float w, float h)
    {
        return juce::Rectangle<float> (w, h).withCentre (centre);
    }

    /** A sash, ribbon or edging band: a narrowing strip of colour with a fine ink edge. */
    void paintBand (juce::Graphics& g, const Frame& f, const std::vector<P>& unit, juce::Colour colour, float width = 0.022f)
    {
        const auto band = taper (f.line (unit), f.len (width), f.len (width * 0.45f));
        fill (g, band, colour, 0.0f);
        g.setColour (ink.withAlpha (0.75f));
        g.strokePath (band, juce::PathStrokeType (0.3f));
    }

    void paintHand (juce::Graphics& g, const Frame& f, P centre)
    {
        drape (g, f.map ({ { -0.015f, -0.01f }, { 0.01f, -0.016f }, { 0.026f, -0.008f }, { 0.024f, 0.008f }, { 0.006f, 0.014f }, { -0.014f, 0.008f } }, centre),
               skin, 0.4f, faceInk);
    }

    void paintFeatures (juce::Graphics& g, const Frame& f)
    {
        softEllipse (g, f (0.026f, -0.847f), f.len (0.035f), f.len (0.02f), vermilion.withAlpha (0.14f));
        brush (g, f.line ({ { -0.022f, -0.878f }, { -0.033f, -0.866f }, { -0.03f, -0.851f }, { -0.02f, -0.847f } }), 0.28f, faceInk);
        brush (g, f.line ({ { 0.022f, -0.889f }, { 0.037f, -0.892f }, { 0.052f, -0.889f } }), 0.28f);
        brush (g, f.line ({ { 0.029f, -0.875f }, { 0.04f, -0.877f }, { 0.051f, -0.873f } }), 0.35f);
        g.setColour (vermilion.darker (0.2f));
        g.fillEllipse (around (f (0.041f, -0.832f), f.len (0.013f), f.len (0.007f)));
    }

    /** A court lady's head: a pale oval face turned three-quarters, hair swept up into a bun with a gold pin. */
    void paintLadyHead (juce::Graphics& g, const Frame& f)
    {
        drape (g, f.map ({ { -0.05f, -0.93f }, { -0.075f, -0.885f }, { -0.065f, -0.835f }, { -0.035f, -0.84f }, { -0.025f, -0.9f } }), hair, 0.35f);
        drape (g, f.map ({ { -0.035f, -0.915f }, { -0.04f, -0.87f }, { -0.02f, -0.826f }, { 0.012f, -0.808f }, { 0.035f, -0.815f },
                           { 0.046f, -0.832f }, { 0.051f, -0.85f }, { 0.06f, -0.864f }, { 0.054f, -0.875f }, { 0.055f, -0.9f },
                           { 0.045f, -0.918f }, { 0.0f, -0.925f } }), skin, 0.32f, faceInk);
        paintFeatures (g, f);

        drape (g, f.map ({ { 0.057f, -0.903f }, { 0.05f, -0.93f }, { 0.02f, -0.95f }, { -0.03f, -0.956f }, { -0.065f, -0.935f },
                           { -0.072f, -0.9f }, { -0.055f, -0.865f }, { -0.035f, -0.872f }, { -0.027f, -0.9f }, { 0.01f, -0.915f },
                           { 0.04f, -0.91f } }), hair, 0.35f);
        drape (g, f.map ({ { 0.012f, -0.948f }, { 0.032f, -0.976f }, { 0.012f, -1.0f }, { -0.028f, -0.997f }, { -0.05f, -0.972f }, { -0.04f, -0.946f } }),
               hair, 0.5f);
        brush (g, f.line ({ { -0.065f, -0.986f }, { -0.01f, -0.976f }, { 0.04f, -0.963f } }), 0.49f, gold);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.strokePath (f.line ({ { 0.045f, -0.92f }, { 0.0f, -0.94f }, { -0.05f, -0.925f } }), juce::PathStrokeType (0.4f));
    }

    /** A pitch-pot arrow: a long bare cane with a small iron head and a tuft of feathers. */
    void paintArrow (juce::Graphics& g, P tail, P head)
    {
        const auto dir = (head - tail) / juce::jmax (0.001f, tail.getDistanceFrom (head));
        const P normal { -dir.y, dir.x };

        g.setColour (cane);
        g.drawLine ({ tail, head }, 1.1f);
        g.setColour (ink.withAlpha (0.35f));
        g.drawLine ({ tail + normal * 0.5f, head + normal * 0.5f }, 0.35f);

        juce::Path tip;
        tip.addTriangle (head + dir * 2.6f, head + normal * 1.0f, head - normal * 1.0f);
        g.setColour (ink);
        g.fillPath (tip);

        for (auto side : { -1.0f, 1.0f })
        {
            juce::Path feather;
            feather.startNewSubPath (tail + dir * 0.5f);
            feather.quadraticTo (tail + normal * (1.2f * side) + dir * 1.5f, tail + normal * (1.0f * side) + dir * 5.0f);
            feather.lineTo (tail + dir * 6.0f);
            feather.closeSubPath();
            g.setColour (vermilion.darker (0.35f).withAlpha (0.85f));
            g.fillPath (feather);
        }
    }

    enum class Pose { clasped, tray, arrow };

    struct Dress
    {
        juce::Colour jacket, skirt, trim, sash;
    };

    /** A court lady in a short wide-sleeved jacket over a long skirt that pools on the ground,
        sash ribbons trailing behind her. */
    void paintLady (juce::Graphics& g, P feet, float h, float facing, Dress dress, Pose pose)
    {
        const Frame f { feet, h, facing };

        paintBand (g, f, { { -0.06f, -0.52f }, { -0.15f, -0.32f }, { -0.2f, -0.14f }, { -0.31f, 0.0f } }, dress.sash, 0.02f);

        drape (g, f.map ({ { -0.09f, -0.52f }, { -0.12f, -0.36f }, { -0.15f, -0.2f }, { -0.19f, -0.06f }, { -0.25f, -0.006f },
                           { -0.16f, 0.004f }, { -0.07f, -0.008f }, { 0.02f, 0.004f }, { 0.1f, -0.006f }, { 0.17f, -0.014f },
                           { 0.14f, -0.16f }, { 0.12f, -0.34f }, { 0.1f, -0.52f } }), dress.skirt);
        for (const auto& fold : std::vector<std::vector<P>> { { { 0.02f, -0.32f }, { 0.0f, -0.18f }, { -0.02f, -0.012f } },
                                                              { { 0.08f, -0.32f }, { 0.09f, -0.16f }, { 0.11f, -0.01f } },
                                                              { { -0.06f, -0.32f }, { -0.09f, -0.18f }, { -0.13f, -0.02f } },
                                                              { { 0.02f, 0.0f }, { 0.05f, -0.028f }, { 0.08f, -0.032f } },
                                                              { { -0.13f, -0.01f }, { -0.16f, -0.04f } } })
            brush (g, f.line (fold), 0.35f);

        drape (g, f.map ({ { -0.04f, -0.805f }, { -0.09f, -0.775f }, { -0.11f, -0.69f }, { -0.11f, -0.57f }, { -0.13f, -0.44f },
                           { -0.15f, -0.34f }, { -0.09f, -0.318f }, { -0.02f, -0.335f }, { 0.05f, -0.315f }, { 0.12f, -0.33f },
                           { 0.11f, -0.45f }, { 0.09f, -0.58f }, { 0.08f, -0.7f }, { 0.05f, -0.78f }, { 0.02f, -0.805f } }), dress.jacket);
        brush (g, f.line ({ { -0.07f, -0.7f }, { -0.09f, -0.56f }, { -0.1f, -0.42f } }), 0.35f);
        brush (g, f.line ({ { 0.065f, -0.52f }, { 0.075f, -0.43f }, { 0.085f, -0.34f } }), 0.35f);
        brush (g, f.line ({ { -0.06f, -0.4f }, { -0.07f, -0.36f }, { -0.1f, -0.33f } }), 0.35f);

        paintBand (g, f, { { -0.03f, -0.808f }, { 0.0f, -0.75f }, { 0.035f, -0.66f }, { 0.065f, -0.57f } }, dress.trim, 0.024f);
        paintBand (g, f, { { 0.035f, -0.805f }, { 0.03f, -0.77f }, { 0.005f, -0.745f } }, dress.trim, 0.02f);

        paintBand (g, f, { { -0.11f, -0.555f }, { -0.02f, -0.535f }, { 0.1f, -0.555f } }, dress.sash, 0.035f);
        paintBand (g, f, { { 0.04f, -0.54f }, { 0.065f, -0.36f }, { 0.045f, -0.19f }, { 0.08f, -0.04f } }, dress.sash, 0.02f);
        paintBand (g, f, { { 0.05f, -0.54f }, { 0.095f, -0.39f }, { 0.105f, -0.23f }, { 0.15f, -0.1f } }, dress.trim, 0.016f);

        std::vector<P> sleeve, cuff;
        P hand;
        switch (pose)
        {
            case Pose::clasped:
                sleeve = { { 0.0f, -0.78f }, { 0.07f, -0.75f }, { 0.13f, -0.66f }, { 0.17f, -0.57f }, { 0.19f, -0.5f }, { 0.17f, -0.43f },
                           { 0.12f, -0.38f }, { 0.06f, -0.4f }, { 0.01f, -0.47f }, { -0.03f, -0.58f }, { -0.05f, -0.7f } };
                cuff = { { 0.165f, -0.58f }, { 0.195f, -0.51f }, { 0.175f, -0.43f } };
                break;
            case Pose::tray:
                sleeve = { { 0.0f, -0.78f }, { 0.08f, -0.75f }, { 0.15f, -0.67f }, { 0.21f, -0.615f }, { 0.24f, -0.6f }, { 0.24f, -0.54f },
                           { 0.18f, -0.5f }, { 0.11f, -0.44f }, { 0.04f, -0.45f }, { -0.01f, -0.54f }, { -0.04f, -0.66f } };
                cuff = { { 0.228f, -0.615f }, { 0.25f, -0.575f }, { 0.236f, -0.53f } };
                hand = { 0.265f, -0.578f };
                break;
            case Pose::arrow:
                sleeve = { { 0.0f, -0.78f }, { 0.07f, -0.74f }, { 0.12f, -0.67f }, { 0.16f, -0.625f }, { 0.18f, -0.6f }, { 0.17f, -0.54f },
                           { 0.12f, -0.5f }, { 0.07f, -0.44f }, { 0.02f, -0.46f }, { -0.02f, -0.56f }, { -0.04f, -0.68f } };
                cuff = { { 0.163f, -0.635f }, { 0.188f, -0.6f }, { 0.172f, -0.555f } };
                hand = { 0.198f, -0.6f };
                break;
        }

        if (pose == Pose::arrow)
            paintArrow (g, f (0.21f, -1.28f), f (0.19f, -0.06f));

        drape (g, f.map (sleeve), dress.jacket.brighter (0.06f));
        brush (g, f.line ({ { 0.03f, -0.74f }, { 0.08f, -0.64f }, { 0.11f, -0.55f } }), 0.35f);
        brush (g, f.line ({ { 0.03f, -0.52f }, { 0.07f, -0.47f }, { 0.12f, -0.46f } }), 0.35f);
        paintBand (g, f, cuff, dress.trim, 0.028f);

        if (pose == Pose::tray)
        {
            for (auto x : { 0.24f, 0.31f, 0.38f })
                drape (g, f.map ({ { -0.022f, -0.012f }, { 0.022f, -0.012f }, { 0.015f, 0.014f }, { -0.015f, 0.014f } }, { x, -0.61f }), gold, 0.28f);
            fill (g, juce::Rectangle<float> (f (0.15f, -0.6f), f (0.46f, -0.575f)), lacquer, 0.6f);
            g.setColour (gold);
            g.drawLine ({ f (0.16f, -0.59f), f (0.45f, -0.59f) }, 0.6f);
        }
        else if (pose == Pose::arrow)
        {
            paintHand (g, f, hand);
        }

        if (pose == Pose::tray)
            paintHand (g, f, hand);

        paintLadyHead (g, f);
    }

    /** The host, seated on a lacquer chair with his boots on a footstool, taking aim. */
    void paintHost (juce::Graphics& g, P feet, float h, float facing)
    {
        const Frame f { feet, h, facing };

        brush (g, f.line ({ { -0.17f, 0.0f }, { -0.18f, -0.5f }, { -0.19f, -0.93f } }), f.len (0.026f), lacquer);
        brush (g, f.line ({ { -0.31f, -0.97f }, { -0.25f, -0.935f }, { -0.15f, -0.94f }, { -0.09f, -0.975f } }), f.len (0.024f), lacquer);
        brush (g, f.line ({ { 0.16f, -0.36f }, { 0.17f, -0.18f }, { 0.155f, 0.0f } }), f.len (0.022f), lacquer);
        fill (g, juce::Rectangle<float> (f (0.06f, -0.055f), f (0.44f, 0.0f)), lacquer, 0.6f);
        g.setColour (gold.withAlpha (0.8f));
        g.drawLine ({ f (0.07f, -0.04f), f (0.43f, -0.04f) }, 0.5f);

        for (auto x : { 0.0f, 0.075f })
            drape (g, f.map ({ { 0.2f, -0.085f }, { 0.29f, -0.092f }, { 0.335f, -0.072f }, { 0.31f, -0.058f }, { 0.2f, -0.06f } }, { x, x * 0.1f }),
                   hatBlack, 0.5f);

        const std::vector<P> robe { { -0.06f, -0.805f }, { -0.11f, -0.78f }, { -0.14f, -0.68f }, { -0.145f, -0.55f }, { -0.15f, -0.45f },
                                    { -0.18f, -0.32f }, { -0.21f, -0.18f }, { -0.245f, -0.06f }, { -0.2f, -0.042f }, { -0.12f, -0.05f },
                                    { -0.04f, -0.04f }, { 0.05f, -0.055f }, { 0.13f, -0.05f }, { 0.22f, -0.07f }, { 0.25f, -0.16f },
                                    { 0.27f, -0.28f }, { 0.28f, -0.38f }, { 0.25f, -0.44f }, { 0.17f, -0.46f }, { 0.11f, -0.5f },
                                    { 0.11f, -0.6f }, { 0.09f, -0.72f }, { 0.04f, -0.8f } };
        drape (g, f.map (robe), lilac);
        {
            // Woven roundels in a deeper shade of the silk.
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (smooth (f.map (robe), true));
            g.setColour (lilac.darker (0.3f).withAlpha (0.7f));
            for (int row = 0; row < 10; ++row)
                for (int col = 0; col < 7; ++col)
                {
                    const auto c = f (-0.27f + (float) col * 0.09f + (float) (row % 2) * 0.045f, -0.8f + (float) row * 0.085f);
                    const auto r = f.len (0.02f);
                    g.drawEllipse (around (c, r * 2.0f, r * 2.0f), 0.45f);
                    g.fillEllipse (around (c, r * 0.6f, r * 0.6f));
                }
        }
        for (const auto& fold : std::vector<std::vector<P>> { { { -0.05f, -0.45f }, { 0.08f, -0.42f }, { 0.22f, -0.405f } },
                                                              { { 0.0f, -0.4f }, { 0.12f, -0.36f }, { 0.24f, -0.35f } },
                                                              { { 0.2f, -0.35f }, { 0.18f, -0.2f }, { 0.2f, -0.08f } },
                                                              { { 0.1f, -0.38f }, { 0.07f, -0.22f }, { 0.1f, -0.06f } },
                                                              { { -0.1f, -0.42f }, { -0.14f, -0.25f }, { -0.17f, -0.06f } },
                                                              { { -0.03f, -0.38f }, { -0.06f, -0.2f }, { -0.05f, -0.05f } },
                                                              { { -0.08f, -0.75f }, { -0.11f, -0.62f }, { -0.1f, -0.5f } } })
            brush (g, f.line (fold), 0.39f);

        // Rank badge and jade belt.
        fill (g, around (f (-0.015f, -0.64f), f.len (0.09f), f.len (0.085f)), gold, 0.5f);
        g.setColour (azurite);
        g.fillEllipse (around (f (-0.015f, -0.64f), f.len (0.05f), f.len (0.05f)));
        brush (g, f.line ({ { -0.035f, -0.63f }, { -0.01f, -0.655f }, { 0.01f, -0.65f } }), 0.35f, juce::Colours::white);
        brush (g, f.line ({ { -0.14f, -0.52f }, { -0.03f, -0.49f }, { 0.1f, -0.505f } }), f.len (0.022f), hatBlack);
        g.setColour (gold);
        for (auto x : { -0.1f, -0.04f, 0.03f })
            g.fillRoundedRectangle (around (f (x, -0.5f), f.len (0.03f), f.len (0.016f)), 0.6f);

        // The other arm rests along his lap.
        drape (g, f.map ({ { 0.06f, -0.62f }, { 0.14f, -0.56f }, { 0.22f, -0.51f }, { 0.275f, -0.48f }, { 0.265f, -0.425f },
                           { 0.18f, -0.415f }, { 0.1f, -0.44f }, { 0.04f, -0.5f } }), lilac.brighter (0.05f));
        brush (g, f.line ({ { 0.1f, -0.52f }, { 0.17f, -0.47f }, { 0.23f, -0.46f } }), 0.35f);
        paintHand (g, f, { 0.285f, -0.445f });

        // The throwing arm, its wide sleeve hanging beneath.
        drape (g, f.map ({ { 0.0f, -0.775f }, { 0.1f, -0.785f }, { 0.2f, -0.77f }, { 0.28f, -0.745f }, { 0.31f, -0.72f },
                           { 0.3f, -0.68f }, { 0.25f, -0.66f }, { 0.2f, -0.6f }, { 0.13f, -0.56f }, { 0.07f, -0.6f },
                           { 0.02f, -0.67f }, { -0.03f, -0.73f } }), lilac.brighter (0.05f));
        brush (g, f.line ({ { 0.05f, -0.76f }, { 0.15f, -0.74f }, { 0.24f, -0.72f } }), 0.35f);
        brush (g, f.line ({ { 0.08f, -0.66f }, { 0.15f, -0.62f }, { 0.2f, -0.625f } }), 0.35f);
        drape (g, f.map ({ { 0.29f, -0.742f }, { 0.315f, -0.72f }, { 0.305f, -0.685f }, { 0.288f, -0.692f } }), lilac.darker (0.45f), 0.28f);
        paintArrow (g, f (0.2f, -0.65f), f (0.7f, -0.86f));
        paintHand (g, f, { 0.335f, -0.708f });

        // Head: winged gauze hat over a bearded face.
        drape (g, f.map ({ { -0.05f, -0.945f }, { -0.13f, -0.957f }, { -0.2f, -0.966f }, { -0.215f, -0.95f }, { -0.2f, -0.936f },
                           { -0.13f, -0.937f }, { -0.05f, -0.928f } }), hatBlack, 0.28f);
        drape (g, f.map ({ { 0.04f, -0.948f }, { 0.1f, -0.958f }, { 0.15f, -0.966f }, { 0.163f, -0.952f }, { 0.15f, -0.94f },
                           { 0.1f, -0.94f }, { 0.04f, -0.935f } }), hatBlack, 0.28f);
        drape (g, f.map ({ { -0.045f, -0.905f }, { -0.05f, -0.86f }, { -0.025f, -0.82f }, { 0.015f, -0.802f }, { 0.045f, -0.81f },
                           { 0.056f, -0.83f }, { 0.06f, -0.845f }, { 0.074f, -0.862f }, { 0.066f, -0.874f }, { 0.068f, -0.9f },
                           { 0.06f, -0.915f }, { 0.0f, -0.92f } }), skin, 0.32f, faceInk);
        paintFeatures (g, f);
        brush (g, f.line ({ { 0.045f, -0.842f }, { 0.062f, -0.836f }, { 0.077f, -0.825f } }), 0.35f);
        for (auto dx : { 0.0f, 0.012f, 0.024f })
            brush (g, f.line ({ { 0.028f + dx, -0.815f }, { 0.034f + dx, -0.77f }, { 0.026f + dx, -0.72f } }), 0.39f);
        drape (g, f.map ({ { 0.068f, -0.905f }, { 0.074f, -0.935f }, { 0.058f, -0.955f }, { 0.03f, -0.96f }, { 0.022f, -0.982f },
                           { -0.015f, -1.0f }, { -0.055f, -0.99f }, { -0.072f, -0.955f }, { -0.066f, -0.91f }, { 0.0f, -0.9f } }), hatBlack, 0.35f);
    }

    //==============================================================================
    /** The bronze pot on its lacquer stand: a round belly, a long neck with two tubular ears and
        a flared mouth, with the arrows that have already gone in. */
    void paintPot (juce::Graphics& g, P base, float h)
    {
        const auto x = base.x;
        const auto mouthY = base.y - h;

        fill (g, juce::Rectangle<float> (x - h * 0.36f, base.y - h * 0.02f, h * 0.72f, h * 0.06f), lacquer, 0.8f);
        for (auto legX : { x - h * 0.33f, x + h * 0.27f })
            fill (g, juce::Rectangle<float> (legX, base.y + h * 0.04f, h * 0.06f, h * 0.06f), lacquer, 0.7f);
        g.setColour (gold.withAlpha (0.8f));
        g.drawLine (x - h * 0.33f, base.y + h * 0.01f, x + h * 0.33f, base.y + h * 0.01f, 0.8f);

        const float tilts[] = { -0.4f, -0.24f, -0.08f, 0.08f, 0.22f, 0.38f };
        for (int i = 0; i < 6; ++i)
        {
            const auto len = h * (0.9f + 0.05f * (float) (i % 3));
            paintArrow (g, { x + std::sin (tilts[i]) * len, mouthY + h * 0.12f - std::cos (tilts[i]) * len },
                           { x + ((float) i - 2.5f) * 1.2f, mouthY + h * 0.14f });
        }

        juce::Path pot;
        pot.startNewSubPath (x - h * 0.14f, base.y - h * 0.02f);
        pot.lineTo (x - h * 0.16f, base.y - h * 0.07f);
        pot.cubicTo (x - h * 0.38f, base.y - h * 0.16f, x - h * 0.36f, base.y - h * 0.44f, x - h * 0.1f, base.y - h * 0.52f);
        pot.quadraticTo (x - h * 0.075f, base.y - h * 0.56f, x - h * 0.075f, base.y - h * 0.62f);
        pot.lineTo (x - h * 0.075f, mouthY + h * 0.1f);
        pot.quadraticTo (x - h * 0.08f, mouthY + h * 0.04f, x - h * 0.13f, mouthY);
        pot.lineTo (x + h * 0.13f, mouthY);
        pot.quadraticTo (x + h * 0.08f, mouthY + h * 0.04f, x + h * 0.075f, mouthY + h * 0.1f);
        pot.lineTo (x + h * 0.075f, base.y - h * 0.62f);
        pot.quadraticTo (x + h * 0.075f, base.y - h * 0.56f, x + h * 0.1f, base.y - h * 0.52f);
        pot.cubicTo (x + h * 0.36f, base.y - h * 0.44f, x + h * 0.38f, base.y - h * 0.16f, x + h * 0.16f, base.y - h * 0.07f);
        pot.lineTo (x + h * 0.14f, base.y - h * 0.02f);
        pot.closeSubPath();

        fill (g, pot, bronze, 0.0f);
        {
            // Decoration and patina, kept inside the bronze.
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (pot);

            const auto bandY = base.y - h * 0.34f;
            g.setColour (gold.darker (0.5f).withAlpha (0.55f));
            g.fillRect (x - h * 0.4f, bandY, h * 0.8f, h * 0.09f);
            juce::Path meander;
            meander.startNewSubPath (x - h * 0.4f, bandY + h * 0.07f);
            for (auto mx = x - h * 0.4f; mx < x + h * 0.4f; mx += h * 0.06f)
            {
                meander.lineTo (mx, bandY + h * 0.02f);
                meander.lineTo (mx + h * 0.03f, bandY + h * 0.02f);
                meander.lineTo (mx + h * 0.03f, bandY + h * 0.07f);
                meander.lineTo (mx + h * 0.06f, bandY + h * 0.07f);
            }
            g.setColour (gold.brighter (0.3f).withAlpha (0.8f));
            g.strokePath (meander, juce::PathStrokeType (0.8f));

            g.setColour (ink.withAlpha (0.4f));
            for (auto ringY : { base.y - h * 0.5f, base.y - h * 0.12f, mouthY + h * 0.12f, mouthY + h * 0.34f })
                g.drawLine (x - h * 0.4f, ringY, x + h * 0.4f, ringY, 0.8f);

            juce::Random random (0x2b07);
            for (int i = 0; i < 9; ++i)
                softEllipse (g, { x + (random.nextFloat() - 0.5f) * h * 0.6f, base.y - h * (0.1f + random.nextFloat() * 0.5f) },
                             h * 0.12f, h * 0.08f, malachite.darker (0.3f).withAlpha (0.45f));

        }
        stroke (g, pot, 1.0f, 0.85f);

        for (auto side : { -1.0f, 1.0f })
        {
            juce::Path ear;
            ear.addRoundedRectangle (around ({ x + side * h * 0.115f, mouthY + h * 0.24f }, h * 0.07f, h * 0.16f), h * 0.03f);
            fill (g, ear, gold.darker (0.2f), 0.8f);
            g.setColour (ink.withAlpha (0.8f));
            g.fillEllipse (around ({ x + side * h * 0.115f, mouthY + h * 0.17f }, h * 0.045f, h * 0.02f));
        }

        juce::Path rim;
        rim.addEllipse (around ({ x, mouthY }, h * 0.27f, h * 0.035f));
        fill (g, rim, gold.darker (0.3f), 0.8f);
    }
}

//==============================================================================
void CourtPainting::paint (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.fillAll (juce::Colour (0xff141011));
    const auto box = bounds.reduced (6.0f);
    paintMount (g, box);

    const auto face = box.reduced (8.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (face.toNearestInt());

        paintSilk (g, face);
        paintHills (g, face);

        paintPavilion (g, face.getX());
        paintPine (g);

        for (const auto& [x, y, size] : { std::tuple { 20.0f, 290.0f, 9.0f }, { 120.0f, 300.0f, 10.0f }, { 152.0f, 306.0f, 7.0f },
                                         { 318.0f, 304.0f, 8.0f }, { 350.0f, 302.0f, 10.0f }, { 440.0f, 302.0f, 9.0f },
                                         { 352.0f, 226.0f, 8.0f }, { 438.0f, 228.0f, 9.0f } })
            paintGrass (g, { x, y }, size);

        paintLady (g, { 34.0f, 284.0f }, 54.0f, 1.0f, { mist, skirtRed, sage, blush }, Pose::tray);
        paintHost (g, { 78.0f, 296.0f }, 76.0f, 1.0f);

        paintPot (g, { 396.0f, 222.0f }, 72.0f);
        paintArrow (g, { 334.0f, 66.0f }, { 356.0f, 78.0f });
        for (const auto& [tail, head] : { std::pair { P (332, 230), P (378, 222) }, { P (444, 238), P (364, 232) },
                                           { P (346, 240), P (402, 236) }, { P (440, 218), P (410, 240) } })
            paintArrow (g, tail, head);

        paintLady (g, { 372.0f, 302.0f }, 54.0f, -1.0f, { sage, ivory, skirtRed, plum }, Pose::arrow);
        paintLady (g, { 420.0f, 298.0f }, 52.0f, -1.0f, { blush, mist, sage, skirtRed }, Pose::clasped);

        paintWear (g, face);
    }

    g.setColour (gold.withAlpha (0.8f));
    g.drawRect (face, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (box, 14.0f, 2.0f);
}

void CourtPainting::paintSeal (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xffb5322a).withAlpha (0.88f));
    g.fillRoundedRectangle (r, 2.0f);

    // Carved in relief: the characters and border show the silk through the seal paste.
    const auto silk = Palette::enclosure;
    g.setColour (silk.withAlpha (0.92f));
    g.drawRect (r.reduced (2.5f), 1.2f);
    g.setFont (juce::FontOptions ("Songti SC", r.getHeight() * 0.42f, juce::Font::bold)
                   .withFallbacks ({ "STSong", "SimSun", "PingFang SC", "Microsoft YaHei" }));
    auto text = r.reduced (3.0f);
    g.drawText (juce::String::fromUTF8 ("\xe6\x8a\x95"), text.removeFromTop (text.getHeight() * 0.5f), juce::Justification::centred);
    g.drawText (juce::String::fromUTF8 ("\xe5\xa3\xba"), text, juce::Justification::centred);

    juce::Random random (0x5ea1);
    g.setColour (silk.withAlpha (0.55f));
    for (int i = 0; i < 26; ++i)
    {
        const auto s = 0.6f + random.nextFloat() * 1.4f;
        g.fillEllipse (r.getX() + random.nextFloat() * r.getWidth(), r.getY() + random.nextFloat() * r.getHeight(), s, s);
    }
}
