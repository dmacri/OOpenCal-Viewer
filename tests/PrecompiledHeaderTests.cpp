#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/PrecompiledHeaderCommand.h"
#include "plugins/PrecompiledHeader.h"

namespace fs = std::filesystem;
namespace pch = viz::plugins::pch;

namespace
{
/// Temporary directory removed in the destructor
class TempDirectory
{
public:
    TempDirectory()
    {
        static int counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() / ("oopencal-pch-tests-" + std::to_string(stamp) + "-" + std::to_string(++counter));
        fs::create_directories(path_);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void writeFile(const fs::path& path, const std::string& content)
{
    std::ofstream(path, std::ios::binary) << content;
}

pch::CompilerIdentity clangIdentity()
{
    return pch::parseCompilerIdentity("Ubuntu clang version 18.1.3 (1ubuntu1)\nTarget: x86_64-pc-linux-gnu\n");
}

/// Creates a complete precompiled header for clang in `directory`. `dependency` is a header it was built from and it is
/// made older than the artifact.
pch::Manifest createValidDirectory(const fs::path& directory, const fs::path& dependency, const std::string& dependencyText)
{
    writeFile(dependency, "// header\n");
    writeFile(directory / pch::HEADER_FILE_NAME, pch::headerFileContent());
    writeFile(directory / pch::artifactFileName(pch::CompilerFamily::Clang), "pch");

    pch::Manifest manifest;
    manifest.family = "clang";
    manifest.compilerVersion = clangIdentity().versionLine;
    manifest.command = "clang++ -fPIC -std=c++23";
    manifest.dependencies = {dependencyText};
    writeFile(directory / pch::manifestFileName(pch::CompilerFamily::Clang), pch::serializeManifest(manifest));

    const auto artifactTime = fs::last_write_time(directory / pch::artifactFileName(pch::CompilerFamily::Clang));
    fs::last_write_time(dependency, artifactTime - std::chrono::hours(1));
    return manifest;
}
} // namespace


TEST(PrecompiledHeaderTests, ParseCompilerIdentityRecognizesClangAndGcc)
{
    const auto clang = pch::parseCompilerIdentity("Ubuntu clang version 18.1.3 (1ubuntu1)\nTarget: x86_64-pc-linux-gnu\n");
    EXPECT_EQ(clang.family, pch::CompilerFamily::Clang);
    EXPECT_EQ(clang.versionLine, "Ubuntu clang version 18.1.3 (1ubuntu1)");

    EXPECT_EQ(pch::parseCompilerIdentity("Apple clang version 15.0.0\n").family, pch::CompilerFamily::Clang);

    const auto gcc = pch::parseCompilerIdentity("g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0\nCopyright (C) 2023 Free Software Foundation, Inc.\n");
    EXPECT_EQ(gcc.family, pch::CompilerFamily::Gcc);
    EXPECT_EQ(gcc.versionLine, "g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0");

    // the "c++" alias does not say g++ in the first line
    EXPECT_EQ(pch::parseCompilerIdentity("c++ (Ubuntu 13.3.0) 13.3.0\nCopyright (C) 2023 Free Software Foundation, Inc.\n").family,
              pch::CompilerFamily::Gcc);

    EXPECT_EQ(pch::parseCompilerIdentity("sh: 1: nothing: not found\n").family, pch::CompilerFamily::Unknown);
    EXPECT_EQ(pch::parseCompilerIdentity("").family, pch::CompilerFamily::Unknown);
}

TEST(PrecompiledHeaderTests, WrapperPreambleIncludesPrecompiledHeaderFirstAndFallsBackToDirectIncludes)
{
    const std::string preamble = pch::wrapperPreamble();

    EXPECT_NE(preamble.find("__has_include(\"OOpenCalViewerPrecompiled.h\")"), std::string::npos);
    EXPECT_NE(preamble.find("#ifndef OOPENCAL_VIEWER_PRECOMPILED_HEADER_USED"), std::string::npos);

    // the precompiled header has to be the first thing which is included (required by g++)
    const auto precompiledInclude = preamble.find("include \"OOpenCalViewerPrecompiled.h\"");
    const auto firstDirectInclude = preamble.find("#include <iostream>");
    ASSERT_NE(precompiledInclude, std::string::npos);
    ASSERT_NE(firstDirectInclude, std::string::npos);
    EXPECT_LT(precompiledInclude, firstDirectInclude);

    // the fallback contains exactly what the precompiled header contains
    EXPECT_NE(preamble.find(pch::commonIncludeDirectives()), std::string::npos);
    EXPECT_NE(pch::headerFileContent().find(pch::commonIncludeDirectives()), std::string::npos);
    EXPECT_NE(pch::commonIncludeDirectives().find("#include \"visualiserProxy/SceneWidgetVisualizerProxy.h\""), std::string::npos);
}

TEST(PrecompiledHeaderTests, HeaderFileContentHasIncludeGuard)
{
    const std::string content = pch::headerFileContent();
    EXPECT_NE(content.find("#ifndef OOPENCAL_VIEWER_PRECOMPILED_H"), std::string::npos);
    EXPECT_NE(content.find("#define OOPENCAL_VIEWER_PRECOMPILED_H"), std::string::npos);
    EXPECT_NE(content.rfind("#endif"), std::string::npos);
}

TEST(PrecompiledHeaderTests, FileNamesDependOnCompilerFamily)
{
    EXPECT_EQ(pch::artifactFileName(pch::CompilerFamily::Clang), "OOpenCalViewerPrecompiled.h.pch");
    EXPECT_EQ(pch::artifactFileName(pch::CompilerFamily::Gcc), "OOpenCalViewerPrecompiled.h.gch");
    EXPECT_EQ(pch::artifactFileName(pch::CompilerFamily::Unknown), "");
    EXPECT_EQ(pch::manifestFileName(pch::CompilerFamily::Clang), "OOpenCalViewerPrecompiled.clang.manifest");
    EXPECT_EQ(pch::manifestFileName(pch::CompilerFamily::Gcc), "OOpenCalViewerPrecompiled.gcc.manifest");
}

TEST(PrecompiledHeaderTests, ParseMakeDependenciesHandlesContinuationsAndEscapedSpaces)
{
    const std::string text = "dir/out.pch: header.h \\\n  /usr/include/a.h \\\n  /path\\ with\\ space/b.h\n";
    const std::vector<std::string> expected = {"header.h", "/usr/include/a.h", "/path with space/b.h"};
    EXPECT_EQ(pch::parseMakeDependencies(text), expected);

    const std::string windowsLineEndings = "out.pch: a.h \\\r\n b.h\r\n";
    EXPECT_EQ(pch::parseMakeDependencies(windowsLineEndings), (std::vector<std::string>{"a.h", "b.h"}));

    EXPECT_TRUE(pch::parseMakeDependencies("").empty());
    EXPECT_TRUE(pch::parseMakeDependencies("no rule here").empty());
}

TEST(PrecompiledHeaderTests, FilterProjectDependenciesKeepsOnlyAbsolutePathsBelowRoots)
{
    const std::vector<std::string> dependencies = {
        "/a/viewer/x.h",
        "/a/viewer/../viewer/y.h",
        "/a/viewer/x.h",
        "/a/viewerother/z.h",
        "relative/w.h",
        "/a/lib/OOpenCAL/base/Cell.h",
        "/usr/include/c++/13/vector",
    };
    const std::vector<std::string> expected = {"/a/lib/OOpenCAL/base/Cell.h", "/a/viewer/x.h", "/a/viewer/y.h"};
    EXPECT_EQ(pch::filterProjectDependencies(dependencies, {"/a/viewer/", "/a/lib/OOpenCAL", ""}), expected);
    EXPECT_TRUE(pch::filterProjectDependencies(dependencies, {}).empty());
}

TEST(PrecompiledHeaderTests, ManifestRoundTrip)
{
    pch::Manifest manifest;
    manifest.family = "clang";
    manifest.compilerVersion = "Ubuntu clang version 18.1.3 (1ubuntu1)";
    manifest.command = "@ROOT@/bin/clang++ -fPIC -std=c++23 -I\"@ROOT@/include\" -DX=1";
    manifest.dependencies = {"@ROOT@/include/a.h", "/other/b.h"};

    const auto parsed = pch::parseManifest(pch::serializeManifest(manifest));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->format, pch::MANIFEST_FORMAT);
    EXPECT_EQ(parsed->family, manifest.family);
    EXPECT_EQ(parsed->compilerVersion, manifest.compilerVersion);
    EXPECT_EQ(parsed->command, manifest.command);
    EXPECT_EQ(parsed->dependencies, manifest.dependencies);
}

TEST(PrecompiledHeaderTests, ManifestRejectsUnknownFormatAndIncompleteContent)
{
    EXPECT_FALSE(pch::parseManifest("").has_value());
    EXPECT_FALSE(pch::parseManifest("format=999\nfamily=clang\ncommand=x\n").has_value());
    EXPECT_FALSE(pch::parseManifest("format=1\nfamily=clang\n").has_value());      // no command
    EXPECT_FALSE(pch::parseManifest("format=1\ncommand=x\n").has_value());         // no family
    EXPECT_FALSE(pch::parseManifest("format=abc\nfamily=clang\ncommand=x\n").has_value());
    EXPECT_TRUE(pch::parseManifest("# comment\r\nformat=1\r\nfamily=gcc\r\ncommand=x\r\n").has_value());
}

TEST(PrecompiledHeaderTests, NormalizePathsIsReversibleAndHarmlessWithoutRoot)
{
    const std::string command = "/tmp/.mount_a/compiler/bin/clang++ -I\"/tmp/.mount_a/compiler/include\"";
    const std::string normalized = pch::normalizePaths(command, "/tmp/.mount_a/compiler");
    EXPECT_EQ(normalized, "@ROOT@/bin/clang++ -I\"@ROOT@/include\"");
    EXPECT_EQ(pch::denormalizePaths(normalized, "/tmp/.mount_a/compiler"), command);

    // another mount point gives the same normalized text
    EXPECT_EQ(pch::normalizePaths("/tmp/.mount_b/compiler/bin/clang++ -I\"/tmp/.mount_b/compiler/include\"", "/tmp/.mount_b/compiler"),
              normalized);

    EXPECT_EQ(pch::normalizePaths(command, ""), command);
}

TEST(PrecompiledHeaderTests, StripLinkerOnlyFlagsRemovesShared)
{
    EXPECT_EQ(pch::stripLinkerOnlyFlags("-shared -fPIC"), "-fPIC");
    EXPECT_EQ(pch::stripLinkerOnlyFlags("-fPIC -shared -O2"), "-fPIC -O2");
    EXPECT_EQ(pch::stripLinkerOnlyFlags("-fPIC"), "-fPIC");
    EXPECT_EQ(pch::stripLinkerOnlyFlags(""), "");
}

TEST(PrecompiledHeaderTests, QuoteOnlyWhenNeeded)
{
    EXPECT_EQ(pch::quote("/a/b"), "/a/b");
    EXPECT_EQ(pch::quote("/a b/c"), "\"/a b/c\"");
}

TEST(PrecompiledHeaderTests, UsageFlagsDifferBetweenClangAndGcc)
{
    EXPECT_EQ(pch::usageFlags(pch::CompilerFamily::Clang, "/p", ""),
              " -I/p -include-pch /p/OOpenCalViewerPrecompiled.h.pch");
    EXPECT_EQ(pch::usageFlags(pch::CompilerFamily::Clang, "/p", "/app/compiler"),
              " -I/p -include-pch /p/OOpenCalViewerPrecompiled.h.pch -isysroot /app/compiler");
    // g++ finds the .gch by itself when the header is included from a directory given with -I
    EXPECT_EQ(pch::usageFlags(pch::CompilerFamily::Gcc, "/p", ""), " -I/p");
}

TEST(PrecompiledHeaderTests, BuildFlagsMakeClangPrecompiledHeaderRelocatableOnlyWithRoot)
{
    const std::string withRoot = pch::buildFlags(pch::CompilerFamily::Clang, "/p/h.h", "/p/h.h.pch", "/p/h.d", "/app/compiler");
    EXPECT_NE(withRoot.find("-x c++-header"), std::string::npos);
    EXPECT_NE(withRoot.find("-Xclang -relocatable-pch -isysroot /app/compiler"), std::string::npos);
    EXPECT_NE(withRoot.find("-MD -MF /p/h.d /p/h.h -o /p/h.h.pch"), std::string::npos);

    const std::string withoutRoot = pch::buildFlags(pch::CompilerFamily::Clang, "/p/h.h", "/p/h.h.pch", "/p/h.d", "");
    EXPECT_EQ(withoutRoot.find("relocatable"), std::string::npos);

    const std::string gcc = pch::buildFlags(pch::CompilerFamily::Gcc, "/p/h.h", "/p/h.h.gch", "/p/h.d", "/ignored");
    EXPECT_EQ(gcc.find("relocatable"), std::string::npos);
    EXPECT_EQ(gcc.find("isysroot"), std::string::npos);
}

TEST(PrecompiledHeaderTests, ProbeForGccFailsOnInvalidPrecompiledHeader)
{
    const std::string gcc = pch::probeFlags(pch::CompilerFamily::Gcc, "/p", "", "/p/probe.cpp");
    EXPECT_NE(gcc.find("-Winvalid-pch -Werror=invalid-pch"), std::string::npos);
    EXPECT_NE(gcc.find("-fsyntax-only /p/probe.cpp"), std::string::npos);

    const std::string clang = pch::probeFlags(pch::CompilerFamily::Clang, "/p", "", "/p/probe.cpp");
    EXPECT_EQ(clang.find("invalid-pch"), std::string::npos);
    EXPECT_NE(clang.find("-include-pch"), std::string::npos);
}

TEST(PrecompiledHeaderTests, PreviewUsageFlagsAreEmptyUntilTheArtifactExists)
{
    TempDirectory temp;
    EXPECT_EQ(pch::previewUsageFlags(temp.path().string(), pch::CompilerFamily::Clang, ""), "");
    EXPECT_EQ(pch::previewUsageFlags("", pch::CompilerFamily::Clang, ""), "");

    writeFile(temp.path() / pch::artifactFileName(pch::CompilerFamily::Clang), "pch");
    EXPECT_NE(pch::previewUsageFlags(temp.path().string(), pch::CompilerFamily::Clang, ""), "");
    EXPECT_EQ(pch::previewUsageFlags(temp.path().string(), pch::CompilerFamily::Gcc, ""), "");
    EXPECT_EQ(pch::previewUsageFlags(temp.path().string(), pch::CompilerFamily::Unknown, ""), "");
}

#ifndef _WIN32
TEST(PrecompiledHeaderTests, RelocationRootIsParentOfBundledIncludeDirectory)
{
    const char* previous = std::getenv("COMPILER_INCLUDEDIR");
    const std::string saved = previous ? previous : "";

    setenv("COMPILER_INCLUDEDIR", "/tmp/.mount_x/compiler/include", 1);
    EXPECT_EQ(pch::relocationRootFromEnvironment(), "/tmp/.mount_x/compiler");

    setenv("COMPILER_INCLUDEDIR", "/tmp/.mount_x/compiler/include/", 1);
    EXPECT_EQ(pch::relocationRootFromEnvironment(), "/tmp/.mount_x/compiler");

    unsetenv("COMPILER_INCLUDEDIR");
    EXPECT_EQ(pch::relocationRootFromEnvironment(), "");

    if (previous)
        setenv("COMPILER_INCLUDEDIR", saved.c_str(), 1);
}
#endif

TEST(PrecompiledHeaderTests, EvaluateAcceptsCompleteUpToDateHeader)
{
    TempDirectory temp;
    const fs::path dependency = temp.path() / "dep.h";
    const auto manifest = createValidDirectory(temp.path(), dependency, dependency.string());

    const auto usability = pch::evaluate(temp.path().string(), clangIdentity(), manifest.command, "");
    EXPECT_TRUE(usability.usable) << usability.reason;
    EXPECT_EQ(usability.artifactPath, (temp.path() / "OOpenCalViewerPrecompiled.h.pch").string());
    EXPECT_EQ(usability.headerPath, (temp.path() / "OOpenCalViewerPrecompiled.h").string());
}

TEST(PrecompiledHeaderTests, EvaluateRestoresRelocatedDependencies)
{
    TempDirectory temp;
    const fs::path dependency = temp.path() / "dep.h";
    // the manifest stores the path relative to the (moved) toolchain root
    const auto manifest = createValidDirectory(temp.path(), dependency, std::string(pch::ROOT_PLACEHOLDER) + "/dep.h");

    EXPECT_TRUE(pch::evaluate(temp.path().string(), clangIdentity(), manifest.command, temp.path().string()).usable);

    const auto movedElsewhere = pch::evaluate(temp.path().string(), clangIdentity(), manifest.command, (temp.path() / "nowhere").string());
    EXPECT_FALSE(movedElsewhere.usable);
}

TEST(PrecompiledHeaderTests, EvaluateRejectsMissingParts)
{
    TempDirectory temp;
    const fs::path dependency = temp.path() / "dep.h";
    const auto manifest = createValidDirectory(temp.path(), dependency, dependency.string());
    const auto identity = clangIdentity();

    EXPECT_FALSE(pch::evaluate("", identity, manifest.command, "").usable);
    EXPECT_FALSE(pch::evaluate(temp.path().string(), pch::CompilerIdentity{}, manifest.command, "").usable);

    // built for clang only
    const auto gcc = pch::parseCompilerIdentity("g++ (Ubuntu 13.3.0) 13.3.0\nCopyright (C) 2023 Free Software Foundation, Inc.\n");
    EXPECT_FALSE(pch::evaluate(temp.path().string(), gcc, manifest.command, "").usable);

    fs::remove(temp.path() / pch::manifestFileName(pch::CompilerFamily::Clang));
    EXPECT_FALSE(pch::evaluate(temp.path().string(), identity, manifest.command, "").usable);

    writeFile(temp.path() / pch::manifestFileName(pch::CompilerFamily::Clang), "garbage");
    EXPECT_FALSE(pch::evaluate(temp.path().string(), identity, manifest.command, "").usable);

    fs::remove(temp.path() / pch::artifactFileName(pch::CompilerFamily::Clang));
    EXPECT_FALSE(pch::evaluate(temp.path().string(), identity, manifest.command, "").usable);

    fs::remove(temp.path() / pch::HEADER_FILE_NAME);
    EXPECT_FALSE(pch::evaluate(temp.path().string(), identity, manifest.command, "").usable);
}

TEST(PrecompiledHeaderTests, EvaluateRejectsDifferentCompilerOrSettings)
{
    TempDirectory temp;
    const fs::path dependency = temp.path() / "dep.h";
    const auto manifest = createValidDirectory(temp.path(), dependency, dependency.string());

    const auto otherVersion = pch::parseCompilerIdentity("Ubuntu clang version 19.0.0 (1ubuntu1)\n");
    const auto differentCompiler = pch::evaluate(temp.path().string(), otherVersion, manifest.command, "");
    EXPECT_FALSE(differentCompiler.usable);
    EXPECT_NE(differentCompiler.reason.find("different compiler"), std::string::npos);

    const auto differentSettings = pch::evaluate(temp.path().string(), clangIdentity(), manifest.command + " -DOTHER", "");
    EXPECT_FALSE(differentSettings.usable);
    EXPECT_NE(differentSettings.reason.find("settings"), std::string::npos);
}

TEST(PrecompiledHeaderTests, EvaluateRejectsHeadersChangedAfterTheBuild)
{
    TempDirectory temp;
    const fs::path dependency = temp.path() / "dep.h";
    const auto manifest = createValidDirectory(temp.path(), dependency, dependency.string());
    const fs::path artifact = temp.path() / pch::artifactFileName(pch::CompilerFamily::Clang);

    // a header edited after the precompiled header was built (g++ would not notice it by itself)
    fs::last_write_time(dependency, fs::last_write_time(artifact) + std::chrono::hours(1));
    const auto outdated = pch::evaluate(temp.path().string(), clangIdentity(), manifest.command, "");
    EXPECT_FALSE(outdated.usable);
    EXPECT_NE(outdated.reason.find("newer"), std::string::npos);

    // a header which disappeared
    fs::remove(dependency);
    const auto missing = pch::evaluate(temp.path().string(), clangIdentity(), manifest.command, "");
    EXPECT_FALSE(missing.usable);
    EXPECT_NE(missing.reason.find("missing"), std::string::npos);
}

TEST(PrecompiledHeaderTests, FindBuildPrecompiledHeaderRequestRecognizesOnlyTheExactOption)
{
    const char* normalStart[] = {"app", "/some/model/dir", "--autoPlay"};
    EXPECT_FALSE(viz::cli::findBuildPrecompiledHeaderRequest(3, normalStart).has_value());

    const char* withoutDirectory[] = {"app", "--buildPrecompiledHeader"};
    const auto bare = viz::cli::findBuildPrecompiledHeaderRequest(2, withoutDirectory);
    ASSERT_TRUE(bare.has_value());
    EXPECT_EQ(*bare, "");

    const char* withDirectory[] = {"app", "--autoPlay", "--buildPrecompiledHeader=/a dir/with spaces"};
    const auto given = viz::cli::findBuildPrecompiledHeaderRequest(3, withDirectory);
    ASSERT_TRUE(given.has_value());
    EXPECT_EQ(*given, "/a dir/with spaces");

    const char* similar[] = {"app", "--buildPrecompiledHeaderX", "--buildPrecompiledHeade"};
    EXPECT_FALSE(viz::cli::findBuildPrecompiledHeaderRequest(3, similar).has_value());

    // the name of the program (argv[0]) is not an option
    const char* programNameOnly[] = {"--buildPrecompiledHeader"};
    EXPECT_FALSE(viz::cli::findBuildPrecompiledHeaderRequest(1, programNameOnly).has_value());
}
