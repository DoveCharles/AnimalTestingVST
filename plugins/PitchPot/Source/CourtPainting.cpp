#include "CourtPainting.h"
#include "PedalLookAndFeel.h"

namespace
{
    using Palette = PedalLookAndFeel::Palette;
    using P = juce::Point<float>;

    const juce::Colour ink       { 0xff2b1d14 };
    const juce::Colour vermilion { 0xffb3402f };
    const juce::Colour lacquer   { 0xff7a2a1e };
    const juce::Colour malachite { 0xff4f8a68 };
    const juce::Colour azurite   { 0xff3d6391 };
    const juce::Colour lilac     { 0xff857892 };
    const juce::Colour skin      { 0xffeccfa6 };
    const juce::Colour gold      { 0xffc9a24a };
    const juce::Colour slate     { 0xff4b5552 };
    const juce::Colour marble    { 0xffd8cfb9 };
    const juce::Colour pine      { 0xff2f5e45 };
    const juce::Colour bark      { 0xff6b4a32 };
    const juce::Colour earth     { 0xff8a6a3e };

    //==============================================================================
    void stroke (juce::Graphics& g, const juce::Path& path, float width = 0.9f, float alpha = 0.75f)
    {
        g.setColour (ink.withAlpha (alpha));
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Flat colour, a little lighter at `from` than at `to`, then an ink outline (none if `outline` is 0). */
    void fill (juce::Graphics& g, const juce::Path& path, juce::Colour colour, P from, P to, float outline = 0.9f)
    {
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.1f), from, colour.darker (0.18f), to, false));
        g.fillPath (path);
        if (outline > 0.0f)
            stroke (g, path, outline);
    }

    void fill (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float outline = 0.9f)
    {
        const auto b = path.getBounds();
        fill (g, path, colour, { b.getCentreX(), b.getY() }, { b.getCentreX(), b.getBottom() + 0.01f }, outline);
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

    /** A stroke that thins from `startWidth` to `endWidth` along `spine`, for branches. */
    juce::Path taper (const juce::Path& spine, float startWidth, float endWidth)
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
            const auto n = P (-d.y, d.x) * (juce::jmap (t, startWidth, endWidth) * 0.5f);
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

    void softEllipse (juce::Graphics& g, P centre, float width, float height, juce::Colour colour)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::scale (1.0f, height / width, centre.x, centre.y));
        g.setGradientFill (juce::ColourGradient (colour, centre, colour.withAlpha (0.0f), centre.translated (width * 0.5f, 0.0f), true));
        g.fillEllipse (juce::Rectangle<float> (width, width).withCentre (centre));
    }

    void groundShadow (juce::Graphics& g, P feet, float width)
    {
        softEllipse (g, feet, width, width * 0.22f, juce::Colour (0xff5a4026).withAlpha (0.28f));
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
        limb.startNewSubPath (468.0f, 2.0f);
        limb.cubicTo (436.0f, 30.0f, 404.0f, 22.0f, 376.0f, 42.0f);
        limb.quadraticTo (362.0f, 52.0f, 344.0f, 50.0f);

        juce::Path twig;
        twig.startNewSubPath (408.0f, 28.0f);
        twig.quadraticTo (412.0f, 44.0f, 402.0f, 56.0f);

        for (const auto& [spine, w0, w1] : { std::tuple { limb, 11.0f, 2.5f }, std::tuple { twig, 4.0f, 1.5f } })
        {
            const auto body = taper (spine, w0, w1);
            fill (g, body, bark, 0.9f);
        }

        // Scaly bark.
        juce::Random random (0x9172);
        for (int i = 0; i < 14; ++i)
        {
            const auto p = limb.getPointAlongPath (limb.getLength() * (0.05f + 0.6f * random.nextFloat()));
            stroke (g, curve (p.translated (-2.0f, -1.5f), p.translated (0.0f, 1.0f), p.translated (2.5f, -1.0f)), 0.7f, 0.45f);
        }

        // Needles in wheel-shaped clusters, each over a soft green wash.
        for (const auto c : { P (446, 16), P (426, 30), P (406, 18), P (398, 44), P (372, 38), P (352, 52), P (420, 50), P (382, 60) })
        {
            softEllipse (g, c, 30.0f, 16.0f, pine.withAlpha (0.35f));
            g.setColour (pine.withAlpha (0.9f));
            for (int i = 0; i < 17; ++i)
            {
                const auto a = juce::degreesToRadians (-170.0f + (float) i * 10.0f);
                const auto r = 9.0f + random.nextFloat() * 3.0f;
                g.drawLine ({ c, c + P (std::cos (a), std::sin (a) * 0.7f) * r }, 0.75f);
            }
            g.setColour (bark.darker (0.4f));
            g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (c));
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
            fill (g, rect ({ x, 56.0f, 8.0f, 134.0f }), vermilion, { x, 120.0f }, { x + 8.0f, 120.0f });
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
    // y = -1, facing +x, so one set of shapes serves for either direction and any size.

    struct Frame
    {
        P feet;
        float h, facing;

        P operator() (float x, float y) const { return { feet.x + x * h * facing, feet.y + y * h }; }
        float len (float v) const { return v * h; }
    };

    juce::Rectangle<float> around (P centre, float w, float h)
    {
        return juce::Rectangle<float> (w, h).withCentre (centre);
    }

    enum class Hat { futou, winged };

    void paintHead (juce::Graphics& g, const Frame& f, float cx, float cy, Hat hat, bool beard)
    {
        juce::Path face;
        face.addEllipse (around (f (cx, cy), f.len (0.125f), f.len (0.15f)));
        fill (g, face, skin, 0.8f);

        juce::Path ear;
        ear.addEllipse (around (f (cx - 0.035f, cy + 0.005f), f.len (0.03f), f.len (0.05f)));
        fill (g, ear, skin.darker (0.05f), 0.6f);

        g.setColour (ink);
        g.drawLine ({ f (cx + 0.02f, cy - 0.008f), f (cx + 0.045f, cy - 0.01f) }, 1.0f);
        g.setColour (ink.withAlpha (0.6f));
        g.drawLine ({ f (cx + 0.012f, cy - 0.032f), f (cx + 0.05f, cy - 0.036f) }, 0.8f);
        g.setColour (vermilion.withAlpha (0.7f));
        g.drawLine ({ f (cx + 0.028f, cy + 0.04f), f (cx + 0.045f, cy + 0.039f) }, 0.9f);

        if (beard)
        {
            stroke (g, curve (f (cx + 0.02f, cy + 0.03f), f (cx + 0.05f, cy + 0.035f), f (cx + 0.065f, cy + 0.05f)), 0.9f, 0.85f);
            stroke (g, curve (f (cx + 0.035f, cy + 0.06f), f (cx + 0.045f, cy + 0.11f), f (cx + 0.02f, cy + 0.15f)), 1.6f, 0.85f);
            stroke (g, curve (f (cx + 0.015f, cy + 0.06f), f (cx + 0.02f, cy + 0.1f), f (cx + 0.0f, cy + 0.13f)), 1.1f, 0.7f);
        }

        // Black gauze hat: a rounded crown with a raised back, and either the soft hanging tails
        // of an attendant's futou or the stiff side wings of an official.
        g.setColour (ink.brighter (0.1f));
        g.fillEllipse (around (f (cx - 0.005f, cy - 0.058f), f.len (0.145f), f.len (0.075f)));
        g.fillEllipse (around (f (cx - 0.02f, cy - 0.095f), f.len (0.1f), f.len (0.07f)));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (around (f (cx + 0.01f, cy - 0.105f), f.len (0.04f), f.len (0.025f)));

        if (hat == Hat::winged)
        {
            g.setColour (ink.brighter (0.1f));
            for (auto side : { -1.0f, 1.0f })
                g.fillRoundedRectangle (around (f (cx - 0.01f + side * 0.15f, cy - 0.07f), f.len (0.17f), f.len (0.032f)), f.len (0.014f));
        }
        else
        {
            stroke (g, curve (f (cx - 0.06f, cy - 0.06f), f (cx - 0.11f, cy - 0.02f), f (cx - 0.1f, cy + 0.08f)), 1.5f, 0.9f);
            stroke (g, curve (f (cx - 0.065f, cy - 0.05f), f (cx - 0.125f, cy - 0.0f), f (cx - 0.12f, cy + 0.07f)), 1.2f, 0.8f);
        }
    }

    void paintBelt (juce::Graphics& g, const Frame& f, float y, float halfWidth)
    {
        const auto belt = curve (f (-halfWidth, y - 0.01f), f (0.02f, y + 0.04f), f (halfWidth + 0.01f, y - 0.015f));
        g.setColour (ink.brighter (0.15f));
        g.strokePath (belt, juce::PathStrokeType (f.len (0.022f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (gold);
        for (auto t : { 0.3f, 0.55f, 0.8f })
            g.fillEllipse (around (belt.getPointAlongPath (belt.getLength() * t), f.len (0.03f), f.len (0.018f)));
    }

    void paintCollar (juce::Graphics& g, const Frame& f, float y, juce::Colour robe)
    {
        juce::Path collar;
        collar.addEllipse (around (f (0.01f, y), f.len (0.13f), f.len (0.05f)));
        fill (g, collar, robe.darker (0.15f), 0.8f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (around (f (0.025f, y - 0.004f), f.len (0.06f), f.len (0.022f)));
    }

    void paintArrow (juce::Graphics& g, P tail, P head)
    {
        const auto dir = (head - tail) / juce::jmax (0.001f, tail.getDistanceFrom (head));
        const P normal { -dir.y, dir.x };

        g.setColour (juce::Colour (0xff7a5a34));
        g.drawLine ({ tail, head }, 1.1f);
        juce::Path tip;
        tip.addTriangle (head + dir * 3.5f, head + normal * 1.4f, head - normal * 1.4f);
        g.setColour (ink);
        g.fillPath (tip);

        for (auto side : { -1.0f, 1.0f })
        {
            juce::Path feather;
            feather.startNewSubPath (tail + dir * 0.5f);
            feather.quadraticTo (tail + normal * (3.4f * side) + dir * 2.0f, tail + normal * (2.6f * side) + dir * 8.0f);
            feather.lineTo (tail + dir * 10.0f);
            feather.closeSubPath();
            g.setColour (vermilion);
            g.fillPath (feather);
            g.setColour (ink.withAlpha (0.5f));
            g.strokePath (feather, juce::PathStrokeType (0.5f));
        }
    }

    enum class Pose { clasped, tray, arrows };

    /** A standing attendant in a long round-collared robe with flared side pleats. */
    void paintAttendant (juce::Graphics& g, P feet, float h, float facing, juce::Colour robe, Pose pose)
    {
        const Frame f { feet, h, facing };
        groundShadow (g, feet.translated (0.0f, -1.0f), h * 0.6f);

        g.setColour (ink);
        g.fillEllipse (around (f (-0.04f, -0.012f), f.len (0.1f), f.len (0.04f)));
        g.fillEllipse (around (f (0.11f, -0.008f), f.len (0.1f), f.len (0.04f)));

        juce::Path pleat;
        pleat.startNewSubPath (f (-0.12f, -0.47f));
        pleat.quadraticTo (f (-0.2f, -0.3f), f (-0.25f, -0.03f));
        pleat.lineTo (f (-0.15f, -0.03f));
        pleat.closeSubPath();
        fill (g, pleat, robe.darker (0.25f));

        juce::Path body;
        body.startNewSubPath (f (-0.1f, -0.79f));
        body.quadraticTo (f (-0.15f, -0.62f), f (-0.14f, -0.44f));
        body.quadraticTo (f (-0.17f, -0.22f), f (-0.19f, -0.02f));
        body.quadraticTo (f (0.0f, 0.012f), f (0.2f, -0.02f));
        body.quadraticTo (f (0.17f, -0.25f), f (0.15f, -0.46f));
        body.quadraticTo (f (0.15f, -0.64f), f (0.09f, -0.79f));
        body.quadraticTo (f (0.0f, -0.83f), f (-0.1f, -0.79f));
        body.closeSubPath();
        fill (g, body, robe, f (0.0f, -0.8f), f (0.0f, 0.0f));

        stroke (g, curve (f (0.03f, -0.44f), f (0.0f, -0.22f), f (-0.03f, -0.03f)), 0.8f, 0.4f);
        stroke (g, curve (f (0.1f, -0.44f), f (0.11f, -0.22f), f (0.14f, -0.03f)), 0.8f, 0.4f);
        stroke (g, curve (f (-0.07f, -0.43f), f (-0.1f, -0.22f), f (-0.11f, -0.03f)), 0.8f, 0.4f);
        stroke (g, curve (f (-0.06f, -0.74f), f (-0.1f, -0.6f), f (-0.08f, -0.5f)), 0.7f, 0.3f);

        paintBelt (g, f, -0.47f, 0.15f);
        paintCollar (g, f, -0.79f, robe);

        if (pose == Pose::arrows)
            for (int i = 0; i < 5; ++i)
            {
                const auto spread = (float) (i - 2);
                paintArrow (g, f (0.19f + spread * 0.022f, -1.1f + std::abs (spread) * 0.02f), f (0.2f + spread * 0.006f, -0.2f));
            }

        juce::Path sleeve;
        sleeve.startNewSubPath (f (0.0f, -0.77f));
        P hand;
        switch (pose)
        {
            case Pose::clasped:
                sleeve.quadraticTo (f (0.15f, -0.72f), f (0.2f, -0.58f));
                sleeve.lineTo (f (0.22f, -0.46f));
                sleeve.quadraticTo (f (0.16f, -0.34f), f (0.06f, -0.4f));
                break;
            case Pose::tray:
                sleeve.quadraticTo (f (0.14f, -0.72f), f (0.22f, -0.61f));
                sleeve.lineTo (f (0.25f, -0.53f));
                sleeve.quadraticTo (f (0.18f, -0.36f), f (0.06f, -0.42f));
                hand = f (0.27f, -0.57f);
                break;
            case Pose::arrows:
                sleeve.quadraticTo (f (0.13f, -0.72f), f (0.17f, -0.61f));
                sleeve.lineTo (f (0.2f, -0.52f));
                sleeve.quadraticTo (f (0.14f, -0.36f), f (0.05f, -0.42f));
                hand = f (0.2f, -0.57f);
                break;
        }
        sleeve.quadraticTo (f (-0.01f, -0.56f), f (0.0f, -0.77f));
        sleeve.closeSubPath();
        fill (g, sleeve, robe.brighter (0.08f));
        stroke (g, curve (f (0.04f, -0.7f), f (0.1f, -0.56f), f (0.1f, -0.45f)), 0.7f, 0.35f);

        if (pose == Pose::clasped)
        {
            g.setColour (ink.withAlpha (0.55f));
            g.fillEllipse (around (f (0.21f, -0.52f), f.len (0.03f), f.len (0.09f)));
        }
        else
        {
            juce::Path fist;
            fist.addEllipse (around (hand, f.len (0.055f), f.len (0.05f)));
            fill (g, fist, skin, 0.6f);
        }

        if (pose == Pose::tray)
        {
            // A red lacquer tray of gold wine cups.
            for (auto x : { 0.22f, 0.3f, 0.38f })
            {
                juce::Path cup;
                cup.addEllipse (around (f (x, -0.615f), f.len (0.055f), f.len (0.045f)));
                fill (g, cup, gold, 0.6f);
            }
            juce::Path tray;
            tray.addRoundedRectangle (around (f (0.3f, -0.585f), f.len (0.3f), f.len (0.035f)), f.len (0.01f));
            fill (g, tray, lacquer, 0.7f);
            g.setColour (gold);
            g.drawLine ({ f (0.16f, -0.598f), f (0.44f, -0.598f) }, 0.8f);
        }

        paintHead (g, f, 0.03f, -0.875f, Hat::futou, false);
    }

    /** The host, seated on a lacquer chair with his boots on a footstool, taking aim. */
    void paintHost (juce::Graphics& g, P feet, float h, float facing)
    {
        const Frame f { feet, h, facing };
        groundShadow (g, f (0.06f, 0.0f), h * 0.8f);

        g.setColour (lacquer);
        g.strokePath (curve (f (-0.2f, 0.0f), f (-0.21f, -0.5f), f (-0.19f, -0.96f)), juce::PathStrokeType (f.len (0.035f)));
        g.strokePath (curve (f (-0.29f, -0.93f), f (-0.21f, -1.02f), f (-0.1f, -0.96f)), juce::PathStrokeType (f.len (0.03f)));
        g.strokePath (curve (f (0.18f, -0.38f), f (0.19f, -0.2f), f (0.17f, 0.0f)), juce::PathStrokeType (f.len (0.03f)));
        g.setColour (gold.withAlpha (0.8f));
        g.fillEllipse (around (f (-0.29f, -0.93f), f.len (0.035f), f.len (0.035f)));
        fill (g, juce::Rectangle<float> (f (0.06f, -0.05f), f (0.42f, 0.0f)), lacquer, 0.7f);

        g.setColour (ink);
        g.fillEllipse (around (f (0.25f, -0.065f), f.len (0.11f), f.len (0.04f)));
        g.fillEllipse (around (f (0.35f, -0.06f), f.len (0.11f), f.len (0.04f)));

        juce::Path body;
        body.startNewSubPath (f (-0.12f, -0.8f));
        body.quadraticTo (f (-0.19f, -0.62f), f (-0.18f, -0.42f));
        body.quadraticTo (f (-0.24f, -0.2f), f (-0.27f, -0.03f));
        body.quadraticTo (f (0.0f, 0.0f), f (0.32f, -0.075f));
        body.quadraticTo (f (0.3f, -0.25f), f (0.32f, -0.4f));
        body.quadraticTo (f (0.25f, -0.5f), f (0.12f, -0.48f));
        body.quadraticTo (f (0.15f, -0.64f), f (0.1f, -0.8f));
        body.quadraticTo (f (0.0f, -0.84f), f (-0.12f, -0.8f));
        body.closeSubPath();
        fill (g, body, lilac, f (0.0f, -0.8f), f (0.0f, 0.0f));

        stroke (g, curve (f (0.23f, -0.42f), f (0.2f, -0.22f), f (0.25f, -0.09f)), 0.8f, 0.4f);
        stroke (g, curve (f (0.07f, -0.44f), f (0.02f, -0.22f), f (0.05f, -0.04f)), 0.8f, 0.4f);
        stroke (g, curve (f (-0.1f, -0.42f), f (-0.16f, -0.22f), f (-0.17f, -0.04f)), 0.8f, 0.4f);
        stroke (g, curve (f (-0.04f, -0.47f), f (0.12f, -0.43f), f (0.3f, -0.4f)), 0.8f, 0.4f);

        // Rank badge: a gold square with a blue roundel.
        const auto badge = around (f (0.03f, -0.64f), f.len (0.12f), f.len (0.11f));
        fill (g, badge, gold, 0.7f);
        g.setColour (azurite.withAlpha (0.85f));
        g.fillEllipse (badge.reduced (badge.getWidth() * 0.2f));
        g.setColour (gold.brighter (0.3f));
        g.drawEllipse (badge.reduced (badge.getWidth() * 0.32f), 0.8f);

        paintBelt (g, f, -0.5f, 0.17f);
        paintCollar (g, f, -0.8f, lilac);

        // The throwing arm, sleeve hanging loose beneath it.
        juce::Path sleeve;
        sleeve.startNewSubPath (f (0.04f, -0.78f));
        sleeve.quadraticTo (f (0.2f, -0.79f), f (0.33f, -0.74f));
        sleeve.lineTo (f (0.34f, -0.67f));
        sleeve.quadraticTo (f (0.26f, -0.63f), f (0.22f, -0.52f));
        sleeve.quadraticTo (f (0.12f, -0.48f), f (0.05f, -0.6f));
        sleeve.closeSubPath();
        fill (g, sleeve, lilac.brighter (0.08f));
        stroke (g, curve (f (0.1f, -0.75f), f (0.2f, -0.67f), f (0.23f, -0.56f)), 0.7f, 0.35f);

        paintArrow (g, f (0.2f, -0.65f), f (0.62f, -0.8f));
        juce::Path fist;
        fist.addEllipse (around (f (0.36f, -0.705f), f.len (0.055f), f.len (0.05f)));
        fill (g, fist, skin, 0.6f);

        paintHead (g, f, 0.03f, -0.885f, Hat::winged, true);
    }

    //==============================================================================
    /** The bronze pot on its lacquer stand: a round belly, a long neck with two tubular ears and
        a flared mouth, with the arrows that have already gone in. */
    void paintPot (juce::Graphics& g, P base, float h)
    {
        const auto x = base.x;
        const auto mouthY = base.y - h;
        groundShadow (g, base.translated (0.0f, h * 0.09f), h * 0.9f);

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

        g.setGradientFill (juce::ColourGradient (gold.brighter (0.5f), { x - h * 0.12f, base.y - h * 0.36f },
                                                 gold.darker (0.75f), { x + h * 0.4f, base.y - h * 0.2f }, true));
        g.fillPath (pot);
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
                             h * 0.12f, h * 0.08f, malachite.withAlpha (0.3f));

            softEllipse (g, { x - h * 0.17f, base.y - h * 0.3f }, h * 0.1f, h * 0.3f, juce::Colours::white.withAlpha (0.35f));
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
        softEllipse (g, { 90.0f, 292.0f }, 200.0f, 40.0f, earth.withAlpha (0.12f));
        softEllipse (g, { 390.0f, 298.0f }, 150.0f, 34.0f, earth.withAlpha (0.12f));
        softEllipse (g, { 380.0f, 232.0f }, 140.0f, 28.0f, earth.withAlpha (0.1f));

        paintPavilion (g, face.getX());
        paintPine (g);

        for (const auto& [x, y, size] : { std::tuple { 20.0f, 290.0f, 9.0f }, { 120.0f, 300.0f, 10.0f }, { 152.0f, 306.0f, 7.0f },
                                         { 318.0f, 304.0f, 8.0f }, { 350.0f, 302.0f, 10.0f }, { 440.0f, 302.0f, 9.0f },
                                         { 352.0f, 226.0f, 8.0f }, { 438.0f, 228.0f, 9.0f } })
            paintGrass (g, { x, y }, size);

        paintAttendant (g, { 32.0f, 282.0f }, 52.0f, 1.0f, azurite, Pose::tray);
        paintHost (g, { 78.0f, 296.0f }, 76.0f, 1.0f);

        paintPot (g, { 396.0f, 222.0f }, 72.0f);
        paintArrow (g, { 334.0f, 66.0f }, { 356.0f, 78.0f });
        paintArrow (g, { 330.0f, 234.0f }, { 354.0f, 226.0f });

        paintAttendant (g, { 372.0f, 300.0f }, 52.0f, -1.0f, malachite, Pose::arrows);
        paintAttendant (g, { 418.0f, 296.0f }, 50.0f, -1.0f, azurite, Pose::clasped);

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
