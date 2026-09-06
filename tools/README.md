# Developer tools

## ui_snapshot

Renders the plugin editor off-screen to a PNG so UI changes can be reviewed without opening a host.
It is built with the tests (target `YMulatorSynthAU_UISnapshot`, requires Google Test to be found).

```bash
cd build
cmake --build . --target YMulatorSynthAU_UISnapshot
./bin/YMulatorSynthAU_UISnapshot --list-presets
./bin/YMulatorSynthAU_UISnapshot --out ui.png --bank 0 --preset 2      # via the UI's bank/preset path
./bin/YMulatorSynthAU_UISnapshot --out ui.png --preset 2               # via host program change
./bin/YMulatorSynthAU_UISnapshot --out ui.png --scale 1 --settle 500
./bin/YMulatorSynthAU_UISnapshot --out ui.png --preset 2 --then-preset 5 --dump   # program change with the editor open
./bin/YMulatorSynthAU_UISnapshot --out ui.png --preset 3 --focus-macro 0          # highlight the knobs a macro drives
```

Options: `--out` (default `ui_snapshot.png`, relative to the working directory), `--preset`, `--bank`,
`--then-preset` (a second program change after the editor exists), `--dump` (print state properties
and every combo box text), `--scale` (render scale, default 2), `--settle` (milliseconds of
message-loop time given to asynchronous UI updates before capture, default 300), `--focus-macro N`
(0 Brightness, 1 Harmonics, 2 Attack, 3 Decay, 4 Release, 5 Spread: draws the amber rings on that
macro's target knobs, the way touching the TONE knob does), `--note N` (holds MIDI note N for 40 blocks
before the capture so the output scope shows a waveform).

## gen_algorithm_svg.py

Writes `resources/algorithms/algorithm0..7.svg` and `algorithm0..7_fb.svg` (feedback loop lit), the algorithm diagrams the editor shows
(embedded through `juce_add_binary_data`). Roles and edges follow `src/dsp/AlgorithmInfo.h`;
only the box positions live in the script. Re-run it from the repository root after changing
either, then rebuild.

```bash
python3 tools/gen_algorithm_svg.py
```
