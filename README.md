# Animal Testing

A monorepo of audio plugins by **Animal Testing**. Each plugin builds as VST3 (Windows and macOS), AU (macOS) and Standalone, using [JUCE](https://juce.com) and CMake.

| Plugin | Code | Version | Description |
|---|---|---|---|
| [AT Saturator](plugins/Saturator) | `Satr` | 1.0.0 | tanh saturator with Drive, Tone (low-pass) and Mix |
| [AT Waterfall](plugins/Waterfall) | `Wtfl` | 1.0.0 | SP-404-style chord resonator: Key, Octave (C-1 to G9), Chord, Brightness, Decay, Mix, with a soft sky-blue spectrogram |
| [AT Green Rooskie](plugins/GreenRooskie) | `Grsk` | 1.0.0 | Fuzz modelled on the 90s green Russian muff: Volume, Tone, Sustain and a footswitch, 4x oversampled |
| [AT Shifty](plugins/Shifty) | `Shft` | 1.0.0 | Late-80s digital pitch shifter/delay: Balance, Feedback, Fine-Manual and a six-way Mode (125 ms / 500 ms / 2 s delay, manual / -1 oct / +1 oct pitch) |
| [AT Sky Chorus](plugins/SkyChorus) | `Skyc` | 1.1.0 | Mid-80s analog BBD chorus after the Ibanez SC10, its 1024-stage bucket brigade run at its own swept clock (4x oversampled), dressed in the colours of Blue Smiley's *Return*: Speed, Width, Delay Time, Mix and a footswitch, with a Stereo switch: Bi-Phase (A = dry + wet, B = dry − wet, like the original), Dry/Wet Split or Mono-Safe |
| [AT Cabinet](plugins/Cabinet) | `Cabn` | 1.0.0 | Cab simulator: six built-in cabs (4x12 Vintage and Modern, 2x12 Open Alnico, 1x12 Open Combo, 4x10 Tweed, 1x8 Practice) with Dynamic, Ribbon or Condenser mic, a speaker pad for mic Position and Distance, Room, Push (speaker drive), Low/High Cut and Output, or load your own IR (embedded in the session); zero latency |
| [AT Seven Rooms](plugins/SevenRooms) | `Svnr` | 1.0.0 | Stereo digital reverb after the DigiTech HardWire RV-7: Level, Liveliness, Decay and seven Reverb Types (Room, Plate, Reverse, Modulated, Gated, Hall, Spring), with a Tails switch so the reverb rings out after bypass; Reverse is wet-only, like the original |
| [AT Chilltron](plugins/Chilltron) | `Chtn` | 0.1.0 | Mellotron-style tape replay keyboard, rebuilt from the Redtron SE tapes: two tape banks (A/B) with a blend fader, per-key 8-second tapes that rewind on release, capstan wow & flutter, tape clicks, optional loop, tone, spread and reverb. Tapes are read from `~/Library/Application Support/Animal Testing/Chilltron/Tapes` |
| [AT Voice Box](plugins/VoiceBox) | `Vbox` | 0.1.0 | Vocal pitch and formant shifter (PSOLA with YIN tracking, so pitch and formant move independently): Transpose, Quantize (Key/Scale/Retune) and Robot modes, a Gender macro, and a Character section with Chest Cut, Air, Breath, Drive and delayed Vibrato; about 40 ms latency, reported to the host |
| [AT Smooth Operator](plugins/SmoothOperator) | `Smop` | 0.1.0 | Dynamic resonance suppressor: an STFT that cuts whatever sticks up out of the sound's own (harmonic-smoothed) spectrum, with Depth, Sharpness, Selectivity, Attack/Release, five-band weighting, Stereo or Mid/Side, Delta listen and a live cut display; about 43 ms latency, reported to the host |
| [AT Tapeworm](plugins/Tapeworm) | `Tpwm` | 0.1.0 | Lo-fi tape machine fitted to TapeCapture measurements: Drive (4x oversampled) with Boost, a morphing tape voicing (Tone) and a bipolar low/high-pass Filter, Wow, Flutter and Wear with an optional wide stereo transport, mono tape hiss and motor noise, Output, a tempo-synced Tape Stop (1/4 to 8 bars) with Instant or Fast-Forward catch-up, and Speed (0.25x to 1x) that slows everything down on up to 5 minutes of tape. A matte silver deck whose reels turn with the tape |
| [AT Good Boy](plugins/GoodBoy) | `Gdby` | 0.1.0 | One-knob multiband maximizer: Amount drives a three-band compressor with makeup, light saturation, high-band width and a lookahead limiter (-0.3 dBFS), voiced four ways by Mode (A Warm, B Bright, C Loud, D Big), plus Output; Amount 0 leaves the sound untouched (crossover phase only); 1.5 ms latency, reported to the host |
| [AT Big Mouth](plugins/BigMouth) | `Bmth` | 0.1.0 | Beatbox to MIDI drums: finds each hit in a mic signal and sends a drum note (GM numbers, so Addictive Drums 2 and most drum samplers play it). Five pads (Kick, Snare, Hi-Hat, Open Hat, Tom) with their own notes; untrained it sorts hits by brightness, or train each pad on eight of your own sounds (saved with the set). Threshold, Listen (window, about the note latency), Gap, Dynamics and Monitor. MIDI goes to the host and, on macOS, to a virtual port "AT Big Mouth" |

## Layout

```
CMakeLists.txt            top level: adds JUCE, the shared module, and the plugin registry
cmake/AnimalTesting.cmake manufacturer name/code, bundle ID, formats, at_plugin()/at_add_plugin()
shared/at_shared/         JUCE module shared by all plugins
  processor/              at::ProcessorBase: APVTS, state save/load, bus layouts
  params/                 parameter factory helpers and at::PresetManager
  gui/                    at::LookAndFeel (the house style), at::Knob, at::PresetBar
  dsp/                    reusable DSP (waveshapers, tuned comb resonator, ...)
plugins/<Name>/           one folder per plugin, self-contained
templates/PluginTemplate/ starting point for new plugins (a simple gain plugin)
scripts/NewPlugin.cmake   copies the template and registers the new plugin
tools/                    developer tools, built with -DAT_BUILD_TOOLS=ON
libs/JUCE/                JUCE submodule (pinned to 9.0.4)
```

## Requirements

- **CMake** ≥ 3.22 and **Ninja** (recommended)
- **macOS:** Xcode or just the Command Line Tools (`xcode-select --install`). Get full Xcode for debugging and, later, for signing.
- **Windows:** Visual Studio 2022 with the *Desktop development with C++* workload.

## Building

```bash
git clone --recurse-submodules <repo-url>
cd AnimalTestingVST
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If you cloned without `--recurse-submodules`, run `git submodule update --init` first.

Build a single format or plugin: `cmake --build build --target Saturator_VST3` (also `_AU`, `_Standalone`, or `Saturator` for all formats).

On Windows, use a *Developer PowerShell for VS 2022* so Ninja can find the compiler. You can also skip `-G Ninja` to generate a Visual Studio solution.

For an Xcode project: `cmake -S . -B build-xcode -G Xcode`.

### Where builds go

`COPY_PLUGIN_AFTER_BUILD` is on by default, so every build also installs the plugin:

| Format | macOS | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | n/a |

Windows needs an **elevated** (Administrator) shell to write to Program Files. Otherwise, turn the copy off with `-DAT_COPY_PLUGIN_AFTER_BUILD=OFF` and copy the plugin yourself. The build output is always in `build/plugins/<Name>/<Name>_artefacts/Release/`.

To collect every plugin into one folder (this is what CI does):

```bash
cmake --install build --prefix dist --component plugins
```

## Adding a new plugin

1. Pick a name and a unique 4-character code with **exactly one upper-case letter** (AU rule), e.g. `Dlay`.
2. Run:
   ```bash
   cmake -DNAME=Delay -DCODE=Dlay -P scripts/NewPlugin.cmake
   ```
   This copies `templates/PluginTemplate` to `plugins/Delay` and appends `at_plugin(Delay CODE Dlay)` to the top-level `CMakeLists.txt`.
3. Edit `plugins/Delay/CMakeLists.txt` to set the `PRODUCT_NAME` (the name shown in Ableton) and the starting `VERSION`.
4. Re-run the CMake configure step and build. Write your DSP in `PluginProcessor.cpp` and your UI in `PluginEditor.cpp`.

To do it by hand instead, copy the template folder, replace `PluginTemplate` with your name in its files, and add the `at_plugin(...)` line yourself.

### IDs and versions

- **Manufacturer code** (`AnTs`), manufacturer name and bundle ID prefix live in `cmake/AnimalTesting.cmake`, and nowhere else.
- **Plugin codes** live only in the `at_plugin(...)` registry at the bottom of the top-level `CMakeLists.txt`. Configure fails if a code is duplicated, isn't 4 characters, doesn't have exactly one upper-case letter, or equals the manufacturer code.
- **Never change a released plugin's code, manufacturer code or parameter IDs.** Hosts use them to find the plugin and its automation in saved Live sets.
- **Versions** are per plugin: `project(<Name> VERSION x.y.z)` at the top of `plugins/<Name>/CMakeLists.txt`. Bump this on every release, because Ableton and AU caches pick up changes by version.

### Parameters

Every parameter goes through `AudioProcessorValueTreeState` (created in `at::ProcessorBase`), so it's automatable in Ableton and saved with the Live set. Use the factories in `at::params` (`makeDecibels`, `makePercent`, `makeFrequency`, `makeSeconds`, `makeChoice`, `makeFloat`). When you add a parameter to an already released plugin, give it a higher version hint, e.g. `at::params::id ("newParam", 2)`.

### Presets

`at::PresetBar` (Save / Init and a preset list) saves user presets as `.atpreset` XML in `~/Documents/Animal Testing/<Plugin>/Presets/`. Ableton's own device presets (`.adv`) work as usual too.

## Previewing Waterfall offline

`WaterfallPreview` plays an audio file through the Waterfall processor and saves PNG snapshots of the editor, so you can check the spectrogram with real audio without a DAW:

```bash
cmake -S . -B build -DAT_BUILD_TOOLS=ON
cmake --build build --target WaterfallPreview
build/tools/WaterfallPreview/WaterfallPreview_artefacts/Release/WaterfallPreview song.mp3 out --every=5 --chord=Add9 --decay=10
```

Parameters take the same text the knobs show (`--key=E`, `--octave=5`, `--brightness=86`, `--mix=100`).

## Measuring a reference plugin

`TapeCapture` loads an installed VST3/AU (by default Arturia's Tape MELLO-FI), plays test signals through it and prints gain curves and harmonics, frequency response, wow/flutter rates and depths, noise spectra and stereo correlation. These are the targets for a model of our own:

```bash
cmake --build build --target TapeCapture
tools/TapeCapture/capture-mellofi.sh
```

That writes every test to `renders-mellofi/` (gitignored). A summary of the results is in [tools/TapeCapture/mellofi-targets.md](tools/TapeCapture/mellofi-targets.md). Single tests take parameters by name with normalised values, e.g. `TapeCapture wow --Wow=0.5 --"Stereo Width"=1`; `TapeCapture selftest` checks the analysis against signals with known answers. To measure our own model the same way, point it at another plugin and folder: `PLUGIN="$HOME/Library/Audio/Plug-Ins/VST3/AT Tapeworm.vst3" OUT=renders-tapeworm tools/TapeCapture/capture-mellofi.sh`.

## CI

`.github/workflows/build.yml`:

- Builds on `windows-latest` and `macos-latest` (macOS builds a universal arm64 + x86_64 binary).
- Uses ccache on macOS and sccache on Windows, stored in the GitHub Actions cache.
- Uploads `AnimalTesting-<ref>-<OS>.zip` (VST3, AU, Standalone) as an artifact on every push and pull request.
- When you push a tag like `v1.0.0`, it also creates a GitHub Release with both zips attached:
  ```bash
  git tag v1.0.0
  git push origin v1.0.0
  ```
- **TODO:** macOS code signing and notarization. There is a disabled placeholder step with instructions in the workflow. Until it's done, people downloading the Mac builds have to clear the quarantine flag, e.g. `xattr -dr com.apple.quarantine "AT Saturator.vst3"`.

## Loading in Ableton Live

1. Build, which installs the plugin into the folders above.
2. In Live, open **Settings → Plug-Ins**:
   - **macOS:** turn on *Use Audio Units v2* and/or *Use VST3 Plug-in System Folders*.
   - **Windows:** turn on *Use VST3 Plug-in System Folders*.
3. Click **Rescan**. After rebuilding a plugin that's already loaded, **Option-click** (macOS) or **Alt-click** (Windows) *Rescan* for a full rescan, and restart Live if the old binary stays loaded.
4. The plugins appear under **Plug-Ins → VST3 / Audio Units → Animal Testing**.

If Live ignores the AU after a rebuild, run `killall -9 AudioComponentRegistrar` and check it with `auval -v aufx Satr AnTs`.

## Beatboxing into Addictive Drums 2 (Big Mouth)

1. Put **AT Big Mouth** on an audio track with your mic as its input and monitoring set to *In*.
2. Put Addictive Drums 2 on a MIDI track and set its *MIDI From* to **AT Big Mouth** (the virtual port; on macOS it also shows up in Settings → Link, Tempo & MIDI, where *Track* must be on) with monitoring *In*. Hosts that route plug-in MIDI output can take it from the Big Mouth track instead.
3. Click **Train** on a pad and make that sound eight times; do the same for the others. Untrained, low sounds play the kick, middle the snare and high the hat.
4. Notes are GM (Kick C1 36, Snare D1 38, Hat F#1 42, Open Hat A#1 46, Tom A1 45); change a pad's note if your AD2 kit maps a piece elsewhere.

Each note comes out one Listen window (16 ms by default) after the hit starts, so quantize recorded MIDI if it needs to be tight. `BigMouthRender test` scores the detector on a synthetic beatbox pattern, and `BigMouthRender render take.wav hits.mid` turns a recording into MIDI.

## Licensing note

JUCE is dual-licensed under AGPLv3 and a commercial JUCE licence. To distribute closed-source plugins you need a commercial JUCE licence (see `libs/JUCE/LICENSE.md`). VST3 uses the Steinberg VST3 SDK, which JUCE bundles under its own licence terms.
