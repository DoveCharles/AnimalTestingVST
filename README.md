# Animal Testing

A monorepo of audio plugins by **Animal Testing**. Each plugin builds as VST3 (Windows and macOS), AU (macOS) and Standalone, using [JUCE](https://juce.com) and CMake.

| Plugin | Code | Version | Description |
|---|---|---|---|
| [AT Saturator](plugins/Saturator) | `Satr` | 1.0.0 | tanh saturator with Drive, Tone (low-pass) and Mix |
| [AT Waterfall](plugins/Waterfall) | `Wtfl` | 1.0.0 | SP-404-style chord resonator: Key, Chord, Brightness, Decay, Mix, with a soft sky-blue spectrogram |

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

## Licensing note

JUCE is dual-licensed under AGPLv3 and a commercial JUCE licence. To distribute closed-source plugins you need a commercial JUCE licence (see `libs/JUCE/LICENSE.md`). VST3 uses the Steinberg VST3 SDK, which JUCE bundles under its own licence terms.
