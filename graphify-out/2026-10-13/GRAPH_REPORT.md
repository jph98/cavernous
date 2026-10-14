# Graph Report - Cavernous  (2026-10-13)

## Corpus Check
- 12 files · ~61,257 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 354 nodes · 532 edges · 16 communities (14 shown, 2 thin omitted)
- Extraction: 94% EXTRACTED · 6% INFERRED · 0% AMBIGUOUS · INFERRED: 33 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `6ec9681f`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- content
- PluginProcessor
- CavernReverb
- LearnButton
- LayerVoice
- PluginEditor
- StereoDelay
- PluginProcessor.cpp
- PitchShifter
- LayerVoice.h
- GlowLed
- applyAction
- processBlock
- Cavernous
- String
- createEditor

## God Nodes (most connected - your core abstractions)
1. `PluginProcessor` - 84 edges
2. `content` - 62 edges
3. `CavernReverb` - 44 edges
4. `StereoDelay` - 21 edges
5. `PitchShifter` - 19 edges
6. `LearnButton` - 17 edges
7. `LearnSlider` - 16 edges
8. `MetalSwitch` - 15 edges
9. `LayerVoice` - 14 edges
10. `LayerSettings` - 12 edges

## Surprising Connections (you probably didn't know these)
- `getArmedParameterName` --calls--> `actionName()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `applyAction` --calls--> `layerParam()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `removeLayer` --calls--> `layerParam()`  [INFERRED]
  Source/PluginProcessor.h → Source/Parameters.h
- `PluginEditor::PluginEditor()` --references--> `PluginProcessor`  [EXTRACTED]
  Source/PluginEditor.cpp → Source/PluginProcessor.h
- `LayerVoice` --references--> `CavernReverb`  [EXTRACTED]
  Source/Dsp/LayerVoice.h → Source/Dsp/CavernReverb.h

## Import Cycles
- None detected.

## Communities (16 total, 2 thin omitted)

### Community 0 - "content"
Cohesion: 0.05
Nodes (49): AudioProcessorValueTreeState::Listener, ButtonAttachment, Image, Label, NormalisableRange, OwnedArray, ParameterLayout, SliderAttachment (+41 more)

### Community 1 - "PluginProcessor"
Cohesion: 0.04
Nodes (37): AudioProcessorValueTreeState, kMaxLayers, MidiSlot, array, AudioBuffer, AudioProcessor, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, SmoothedValue (+29 more)

### Community 2 - "CavernReverb"
Cohesion: 0.06
Nodes (32): CavernReverb, activeLength, allpass, allpassLength, allpassPos, bypass, coeff, currentType (+24 more)

### Community 3 - "LearnButton"
Cohesion: 0.08
Nodes (28): function, MouseEvent, Slider, Button, String, TextButton, LearnButton, action (+20 more)

### Community 4 - "LayerVoice"
Cohesion: 0.12
Nodes (19): array, AudioBuffer, LayerSettings, decaySeconds, delay, reverbDryDb, reverbOn, reverbType (+11 more)

### Community 5 - "PluginEditor"
Cohesion: 0.11
Nodes (17): Colour, Content, Font, LookAndFeel_V4, BarLookAndFeel, Button, Graphics, TextButton (+9 more)

### Community 6 - "StereoDelay"
Cohesion: 0.10
Nodes (16): array, AudioBuffer, SmoothedValue, vector, StereoDelay, buffer, bypass, dry (+8 more)

### Community 7 - "PluginProcessor.cpp"
Cohesion: 0.12
Nodes (18): BusesLayout, MemoryBlock, AudioProcessor, String, JUCE_CALLTYPE createPluginFilter(), armMidiAction, armMidiLearn, cancelMidiLearn (+10 more)

### Community 8 - "PitchShifter"
Cohesion: 0.12
Nodes (14): array, AudioBuffer, SmoothedValue, vector, PitchShifter, buffer, currentSemitones, mask (+6 more)

### Community 9 - "LayerVoice.h"
Cohesion: 0.19
Nodes (9): atomic, array, vector, DelaySettings, delayMs, dryDb, feedback, on (+1 more)

### Community 10 - "GlowLed"
Cohesion: 0.16
Nodes (6): Rectangle, GlowLed, on, Component, Graphics, MetalSlider

### Community 11 - "applyAction"
Cohesion: 0.36
Nodes (7): applyAction, clearMappingForIndex, clearMidiMapping, getNumLayers, removeLayer, retargetMapping, setNumLayers

### Community 12 - "processBlock"
Cohesion: 0.40
Nodes (6): MidiBuffer, AudioBuffer, handleMidi, processBlock, settingsForLayer, softClip

### Community 13 - "Cavernous"
Cohesion: 0.40
Nodes (4): Build, Cavernous, Layers, MIDI

## Knowledge Gaps
- **136 isolated node(s):** `sr`, `lengths`, `activeLength`, `position`, `currentType` (+131 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PluginProcessor` connect `PluginProcessor` to `content`, `LayerVoice`, `PluginEditor`, `PluginProcessor.cpp`, `LayerVoice.h`, `applyAction`, `processBlock`, `String`, `createEditor`?**
  _High betweenness centrality (0.652) - this node is a cross-community bridge._
- **Why does `content` connect `content` to `PluginProcessor`, `GlowLed`, `LearnButton`, `PluginEditor`?**
  _High betweenness centrality (0.481) - this node is a cross-community bridge._
- **Why does `LayerVoice` connect `LayerVoice` to `PluginProcessor`, `CavernReverb`, `StereoDelay`, `PitchShifter`, `LayerVoice.h`?**
  _High betweenness centrality (0.425) - this node is a cross-community bridge._
- **What connects `sr`, `lengths`, `activeLength` to the rest of the system?**
  _136 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `content` be split into smaller, more focused modules?**
  _Cohesion score 0.050203527815468114 - nodes in this community are weakly interconnected._
- **Should `PluginProcessor` be split into smaller, more focused modules?**
  _Cohesion score 0.04 - nodes in this community are weakly interconnected._
- **Should `CavernReverb` be split into smaller, more focused modules?**
  _Cohesion score 0.056910569105691054 - nodes in this community are weakly interconnected._