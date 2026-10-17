# Afo-Q 4

A 24-band parametric EQ with per-band dynamics, built in JUCE (VST3 / AU / Standalone, AAX with Avid's SDK).

- **Try it in your browser:** `https://<your-username>.github.io/<repo-name>/` (after the one-time Pages setup below)
- **Download the plugin:** the **Releases** page of this repo (after you push a version tag)

## Publish this to GitHub (about 10 minutes, from a computer)
1. Create a GitHub account and install `git` (and optionally the GitHub CLI, `gh`).
2. In this folder run `./publish.sh` (Mac/Linux) or `powershell -ExecutionPolicy Bypass -File publish.ps1` (Windows).
   It creates a public repo and pushes everything. Without `gh`, it prints the three manual commands instead.
   No git? On github.com choose **Add file > Upload files** and drag in the whole folder, including `.github`.
3. **Web version:** repo **Settings > Pages > Source: GitHub Actions**, then **Actions > Publish web version > Run workflow**.
4. **Plugin downloads:** the build workflow runs on every push (VST3 + Standalone on Windows, macOS, Linux; AU on macOS)
   and attaches the results to the run under **Actions > run > Artifacts**.
   For a clean download page, create a tag: `git tag v0.1.0 && git push origin v0.1.0`. A **Release** with one zip per
   platform appears under **Releases** in about 10-15 minutes.
5. If a build goes red, open the failed step and send me the log.

Installing: copy the `.vst3` to `C:\Program Files\Common Files\VST3` (Windows), `/Library/Audio/Plug-Ins/VST3` or
`~/Library/Audio/Plug-Ins/Components` for AU (macOS), `~/.vst3` (Linux). Builds are unsigned: on macOS right-click > Open
(or `xattr -dr com.apple.quarantine <file>`); on Windows choose "Run anyway" in SmartScreen.

## Licence note (important)
JUCE is AGPLv3 or commercial. A public repo with downloadable binaries is fine if you release this project under AGPLv3
(on GitHub: **Add file > Create new file > LICENSE**, then use the template picker). To distribute closed-source builds
you need a JUCE commercial licence. AAX also needs Avid's SDK.

## Build locally
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
    # AAX:  cmake -B build -DAAX_SDK_PATH=/path/to/aax-sdk
JUCE 8.0.4 is fetched automatically on first configure.

## DSP tests (no JUCE needed)
    g++ -std=c++17 -O2 -ISource Tests/filter_test.cpp -o ft && ./ft
    g++ -std=c++17 -O2 -ITests/stub -ISource Tests/engine_test.cpp -o et && ./et

## How it works
- **Zero Latency:** per-band biquads. Cuts are Butterworth cascades (6-96 dB/oct; "brickwall" is order 32 here).
- **Natural Phase:** zero latency, analog-matched bells (no cramping near Nyquist). An approximation, not a vendor copy.
- **Linear Phase:** a background thread builds a 2x2 FIR matrix from the bands (every channel mode exact, cached per band,
  up to ~80 updates/s); JUCE's partitioned convolver runs it. Latency = taps/2, reported to the host.
- **Dynamics (per band):** band-pass detector (narrow = band Q, wide = a quarter of it) -> peak follower -> 8 dB soft knee
  -> attack/release -> gain = static gain + range x activation. No lookahead, no added latency. Threshold is in dBFS of
  the band-filtered signal (default -30).
- **Channels:** stereo, left, right, mid, side per band. Mode switches fade out/in so they don't click.
- **GUI:** `Source/UI/` (curve view, nodes, panel, menus, undo/redo, A/B, presets). `docs/index.html` is the browser
  version, which runs the same EQ engine on real audio (zero-latency mode only).

## Known limitations
- The plugin UI has not been compiled by me (no network in my sandbox); expect small fixes on the first CI run.
- Linear-Phase dynamics follow the signal a little slower than the other modes (IR swaps are cross-faded by JUCE).
- Undo/redo snapshots the whole parameter tree after each edit (100 steps).
