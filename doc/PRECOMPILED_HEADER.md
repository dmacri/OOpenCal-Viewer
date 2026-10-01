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

## Notes for packaging

Nothing may modify the headers inside the package after the precompiled header has been built: clang validates their
modification times (squashfs keeps them, so the AppImage is fine).
