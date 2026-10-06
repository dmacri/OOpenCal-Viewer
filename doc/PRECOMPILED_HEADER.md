# Precompiled header for on-the-fly compilation of models

When a model directory is opened for the first time, the Viewer compiles the model code into a dynamic library
(see [DEVELOPER_GUIDE_MODEL_LOADING.md](DEVELOPER_GUIDE_MODEL_LOADING.md)). A big part of that time is spent on parsing
the same heavy headers for every model (standard library, VTK, the templates of the Viewer, `Cell.h` of OOpenCAL).
The Viewer can precompile them once and reuse the result for every model.

The precompiled header is **optional and never required**. When it is missing, outdated, built by another compiler or rejected
by the compiler, models are compiled exactly as before.

## Measured effect

Compilation of the sample `Output` model (SciddicaT), `[METRIC] Module Compilation` reported by the Viewer, median of 3 runs
on a 1-core machine with warm file cache:

| Compiler                                   | Without | With precompiled header |
|--------------------------------------------|---------|-------------------------|
| system `g++-13`                            | 6.15 s  | 4.36 s                  |
| system `clang++-18`                        | 5.44 s  | 2.66 s                  |
| AppImage (bundled clang, mounted elsewhere) | 5.44 s  | 2.68 s                  |

The rest is instantiation of the templates for the model and code generation, which cannot be precompiled.
Disk usage: about 30 MB for clang, about 180 MB for g++ (a `.gch` stores much more than a `.pch`).

## How it works

- Every generated wrapper starts with `ModelLoader`'s preamble: if `OOpenCalViewerPrecompiled.h` is on the include path it is
  included (it contains exactly the headers the wrapper would include otherwise), if not, the headers are included one by one.
- The directory with the precompiled header is added to the include path **only when it is valid**, so a wrapper never
  silently depends on it.
- **clang** does not look for `.pch` files on `#include`, so `-include-pch <dir>/OOpenCalViewerPrecompiled.h.pch` is added.
- **g++** finds `OOpenCalViewerPrecompiled.h.gch` next to the header by itself.
- Inside the AppImage (`COMPILER_INCLUDEDIR` is set by `AppRun`) the clang file is built as *relocatable*
  (`-Xclang -relocatable-pch -isysroot <toolchain root>`) and used with the same `-isysroot`. The AppImage is mounted under a
  different path on every run, a normal `.pch` would refer to the paths of the build machine and be rejected.
- Files, per compiler family (clang and g++ results can live in the same directory):
  `OOpenCalViewerPrecompiled.h`, `OOpenCalViewerPrecompiled.h.pch` / `.gch`, `OOpenCalViewerPrecompiled.<clang|gcc>.manifest`.

### When it is used

The manifest written at build time must match the current compilation:

- same compiler (`--version`), same family,
- same compilation command (compiler, flags, `-std`, include paths, VTK flags; paths below the toolchain root are compared
  relative to it), so changing e.g. `OOPENCAL_DIR` in the settings switches the precompiled header off instead of silently
  using the `Cell.h` it was built with,
- none of the Viewer / OOpenCAL headers it was built from is missing or newer than it (g++ does not check that by itself).

The reason of every refusal is logged: `[PCH] Precompiled header in '...' is not used: ...`.
If the compiler still rejects the file, the compilation is repeated without it (`[PCH] ... retrying without it`).

## Building it

| Situation | What to do |
|-----------|------------|
| AppImage | Nothing. The release workflow runs `AppDir/AppRun --buildPrecompiledHeader` and `AppRun` exports `OOPENCAL_PRECOMPILED_HEADER_DIR`. |
| Development build | `cmake --build . --target precompiled-header` (directory: `<build dir>/precompiled-header`, or `-DOOPENCAL_PRECOMPILED_HEADER_DIR=<dir>`) |
| Manually | `OOpenCal-Viewer --buildPrecompiledHeader[=<dir>]` (no display needed; the directory defaults to the setting below) |

Rebuild it after changing the headers of the Viewer, the OOpenCAL `Cell.h` or the compiler. The build finishes with a self-test,
a result the compiler cannot load is deleted.

## Configuration

Like the other paths for compilation (`OOPENCAL_DIR`, `OOPENCAL_VIEWER_ROOT`), in order of priority:

1. value typed into the *Compilation settings* dialog (`OOPENCAL_PRECOMPILED_HEADER_DIR`),
2. environment variable `OOPENCAL_PRECOMPILED_HEADER_DIR`,
3. CMake variable `OOPENCAL_PRECOMPILED_HEADER_DIR` (default `<build dir>/precompiled-header`, empty disables).

To switch it off, point it at a directory which does not exist. Windows (MSVC) is not supported.

## Models built with `visualizer.sh` (plugins)

`visualizer.sh <model>` (OOpenCAL) does not use the compilation described above. It builds the model as a plugin with
`scripts/build_plugin.sh`, which configures the CMake project `examples/custom_model_plugin`. That build uses other flags
(`-std=gnu++23`, the definitions of VTK) and g++ accepts a precompiled header only when flags and macros match, so plugins
have their own precompiled header (`examples/custom_model_plugin/OOpenCalPluginPrecompiled.h`). It is built **by the same CMake
project** (`-DOOPENCAL_PLUGIN_PCH_ONLY=ON`), therefore its flags are identical to the flags of the plugins by construction.

- It is shared by all the models of one Viewer and lives in `<viewer>/build/plugin-precompiled-header/<key>/`. The key depends on
  the OOpenCAL directories, the compiler and the other CMake arguments, so different setups never share one.
- `visualizer.sh --build` (`scripts/prepare_and_build.sh`) prepares it right after building the Viewer, so even the first model uses it.
  If it does not exist yet, the first `build_plugin.sh` creates it (about 8 s, once). `visualizer.sh` itself needs no change.
- Every `build_plugin.sh` first runs `cmake --build` on it (0.3 s when it is up to date), so it is rebuilt when a header of the Viewer or of OOpenCAL changed.
- It is best effort: when it cannot be prepared the plugin is built as before. `OOPENCAL_NO_PRECOMPILED_HEADER=1` switches it off.
- Manually: `scripts/build_plugin.sh --prepare-precompiled-header -DOOPENCAL_DIR=<dir> -DOOPENCALVIEWER_DIR=<dir> --includes <OOpenCAL>/base`.

Plugin build time (`SciddicaT`, 1 core, warm cache, median of 3, whole `build_plugin.sh` without the one-time preparation):
g++-13 7.7 s → 5.8 s, clang++-18 6.5 s → 3.3 s. The header takes about 185 MB for g++ and much less for clang.

Where the time goes with g++ (`SciddicaT`): compiling the plugin 6.4 s → 4.3 s, while the CMake configuration of the plugin
(`find_package(VTK)`, 1.2–2 s) and linking (0.2 s) do not depend on the precompiled header. So the whole build gets about 25% faster, not 50%
as the on-the-fly compilation with clang does; the rest is instantiation of the templates for the model, which cannot be precompiled.
Checking that the header is up to date costs about 0.05 s.

To measure it, compare the `Plugin built in ... (with/without precompiled header)` lines printed by `build_plugin.sh`
(delete `lib*.so` of the model first, otherwise the plugin is not built again; `OOPENCAL_NO_PRECOMPILED_HEADER=1` switches the header off).
Timing a whole `visualizer.sh <model>` run also includes starting the Viewer, which is several seconds and varies by about 0.5 s from run to run.
The line `Plugin built in ... (...)` tells the truth: when the compiler does not use the header it says
`precompiled header NOT used by the compiler: <reason>` (g++ warns with `[-Winvalid-pch]`), and when clang cannot load it the build is repeated without it.

To check by hand that g++ uses the header, run in `<model>/build`:

```bash
F=CMakeFiles/<Cell>Plugin.dir/flags.make
grep CXX_FLAGS $F      # has to contain -include .../cmake_pch.hxx
eval c++ $(sed -n 's/^CXX_DEFINES = //p' $F) $(sed -n 's/^CXX_INCLUDES = //p' $F) $(sed -n 's/^CXX_FLAGS = //p' $F) -H -fsyntax-only Plugin_FullTemplate.cpp 2>&1 | head -3
```

The first line of the output starting with `!` means the header is used, `x` that the compiler rejected it (`make 2>&1 | grep -i gch` shows the reason),
and no such line at all (the list starts with `. .../iostream` and its children) that the build has no `-include`, i.e. no precompiled header.
Note that the reasons g++ gives do not always contain `not used because` (e.g. `created and used with different settings of -fpic`).

## Notes for packaging

Nothing may modify the headers inside the package after the precompiled header has been built: clang validates their
modification times (squashfs keeps them, so the AppImage is fine).
