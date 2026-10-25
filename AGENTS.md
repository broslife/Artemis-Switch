# Agent memory

## Project goal

This repository is a fork of `antoinebou12/Artemis-Switch` (itself a fork of XITRIX/Moonlight-Switch): a Nintendo Switch game-streaming client written in C++ (Borealis UI, FFmpeg/NVDEC decode, deko3d render). The goal of this fork is **lower latency and smoother frame presentation**, keeping the existing C++ code base.

The plan lives in `docs/roadmap.md`. Follow its order of steps. Read it before starting any task.

Reference material from the Punktfunk project (MIT / Apache-2.0) is in `reference/punktfunk/`. It is **read-only reading material written in Rust**: do not compile it, do not edit it. Reimplement the logic in C++ and keep attribution to Punktfunk in a comment on the new code and in `docs/source/credits.md`.

## How to work with the user

- The user does not program and writes in Portuguese. Explain in Portuguese, objectively and didactically, and make the technical decisions an experienced programmer would make.
- Change only what was asked. Do not change what already works or is not part of the request. Add only what is necessary.
- Write code comments in English. Never edit or remove existing comments.
- Prefer libraries and helpers already in the project over new dependencies.
- When reporting a change, show only the changed snippet and say exactly where it is (file, function, approximate line) so the user can find it.
- You cannot run the app on a Switch. State clearly what was only built or unit-tested and what still needs an on-device check. Never claim a smoothness or latency improvement without a measurement.

## Learned User Preferences

- Always push to the current repository of the workspace (the repo selected for this session). Never push to or open PRs against any other repository.
- Check what is already on this fork's `main` before re-implementing anything. Prefer simple, non-breaking changes, in small sequenced steps (one roadmap step per change), not one mega-change.
- Every new presentation/pacing mode must be optional and keep a fallback to the current behavior (the current pacing stays the default until the new mode is proven on device).
- Gate Switch-only or risky features so other platforms and CI keep working (see `docs/OTHER_PLATFORMS.md`). Keep pacing math (clock, slot selection, judder counting) in portable code that unit tests can cover without a Switch.
- Credit XITRIX/Moonlight-Switch first in About/credits; keep the existing credits (Apollo, Artemis Classic, Moonlight, Artemide, Vibepollo, #323) and add Punktfunk.
- Keep NRO/app author metadata and Artemis branding as they are; do not rebrand.

## Existing behavior to preserve (do not change unless asked)

- Quick Actions overlay stays focused (keyboard, mouse, move window, volume, touch); host shortcuts are labeled by feature and stay distinct from server commands.
- Stream profiles (`profile.json`, schema 13) have parity with the Settings UI and never mutate global Settings. Low-latency pacing defaults Off. A new pacing option that belongs to a profile must follow the same schema rules, including a version bump if fields are added.
- Host web-config QR follows switch-wifi NanoVG batched rendering patterns; avoid per-module NanoVG fills on Switch/deko3d; do not crash if opening web-config fails.
- Connected host UI uses an Applications tab plus a Host settings tab. Sunshine/Apollo/Punktfunk are servers only.
- Debug stays under Stream Performance after Benchmark.
- On disconnect, stream errors, or deleted host apps, safely tear down the session so the Switch is not left on an orange or bricked screen.
- Fit/Fill/Stretch must keep filling correctly in handheld, dock, and 720p profiles.

## Learned Workspace Facts

- Build: the user installs nothing locally. Every build is done by GitHub Actions in the current repository. The Switch `.nro` is built by `.github/workflows/docker-image.yml` ("Build Artemis Switch", manual `workflow_dispatch`) inside the `devkitpro/devkita64` image, which already provides the Switch toolchain and libraries (libnx, deko3d, etc.). Do not vendor or copy Switch SDK libraries into the repo; the few public headers needed for reading (deko3d, libnx vi/timing) are in `reference/switch-api/` (read-only, not compiled), so look there before searching the internet. The result is the `Artemis-Switch-nro` artifact of the run (kept 14 days). Use the Switch-only workflow for iteration; `all-builds.yml` runs six platforms on every PR to `main` and is expensive. After pushing, check the run with `gh run watch` / `gh run view --log-failed` and fix compile errors before reporting a step as done. Portable unit tests and sanitizers run in `unit-tests.yml`. Default branch for day-to-day work is `main`.
- Code areas the roadmap touches (verify paths before editing):
  - `app/src/streaming/AVFrameHolder.cpp` / `.hpp`: frame queue and pacer (legacy path and low-latency path).
  - `app/src/streaming/LowLatencyFrameSelection.hpp`, `PresentDeadline.hpp`: low-latency selection and present deadline.
  - `app/src/streaming/video/deko3d/DKVideoRenderer.cpp` and `DKVideoRendererLegacy.inc`: draw loop and `rendered_fps` statistics (known off-by-one window bug, roadmap step 1).
  - `app/src/streaming/ffmpeg/FFmpegVideoDecoder.cpp`: decoder; today it uses only `receiveTimeUs` and ignores `DECODE_UNIT.presentationTimeMs`.
  - `app/src/streaming_view.cpp`: builds the debug overlay text and counters.
- Documentation: `docs/source/` is the Sphinx site (settings, profiles, overlay, performance, credits). Update the matching page when a user-visible option or metric changes. `docs/roadmap.md` is the plan and is not part of the site.
- Ignore when working on streaming/pacing: remote-access docs (Tailscale, NetBird, WireGuard), PS Vita and Linux notes, and `docs/source/_static/diagrams/*.svg`.
- Hosts: Sunshine/Apollo/Vibepollo/Vibeshine/Punktfunk (GameStream plane). With Punktfunk the video timestamp is the host tick wall clock; with other hosts its meaning may differ, so any timestamp-based mode must be optional.
- Inspiration repos for client features: `derflacco/moonlight-android` (Artemide), `Nonary/Vibepollo`, `qiin2333/moonlight-vplus` (ideas only, not ported), and `punktfunk/punktfunk` (pacing logic, see `reference/punktfunk/`).
