# Graph Report - Cavernous  (2026-10-14)

## Corpus Check
- 13 files · ~67,452 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 375 nodes · 587 edges · 21 communities (18 shown, 3 thin omitted)
- Extraction: 93% EXTRACTED · 7% INFERRED · 0% AMBIGUOUS · INFERRED: 39 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `6ec9681f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- LedReadout
- PluginProcessor
- CavernReverb
- LearnButton
- LayerVoice
- PluginEditor
- StereoDelay
- PluginProcessor.cpp
- PitchShifter
- LayerVoice.h
- MetalSlider
- processBlock
- Cavernous
- String
- applyAction
- LearnSlider
- MetalSwitch
- LearnControls.h
- build.sh
- MouseEvent
- PluginEditor.cpp

## God Nodes (most connected - your core abstractions)
1. `PluginProcessor` - 89 edges
2. `CavernReverb` - 52 edges
3. `LearnSlider` - 26 edges
4. `StereoDelay` - 22 edges
5. `PitchShifter` - 21 edges
6. `Content()` - 18 edges
7. `LearnButton` - 16 edges
8. `LayerVoice` - 14 edges
9. `PluginEditor` - 14 edges
10. `MetalSwitch` - 14 edges

## Surprising Connections (you probably didn't know these)
- `rebuildLayerButtons()` --calls--> `selectLayerAction()`  [INFERRED]
  Source/PluginEditor.cpp → Source/Parameters.h
- `bindControls()` --calls--> `reverbTypeAction()`  [INFERRED]
  Source/PluginEditor.cpp → Source/Parameters.h
- `settingsForLayer` --calls--> `octaveSemitones()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `applyAction` --calls--> `legacyOctaveIndex()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `setStateInformation` --calls--> `legacyOctaveIndex()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h

## Import Cycles
- None detected.

## Communities (21 total, 3 thin omitted)

### Community 0 - "LedReadout"
Cohesion: 0.40
Nodes (4): Component, String, LedReadout, text

### Community 1 - "PluginProcessor"
Cohesion: 0.04
Nodes (41): AudioProcessorValueTreeState, kMaxLayers, MidiSlot, array, AudioBuffer, AudioProcessor, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, SmoothedValue (+33 more)

### Community 2 - "CavernReverb"
Cohesion: 0.05
Nodes (38): CavernReverb, activeLength, allpass, allpassLength, allpassPos, bypass, coeff, currentType (+30 more)

### Community 3 - "LearnButton"
Cohesion: 0.22
Nodes (9): TextButton, LearnButton, action, consumeClick, learning, mapped, onClear, onLearn (+1 more)

### Community 4 - "LayerVoice"
Cohesion: 0.21
Nodes (10): AudioBuffer, LayerVoice, delays, reverbs, shifters, AudioBuffer, expect(), finiteBuffer() (+2 more)

### Community 5 - "PluginEditor"
Cohesion: 0.18
Nodes (11): AudioProcessorEditor, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, unique_ptr, PluginEditor, content, designHeight, designWidth, paint (+3 more)

### Community 6 - "StereoDelay"
Cohesion: 0.10
Nodes (16): array, AudioBuffer, SmoothedValue, vector, StereoDelay, buffer, bypass, dry (+8 more)

### Community 7 - "PluginProcessor.cpp"
Cohesion: 0.10
Nodes (21): BusesLayout, MemoryBlock, AudioProcessor, AudioProcessorEditor, String, JUCE_CALLTYPE createPluginFilter(), armMidiAction, armMidiLearn (+13 more)

### Community 8 - "PitchShifter"
Cohesion: 0.13
Nodes (14): array, AudioBuffer, SmoothedValue, vector, PitchShifter, buffer, currentSemitones, mask (+6 more)

### Community 9 - "LayerVoice.h"
Cohesion: 0.10
Nodes (20): atomic, array, vector, DelaySettings, delayMs, dryDb, feedback, on (+12 more)

### Community 10 - "MetalSlider"
Cohesion: 0.22
Nodes (5): Rectangle, Graphics, MetalSlider, ledBand, ledClick

### Community 11 - "processBlock"
Cohesion: 0.32
Nodes (8): MidiBuffer, octaveSemitones(), AudioBuffer, handleMidi, processBlock, renderTestTone, settingsForLayer, softClip

### Community 13 - "Cavernous"
Cohesion: 0.40
Nodes (4): Build, Cavernous, Layers, MIDI

### Community 15 - "applyAction"
Cohesion: 0.36
Nodes (7): applyAction, clearMappingForIndex, clearMidiMapping, getNumLayers, removeLayer, retargetMapping, setNumLayers

### Community 16 - "LearnSlider"
Cohesion: 0.12
Nodes (15): Slider, unique_ptr, LearnSlider, committing, consumeClick, entry, entryCharacters, learning (+7 more)

### Community 17 - "MetalSwitch"
Cohesion: 0.20
Nodes (9): function, Button, MetalSwitch, consumeClick, learning, mapped, onClear, onLearn (+1 more)

### Community 18 - "LearnControls.h"
Cohesion: 0.22
Nodes (3): GlowLed, on, MetalKnob

### Community 21 - "PluginEditor.cpp"
Cohesion: 0.07
Nodes (52): AudioParameterChoiceAttributes, AudioParameterFloatAttributes, Colour, Font, LookAndFeel_V4, NormalisableRange, ParameterLayout, ScrollBar (+44 more)

## Knowledge Gaps
- **138 isolated node(s):** `sr`, `lengths`, `activeLength`, `position`, `currentType` (+133 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PluginProcessor` connect `PluginProcessor` to `LayerVoice`, `PluginProcessor.cpp`, `LayerVoice.h`, `processBlock`, `String`, `applyAction`, `PluginEditor.cpp`?**
  _High betweenness centrality (0.598) - this node is a cross-community bridge._
- **Why does `LayerVoice` connect `LayerVoice` to `PluginProcessor`, `CavernReverb`, `StereoDelay`, `PitchShifter`, `LayerVoice.h`?**
  _High betweenness centrality (0.398) - this node is a cross-community bridge._
- **Why does `Content()` connect `PluginEditor.cpp` to `PluginProcessor`, `PluginEditor`?**
  _High betweenness centrality (0.321) - this node is a cross-community bridge._
- **What connects `sr`, `lengths`, `activeLength` to the rest of the system?**
  _138 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `PluginProcessor` be split into smaller, more focused modules?**
  _Cohesion score 0.037037037037037035 - nodes in this community are weakly interconnected._
- **Should `CavernReverb` be split into smaller, more focused modules?**
  _Cohesion score 0.04846938775510204 - nodes in this community are weakly interconnected._
- **Should `StereoDelay` be split into smaller, more focused modules?**
  _Cohesion score 0.1038961038961039 - nodes in this community are weakly interconnected._