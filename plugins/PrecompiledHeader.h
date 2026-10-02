/** @file PrecompiledHeader.h
 * @brief Support for the optional precompiled header that speeds up on-the-fly compilation of models.
 *
 * Every model directory is compiled into a plugin (.so) through a generated wrapper which includes the same
 * set of heavy headers (standard library, VTK, Viewer templates, OOpenCAL `Cell.h`). Parsing them is a big part of
 * the compilation time, so they can be precompiled once and reused by every model.
 *
 * This file contains the parts which depend neither on Qt nor on the rest of the Viewer (so they can be unit tested):
 * - text of the precompiled header and of the preamble of generated wrappers,
 * - detection of the compiler family (clang and g++ are handled differently),
 * - manifest stored next to the precompiled header, used to decide whether it may be used,
 * - command line flags which create / use the precompiled header.
 *
 * How the compilers use it:
 * - clang: `-include-pch <dir>/OOpenCalViewerPrecompiled.h.pch`. When the bundled toolchain of the AppImage is used
 *   (COMPILER_INCLUDEDIR is set) the file is built as relocatable (`-relocatable-pch -isysroot <root>`), because
 *   the AppImage is mounted in a different place on every run.
 * - g++: `OOpenCalViewerPrecompiled.h.gch` is found automatically when the wrapper includes
 *   `OOpenCalViewerPrecompiled.h` from a directory given with `-I`.
 *
 * The precompiled header is only an optimization: when it is missing, outdated or rejected by the compiler,
 * models are compiled exactly as if it did not exist. */

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace viz::plugins::pch
{
/// Name of the precompiled header; it is also the header which generated wrappers include.
inline constexpr char HEADER_FILE_NAME[] = "OOpenCalViewerPrecompiled.h";

/// Placeholder replacing the relocation root in the paths stored in the manifest.
inline constexpr char ROOT_PLACEHOLDER[] = "@ROOT@";

/// Version of the manifest format (increase it when the meaning of the stored data changes).
inline constexpr int MANIFEST_FORMAT = 1;

/// Compilers differ in the way they use precompiled headers.
enum class CompilerFamily
{
    Unknown,
    Clang,
    Gcc
};

/// Short lowercase name ("clang", "gcc", "unknown") used in file names and in the manifest.
const char* toString(CompilerFamily family);

/// Result of asking the compiler for its version.
struct CompilerIdentity
{
    CompilerFamily family = CompilerFamily::Unknown;
    std::string versionLine; ///< First line of the `--version` output
};

/// Recognize the compiler from the output of `<compiler> --version`.
CompilerIdentity parseCompilerIdentity(const std::string& versionOutput);

/// Run `<compiler> --version` and recognize the compiler. The result is cached per compiler path.
/// Returns an unknown compiler on Windows (precompiled headers are not supported there).
CompilerIdentity identifyCompiler(const std::string& compilerPath);

/// `#include` directives which every generated wrapper needs before the model header.
std::string commonIncludeDirectives();

/// Content of OOpenCalViewerPrecompiled.h (what is precompiled).
std::string headerFileContent();

/// Text placed at the top of every generated wrapper: includes the precompiled header if it is on the include path,
/// otherwise includes the same headers directly.
std::string wrapperPreamble();

/// File name of the compiled artifact: OOpenCalViewerPrecompiled.h.pch (clang) or .gch (g++).
std::string artifactFileName(CompilerFamily family);

/// File name of the manifest describing the artifact for the given compiler family.
std::string manifestFileName(CompilerFamily family);

/// Remove flags which matter only for linking (`-shared`), so the same flags can be used to build the header.
std::string stripLinkerOnlyFlags(const std::string& flags);

/// Root directory of the bundled toolchain (parent of COMPILER_INCLUDEDIR) or empty string when a system compiler is used.
/// Paths below it are stored relative to it, so the precompiled header survives moving the whole toolchain.
std::string relocationRootFromEnvironment();

/// Replace `root` with ROOT_PLACEHOLDER in `text` (does nothing if `root` is empty).
std::string normalizePaths(std::string text, const std::string& root);

/// Reverse of normalizePaths().
std::string denormalizePaths(std::string text, const std::string& root);

/// Quote a path for the command line when needed.
std::string quote(const std::string& path);

/// Flags (with leading space) which turn the compilation into building the precompiled header.
std::string buildFlags(CompilerFamily family,
                       const std::string& header,
                       const std::string& artifact,
                       const std::string& dependencyFile,
                       const std::string& root);

/// Flags (with leading space) which make the compiler use the precompiled header from `directory`.
std::string usageFlags(CompilerFamily family, const std::string& directory, const std::string& root);

/// Flags (with leading space) of the self-test which checks that the freshly built precompiled header can be loaded.
std::string probeFlags(CompilerFamily family,
                       const std::string& directory,
                       const std::string& root,
                       const std::string& probeSource);

/// Like usageFlags(), but empty when no artifact for the family exists in `directory` (used for previews in the GUI).
std::string previewUsageFlags(const std::string& directory, CompilerFamily family, const std::string& root);

/// Parse a dependency file in the Makefile format written by `-MD -MF` (continuation lines, escaped spaces).
std::vector<std::string> parseMakeDependencies(const std::string& dependencyFileText);

/// Keep only the (absolute, lexically normalized, unique) dependencies located below one of `roots`.
std::vector<std::string> filterProjectDependencies(const std::vector<std::string>& dependencies,
                                                   const std::vector<std::string>& roots);

/// Description of a built precompiled header, stored next to it.
struct Manifest
{
    int format = MANIFEST_FORMAT;
    std::string family;                    ///< toString(CompilerFamily) of the compiler which built it
    std::string compilerVersion;           ///< first line of `<compiler> --version`
    std::string command;                   ///< normalized compilation command (without the file specific parts)
    std::vector<std::string> dependencies; ///< normalized paths of headers which may change during development
};

std::string serializeManifest(const Manifest& manifest);

/// Returns nothing if the text is not a manifest in the supported format.
std::optional<Manifest> parseManifest(const std::string& text);

/// Verdict whether the precompiled header from a directory may be used.
struct Usability
{
    bool usable = false;
    std::string reason;       ///< Why it cannot be used
    std::string headerPath;   ///< <dir>/OOpenCalViewerPrecompiled.h
    std::string artifactPath; ///< <dir>/OOpenCalViewerPrecompiled.h.pch (or .gch)
};

/** @brief Decide whether the precompiled header stored in `directory` may be used right now.
 * @param directory Directory with the header, the artifact and the manifest
 * @param compiler Compiler which is going to use it
 * @param expectedCommand Normalized command the compiler would use to build it right now (see Manifest::command)
 * @param root Relocation root used to restore the paths of dependencies stored in the manifest */
Usability evaluate(const std::string& directory,
                   const CompilerIdentity& compiler,
                   const std::string& expectedCommand,
                   const std::string& root);
} // namespace viz::plugins::pch
