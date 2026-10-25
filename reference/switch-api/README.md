# Switch API reference (read-only)

Public headers copied from devkitPro/deko3d and switchbrew/libnx (licenses alongside) so the agent does not need the internet. They are NOT compiled; the GitHub Actions build uses the versions inside the devkitpro/devkita64 image.

- deko3d.h — GPU API (queues, fences, swapchain present)
- vi.h — `viGetDisplayVsyncEvent` (display vsync event, relevant to the vsync-timing question in docs/roadmap.md)
- native_window.h — window/presentation helpers
- counter.h — `armGetSystemTick` and friends (timing)

Full docs: https://switchbrew.github.io/libnx/ and https://github.com/devkitPro/deko3d
