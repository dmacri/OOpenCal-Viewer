#include "PrecompiledHeaderCommand.h"

#include <QCoreApplication>
#include <iostream>

#include "CommandLineParser.h"
#include "plugins/CompilationConfig.h"
#include "plugins/CppModuleBuilder.h"
#include "plugins/ModelLoader.h"

// The option is also registered in CommandLineParser (only to be listed in --help)
static_assert(std::string_view(CommandLineParser::ARG_BUILD_PRECOMPILED_HEADER) == viz::cli::BUILD_PRECOMPILED_HEADER_OPTION,
              "the option has to be spelled the same way in CommandLineParser and PrecompiledHeaderCommand");

namespace viz::cli
{
namespace
{
int buildPrecompiledHeader(const std::string& requestedDirectory)
{
    const std::string directory = requestedDirectory.empty()
                                      ? plugins::CompilationConfig::getInstance().getPrecompiledHeaderDir()
                                      : requestedDirectory;
    if (directory.empty())
    {
        std::cerr << "Error: no directory for the precompiled header. Pass " << BUILD_PRECOMPILED_HEADER_OPTION
                  << "=<directory> or set OOPENCAL_PRECOMPILED_HEADER_DIR." << std::endl;
        return 2;
    }

    plugins::CppModuleBuilder builder;
    std::cout << "Building the precompiled header in '" << directory << "'" << std::endl;

    const auto result = builder.buildPrecompiledHeader(directory);
    if (! result.success)
    {
        std::cerr << "✗ Building the precompiled header failed (exit code " << result.exitCode << ")\n"
                  << result.stdErr << std::endl;
        return 1;
    }

    std::cout << "✓ Precompiled header is ready: " << result.outputFile << std::endl;
    return 0;
}
} // namespace

std::optional<int> runBuildPrecompiledHeaderCommand(int& argc, char* argv[])
{
    const auto request = findBuildPrecompiledHeaderRequest(argc, argv);
    if (! request)
        return std::nullopt;

    QCoreApplication coreApplication(argc, argv); // no GUI and no display needed
    return buildPrecompiledHeader(*request);
}
} // namespace viz::cli
