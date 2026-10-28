# Windows: portable package with a built-in compiler (MinGW-w64)

The counterpart of the Linux "All-in-One" AppImage (issue #129): one ZIP, no installation, and models (C++ headers) are compiled
on the user's machine by a compiler which is inside the package.

The existing Visual Studio build (`windows-build.yml`) is untouched. It cannot ship its compiler (the MSVC licence does not allow
redistributing it), which is why this variant is built with **MinGW-w64 (UCRT)**, which can be redistributed freely.

## For the user

1. Extract the whole ZIP to a directory **without spaces** in the path (e.g. `C:\OOpenCal-Viewer`).
2. Double-click `OOpenCal-Viewer.bat` (arguments are passed on: `OOpenCal-Viewer.bat C:\models\Ball`).
3. Open a model directory: the model is compiled by `compiler\bin\g++.exe`.

The console window stays open on purpose: it shows the output of the compiler.

## How it is made

| What | Where |
|---|---|
| CI workflow | `.github/workflows/windows-mingw-all-in-one.yml` |
| Packaging (DLLs, Qt plugins, trimmed toolchain, headers, import libraries, notices) | `scripts/windows-mingw/package.sh` |
| Headers needed to compile models | `scripts/windows-mingw/package-headers.sh` |
| Environment + launcher | `scripts/windows-mingw/compiler-env.bat`, `OOpenCal-Viewer.bat` |
| Check of the finished package | `scripts/windows-mingw/smoke-test/` |

Qt 6 and VTK come as prebuilt **MSYS2 UCRT64** packages, so they have the same C++ ABI as the compiler which is shipped (no
building of VTK from sources). The program is built with that very toolchain, which is what makes models compiled later compatible
with it.

Layout of the package:

```
OOpenCal-Viewer.bat  compiler-env.bat  README.txt  THIRD-PARTY-NOTICES.txt
bin/        OOpenCal-Viewer.exe, Qt + VTK + runtime DLLs, plugins/ (Qt)
compiler/   MinGW-w64 g++ (bin, lib, include, ...)
include/    oopencal-viewer/ (OOPENCAL_VIEWER_ROOT), OOpenCAL/base (under OOPENCAL_DIR), vtk/ (VTK_INCLUDES)
lib/        libOOpenCal-Viewer.dll.a, VTK import libraries, plugin-libs.rsp
```

### Environment (`compiler-env.bat`)

The same idea as `AppRun` in the AppImage. The program already reads these variables:

| Variable | Value |
|---|---|
| `OOPENCAL_COMPILER` | `<package>/compiler/bin/g++.exe` |
| `OOPENCAL_DIR` | `<package>/include` |
| `OOPENCAL_VIEWER_ROOT` | `<package>/include/oopencal-viewer` |
| `VTK_INCLUDES` | `<package>/include/vtk` |
| `OOPENCAL_LINK_FLAGS` | **new**, see below |

### Why models are linked against the executable

On Linux a model is a shared object with unresolved symbols which are resolved when it is loaded (`-rdynamic` exports them from the
executable). A Windows DLL cannot be linked with unresolved symbols. Therefore, for MinGW:

* `CMakeLists.txt` exports the symbols of the executable and produces `libOOpenCal-Viewer.dll.a` (`ENABLE_EXPORTS`,
  `--export-all-symbols`);
* the compile command ends with `OOPENCAL_LINK_FLAGS` (`CppModuleBuilder::buildCompileCommand`):
  `-L<package>/lib -lOOpenCal-Viewer @<package>/lib/plugin-libs.rsp` (the import libraries of the shipped VTK DLLs);
* the default compilation flags on MinGW are `-shared -DBUILDING_DLL` (`CompilationConfig`), on MSVC `/LD ...` as before.

`OOPENCAL_LINK_FLAGS` is empty on other platforms, so nothing changes there.

## Checking a package

`scripts/windows-mingw/smoke-test/run-smoke-test.ps1 -Package <dir>` runs with a PATH which has nothing but the Windows directories
and checks that (1) the program starts, (2) the bundled g++ builds a model plugin with the very command line which the program
builds, (3) the program loads that plugin. The workflow runs it; it can also be run by hand on a clean Windows machine.

## Known limitations

* **Paths with spaces**: the Viewer splits `VTK_INCLUDES` on spaces, so the package must be extracted to a path without spaces
  (`compiler-env.bat` warns about it).
* **No precompiled header** on Windows (`selectPrecompiledHeader` / `buildPrecompiledHeader` are disabled under `_WIN32`), so
  compiling a model is slower than in the AppImage.
* **Singletons defined in headers are per module.** A model DLL gets its own copy of e.g. `PerformanceMetrics::instance()`
  (verified: with MinGW the executable and the DLL do not share it, unlike ELF with `-rdynamic`). The performance metrics collected
  by code which runs inside a model (e.g. `ModelReader` data loading) are therefore not added to the summary of the executable.
  Rendering is not affected. Fixing it means defining such singletons in a `.cpp` of the executable.
* A PE file can export at most 65535 symbols; the workflow prints how many the executable exports.
* The package is large (compiler + Qt + VTK, several hundred MB unpacked).

## Licences

The compiler and libraries are unmodified MSYS2 packages; `THIRD-PARTY-NOTICES.txt` in the package lists them with versions and
licences. Code built by the bundled GCC is not covered by the GPL (GCC Runtime Library Exception).
