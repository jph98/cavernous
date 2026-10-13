# Cavernous

Stereo delay, reverb, and octave effect. Each layer is its own chain — octave, then delay, then reverb — and layers sum together.

The faceplate follows the Cavernous pedal: delay time, feedback, dry, and wet; reverb dry, wet, and decay; an octave fader (−2, −1, Off, +1, +2); and bypass switches for delay and reverb.

## Layers

**+ New Layer** adds another chain, up to eight. The numbered buttons choose which layer the faceplate is editing. **Remove** deletes the selected layer and keeps the ones after it. Each layer has its own level in the bar under the pedal. New layers start 6 dB down so they don't jump the volume.

## MIDI

Every slider, the octave fader, both footswitches, and the layer level can be assigned.

- Right-click a control and choose **MIDI Learn**, then move a hardware CC.
- Or click **MIDI Learn**, click the control, then move a CC.
- Assignments are stored with the preset. The same parameters are also host-automatable, so a DAW's own MIDI learn works too.

The track feeding the plugin needs MIDI input enabled.

## Build

Requires CMake, Xcode, and git. The first configure downloads JUCE 8.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build copies the plugins into:

- `~/Library/Audio/Plug-Ins/VST3/Cavernous.vst3`
- `~/Library/Audio/Plug-Ins/Components/Cavernous.component`

The standalone app is `build/Cavernous_artefacts/Release/Standalone/Cavernous.app`.

`CavernousDspTest` checks the delay time, bypass, and reverb tail.
