#pragma once

#include <at_shared/at_shared.h>

/** The pedal face: a Ming court painting of a game of pitch-pot (touhu) on aged silk in a
    brocade mount. A host seated by a pavilion takes aim with an arrow, attendants in green and
    blue wait with trays and spare arrows, and the bronze pot stands across the lawn under a pine.
    Drawn in flat gongbi colour with fine ink outlines, laid out round the 460 x 640 editor's controls. */
namespace CourtPainting
{
    /** Paints the whole background: mount, silk, scene and wear. Slow; cache the result. */
    void paint (juce::Graphics&, juce::Rectangle<float> bounds);

    /** A red collector's seal reading 投壺, "pitch-pot", for beside the nameplate. */
    void paintSeal (juce::Graphics&, juce::Rectangle<float>);
}
