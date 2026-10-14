# Graph Report - Cavernous  (2026-10-14)

## Corpus Check
- 14 files · ~76,723 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 508 nodes · 755 edges · 24 communities (20 shown, 4 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 68 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `bffc3fc5`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- LedReadout
- PluginProcessor
- CavernReverb
- MouseEvent
- ToneBurst
- LearnButton
- StereoDelay
- PluginProcessor.cpp
- PitchShifter
- LayerVoice
- MetalSlider
- processBlock
- .process
- Cavernous
- String
- LearnSlider
- MetalSwitch
- LearnControls.h
- build.sh
- Parameters.h
- PluginEditor
- createEditor

## God Nodes (most connected - your core abstractions)
1. `PluginProcessor` - 124 edges
2. `CavernReverb` - 74 edges
3. `StereoDelay` - 28 edges
4. `ToneBurst` - 26 edges
5. `LearnSlider` - 26 edges
6. `PluginEditor` - 24 edges
7. `PitchShifter` - 21 edges
8. `applyAction` - 19 edges
9. `main()` - 19 edges
10. `LayerVoice` - 17 edges

## Surprising Connections (you probably didn't know these)
- `main()` --calls--> `toneFrequencyHz()`  [INFERRED]
  tests/DspTest.cpp → Source/Dsp/ToneSource.h
- `main()` --calls--> `droneNoteHz()`  [INFERRED]
  tests/DspTest.cpp → Source/Dsp/ToneSource.h
- `removeLayer` --calls--> `isToneLayerParameter()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `settingsForLayer` --calls--> `octaveSemitones()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `applyAction` --calls--> `legacyOctaveIndex()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h

## Import Cycles
- None detected.

## Communities (24 total, 4 thin omitted)

### Community 0 - "LedReadout"
Cohesion: 0.40
Nodes (4): Component, String, LedReadout, text

### Community 1 - "PluginProcessor"
Cohesion: 0.03
Nodes (66): AudioProcessorValueTreeState, kMaxLayers, LayerFade, MidiSlot, array, atomic, AudioBuffer, AudioProcessor (+58 more)

### Community 2 - "CavernReverb"
Cohesion: 0.04
Nodes (55): CavernReverb, activeLength, activeTaps, allpass, allpassLength, allpassPos, bypass, coeff (+47 more)

### Community 3 - "MouseEvent"
Cohesion: 0.33
Nodes (4): MouseEvent, consumeClick, onClear, onLearn

### Community 4 - "ToneBurst"
Cohesion: 0.06
Nodes (41): droneNoteHz(), droneScaleSemitone(), DroneTones, gain, mode, octave, partials, phases (+33 more)

### Community 5 - "LearnButton"
Cohesion: 0.25
Nodes (7): function, TextButton, LearnButton, action, learning, mapped, paramId

### Community 6 - "StereoDelay"
Cohesion: 0.08
Nodes (22): array, AudioBuffer, SmoothedValue, vector, StereoDelay, buffer, bypass, droneBuffer (+14 more)

### Community 7 - "PluginProcessor.cpp"
Cohesion: 0.11
Nodes (30): BusesLayout, MemoryBlock, AudioProcessor, String, JUCE_CALLTYPE createPluginFilter(), applyAction, armMidiAction, armMidiLearn (+22 more)

### Community 8 - "PitchShifter"
Cohesion: 0.13
Nodes (14): array, AudioBuffer, SmoothedValue, vector, PitchShifter, buffer, currentSemitones, mask (+6 more)

### Community 9 - "LayerVoice"
Cohesion: 0.06
Nodes (33): array, atomic, vector, DelaySettings, delayMs, dryDb, feedback, flutter (+25 more)

### Community 10 - "MetalSlider"
Cohesion: 0.22
Nodes (5): Rectangle, Graphics, MetalSlider, ledBand, ledClick

### Community 11 - "processBlock"
Cohesion: 0.20
Nodes (12): MidiBuffer, octaveSemitones(), AudioBuffer, beginLayerFade, beginLayerReturn, cancelLayerFade, handleMidi, processBlock (+4 more)

### Community 13 - "Cavernous"
Cohesion: 0.40
Nodes (4): Build, Cavernous, Layers, MIDI

### Community 16 - "LearnSlider"
Cohesion: 0.12
Nodes (15): Slider, unique_ptr, LearnSlider, committing, consumeClick, entry, entryCharacters, learning (+7 more)

### Community 17 - "MetalSwitch"
Cohesion: 0.22
Nodes (8): Button, MetalSwitch, consumeClick, learning, mapped, onClear, onLearn, paramId

### Community 18 - "LearnControls.h"
Cohesion: 0.22
Nodes (3): GlowLed, on, MetalKnob

### Community 21 - "Parameters.h"
Cohesion: 0.10
Nodes (27): AudioParameterChoiceAttributes, AudioParameterFloatAttributes, NormalisableRange, ParameterLayout, droneModeName(), noiseColourName(), toneKeyName(), actionName() (+19 more)

### Community 23 - "PluginEditor"
Cohesion: 0.05
Nodes (42): AboutPanel, Colour, Content, Font, juce_wchar, KeyListener, KeyPress, Label (+34 more)

## Knowledge Gaps
- **226 isolated node(s):** `sr`, `lengths`, `activeLength`, `position`, `currentType` (+221 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PluginProcessor` connect `PluginProcessor` to `ToneBurst`, `PluginProcessor.cpp`, `LayerVoice`, `processBlock`, `String`, `Parameters.h`, `createEditor`?**
  _High betweenness centrality (0.459) - this node is a cross-community bridge._
- **Why does `LayerVoice` connect `LayerVoice` to `PluginProcessor`, `CavernReverb`, `ToneBurst`, `StereoDelay`, `PitchShifter`?**
  _High betweenness centrality (0.285) - this node is a cross-community bridge._
- **Why does `CavernReverb` connect `CavernReverb` to `PitchShifter`, `LayerVoice`, `.process`, `.setParameters`, `.prepare`?**
  _High betweenness centrality (0.260) - this node is a cross-community bridge._
- **What connects `sr`, `lengths`, `activeLength` to the rest of the system?**
  _226 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `PluginProcessor` be split into smaller, more focused modules?**
  _Cohesion score 0.02531645569620253 - nodes in this community are weakly interconnected._
- **Should `CavernReverb` be split into smaller, more focused modules?**
  _Cohesion score 0.03571428571428571 - nodes in this community are weakly interconnected._
- **Should `ToneBurst` be split into smaller, more focused modules?**
  _Cohesion score 0.055904961565338925 - nodes in this community are weakly interconnected._