#pragma once

#include <at_shared/at_shared.h>

/** The pedal's artwork: a one-colour print, close up from the side, of a heeler slung low and
    nipping at a cow's heel as the hoof comes down. Drawn as silhouettes in one ink, with the
    details cut out of them so the enclosure shows through. */
namespace HeelerPrint
{
    /** Paints the print, framed, fitted to `area`. `paper` is the enclosure colour behind it,
        used for the thin gap that keeps the dog apart from the leg. */
    void paint (juce::Graphics&, juce::Rectangle<float> area, juce::Colour ink, juce::Colour paper);
}
