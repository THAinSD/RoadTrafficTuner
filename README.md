# Road Traffic Tuner

An X-Plane 12 plugin that rebalances X-Plane's own road traffic between road
types. For example, it can make residential streets mostly empty while
leaving arterials and freeways busy.

## How it works

X-Plane's default road files (in `Resources/default scenery/1000 roads`) give
every car lane a "spawn ratio": roughly, cars per metre of road. There are two:
`roads.net`, used by most of the world, and `roads_EU.net`, used by European
scenery. By
default a quiet residential street gets about the same density per lane as a
busy arterial.

The plugin builds a Custom Scenery override pack,
`Custom Scenery/zzz_Road_Traffic_Tuner`, which contains:

- copies of the road files you choose (one or both) in which only the spawn
  ratios are scaled, for four road families (Local, Secondary, Primary,
  Highway);
- copies of the objects and textures those files refer to by relative path
  (about 180 files for both);
- a `library.txt` that points `lib/g10/roads.net` and/or `lib/g10/roads_EU.net`
  at the edited copies.

X-Plane's own files are never modified. The overall number of cars is still set
by X-Plane's graphics settings; the plugin only changes how those cars are
shared between road types. Changes take effect the next time X-Plane starts.

When X-Plane updates either road file, the plugin notices at startup and offers
to rebuild the pack with your saved settings.

## Using it

1. Open **Plugins > Road Traffic Tuner > Open...** (**About...** in the same menu
   explains what the plugin does).
2. Tick the road networks to change: **Rest of the world** (roads.net) and/or
   **Europe** (roads_EU.net).
3. Set the four sliders. The right-hand column shows the typical spacing between
   cars in a lane, default -> new.
4. Press **Build pack**, then restart X-Plane.

**Remove pack** (press twice to confirm) returns X-Plane to default traffic
after a restart. **Reset to defaults** sets the sliders and checkboxes back to the
plugin's starting values.

If the window lists other packs that also replace the road network, the one
highest in `Custom Scenery/scenery_packs.ini` wins.

## Building on a Mac

1. **Install Apple's command-line tools.** In Terminal, run
   `xcode-select --install` (skip this if they are already installed).
2. **Install CMake.** Either `brew install cmake` (Homebrew), or the macOS
   installer from cmake.org.
3. **Get the X-Plane SDK.** It is published on Laminar's GitHub at
   github.com/X-Plane/xpsdk under Releases (built and tested with
   `xpsdk_4.4.0-r2`). Download the release zip, unzip it, and rename the
   unzipped folder (the one containing `CHeaders` and `Libraries`) to `SDK`.
   Put it either inside this folder (`RoadTrafficTuner/SDK`) or next to it
   (in the folder that contains `RoadTrafficTuner`, where other projects can share it). To
   keep it anywhere else, add `-DXPSDK=/path/to/SDK` to the first cmake command.
4. **Build.** In Terminal:
   ```
   cd <path to>/RoadTrafficTuner
   cmake -S . -B build
   cmake --build build
   ```
   The plugin appears at `build/RoadTrafficTuner/mac_x64/RoadTrafficTuner.xpl`.
   It is a universal binary for Apple silicon and Intel Macs.
5. **Install.** Copy the whole `build/RoadTrafficTuner` folder into
   `X-Plane 12/Resources/plugins/`.
6. **Check it loaded.** Start X-Plane and look for **Road Traffic Tuner** under
   the Plugins menu. Its messages in `X-Plane 12/Log.txt` start with
   `Road Traffic Tuner:`.

A plugin you compile yourself is not quarantined by macOS. Copies downloaded by
other people are, unless the plugin is signed and notarized with an Apple
Developer account.

## Building on Windows

1. Install Visual Studio (2022 or newer) with the **Desktop development with
   C++** workload, which includes CMake.
2. Put the SDK in place as described above (inside this folder or next to it).
3. From the Start menu, open **x64 Native Tools Command Prompt** for your
   Visual Studio version, then run:
   ```
   cd /d <path to>\RoadTrafficTuner
   cmake -S . -B build
   cmake --build build --config Release
   ```
   The plugin appears at `build\RoadTrafficTuner\win_x64\RoadTrafficTuner.xpl`.
   It is linked with the static C++ runtime, so users do not need a separate
   Visual C++ redistributable.

## Building on Linux

There are two ways, depending on what the build is for.

**For your own use**, the CMake build works as on the Mac (with the SDK in
place as described above):
```
cmake -S . -B build
cmake --build build
```
The plugin appears at `build/RoadTrafficTuner/lin_x64/RoadTrafficTuner.xpl`.
It depends on the build machine's C library and C++ runtime, so it may not run
on older distributions.

**For distribution**, use the release script, which builds the portable Linux
plugin included in published releases:
```
pip install ziglang        # once, if Zig is not already installed
scripts/build_linux_release.sh
```
The plugin appears at
`build-linux/RoadTrafficTuner/lin_x64/RoadTrafficTuner.xpl`. It targets glibc
2.17, so it runs on essentially any Linux that X-Plane 12 supports, bundles the
C++ runtime, and exports only X-Plane's five plugin functions
(`scripts/linux_exports.map`). The script checks those exports when it
finishes. Zig can also build Linux binaries from macOS, but the script has
only been tested on Linux.

The published Linux build has not yet been tested in X-Plane on Linux; reports
are welcome.

## Licensing notes

- Road Traffic Tuner is released under the MIT License; see `LICENSE`.
- The X-Plane SDK is Laminar Research's, under its own permissive licence
  (`license.txt` in the SDK). It is not included here; download it as
  described above.
- The plugin builds its override pack from each user's own X-Plane files, so
  no Laminar files are distributed with the plugin.
- Road Traffic Tuner is not made or endorsed by Laminar Research.

## Source layout

- `src/rtt_core.*` - all roads.net and file work, with no X-Plane dependencies
- `src/rtt_plugin.cpp` - menu, window and startup check (X-Plane SDK)
- `tests/test_core.cpp` - tests for the core, not part of the plugin build
- `scripts/build_linux_release.sh` - builds the portable Linux release
