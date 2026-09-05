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
```

Options: `--out` (default `ui_snapshot.png`, relative to the working directory), `--preset`, `--bank`,
`--scale` (render scale, default 2), `--settle` (milliseconds of message-loop time given to
asynchronous UI updates before capture, default 300).
