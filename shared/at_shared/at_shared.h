/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 at_shared
  vendor:             Animal Testing
  version:            0.1.0
  name:               Animal Testing shared code
  description:        LookAndFeel, DSP building blocks and parameter/preset helpers shared by all plugins.
  license:            Proprietary
  minimumCppStandard: 20

  dependencies:       juce_audio_utils, juce_dsp

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/at_Waveshapers.h"
#include "dsp/at_CombResonator.h"
#include "params/at_ParameterHelpers.h"
#include "params/at_PresetManager.h"
#include "processor/at_ProcessorBase.h"
#include "gui/at_LookAndFeel.h"
#include "gui/at_Knob.h"
#include "gui/at_PresetBar.h"
