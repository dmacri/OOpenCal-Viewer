#include <iostream>
#include <filesystem>
#include <argparse/argparse.hpp>
#include <QApplication>
#include "CommandLineParser.h"


bool CommandLineParser::parse(int argc, char* argv[])
{
    const auto appName = QApplication::applicationName().toStdString();
    try
    {
        argparse::ArgumentParser program(appName);
        program.add_description(appName + " for simulation data with VTK and Qt");
        program.add_epilog("Examples:\n" +
                           std::format("  {} config.txt\n", appName) +
                           std::format("  {} config.txt {}=MyModel\n", appName, ARG_STARTING_MODEL) +
                           std::format("  {} {}=/tmp/movie {}", appName, ARG_GENERATE_MOVIE, ARG_EXIT_AFTER_LAST));

        // Positional argument: configuration file
        program.add_argument(ARG_CONFIG)
            .help("Path to configuration file")
            .nargs(argparse::nargs_pattern::optional);

        // Optional arguments
        program.add_argument(ARG_LOAD_MODEL)
            .help("Path to custom model plugin (can be repeated)")
            .append();

        program.add_argument(ARG_STARTING_MODEL)
            .help("Name of the model to start with");

        program.add_argument(ARG_GENERATE_MOVIE)
            .help("Generate movie by running all steps (for testing)");

        program.add_argument(ARG_GENERATE_IMAGE)
            .help("Generate image for current step and save to file");

        program.add_argument(ARG_STEP)
            .help("Go to specific step directly")
            .scan<'i', int>();

        program.add_argument(ARG_EXIT_AFTER_LAST)
            .help("Exit after last step (useful with --generateMoviePath)")
            .flag();

        program.add_argument(ARG_SILENT)
            .help("Silent mode: Skip displaying information dialogs (usefull when we use commandline arguments)")
            .flag();

        try
        {
            program.parse_args(argc, argv);
        }
        catch (const std::runtime_error& err)
        {
            std::cerr << err.what() << std::endl;
            std::cerr << program << std::endl;
            return false;
        }

        // Parse positional argument - can be either config file or model directory
        if (auto cfg = program.present<std::vector<std::string>>(ARG_CONFIG))
        {
            if (! cfg->empty())
            {
                const std::string path = cfg->at(0);
                configFile = path;
                
                // Check if it's a directory (model directory) or file (config file)
                namespace fs = std::filesystem;
                if (fs::exists(path) && fs::is_directory(path))
                {
                    isDirectory = true;
                }
                else
                {
                    isDirectory = false;
                }
            }
        }

        if (auto models = program.present<std::vector<std::string>>(ARG_LOAD_MODEL))
            loadModelPaths = *models;

        if (auto model = program.present<std::string>(ARG_STARTING_MODEL))
            startingModel = *model;

        if (auto path = program.present<std::string>(ARG_GENERATE_MOVIE))
            generateMoviePath = *path;

        if (auto path = program.present<std::string>(ARG_GENERATE_IMAGE))
            generateImagePath = *path;

        if (auto st = program.present<int>(ARG_STEP))
            step = *st;

        exitAfterLastStep = program.is_used(ARG_EXIT_AFTER_LAST);
        silentMode        = program.is_used(ARG_SILENT);

        return true;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error parsing command-line arguments: " << err.what() << std::endl;
        printHelp();
        return false;
    }
}

void CommandLineParser::printHelp() const
{
    constexpr int WIDTH = 24; // variable min width
    const auto appName = QApplication::applicationName().toStdString();
    std::cout << std::format("Usage: {} [CONFIG_FILE|MODEL_DIR] [OPTIONS]\n\n", appName)
              << "Positional Arguments:\n"
              << std::format("  {: <{}} Path to configuration file or model directory (optional)\n", "CONFIG_FILE|MODEL_DIR", WIDTH)
              << std::format("  {: <{}} If directory: loads model from dir (Header.txt, .h file, data files)\n", "", WIDTH)
              << std::format("  {: <{}} If file: loads configuration from file\n\n", "", WIDTH)
              << "Optional Arguments:\n"
              << std::format("  {: <{}} Load custom model plugin (can be repeated)\n", ARG_LOAD_MODEL, WIDTH)
              << std::format("  {: <{}} Start with specific model\n", ARG_STARTING_MODEL, WIDTH)
              << std::format("  {: <{}} Generate movie by running all steps\n", ARG_GENERATE_MOVIE, WIDTH)
              << std::format("  {: <{}} Generate image for current step\n", ARG_GENERATE_IMAGE, WIDTH)
              << std::format("  {: <{}} Go to specific step directly\n", ARG_STEP, WIDTH)
              << std::format("  {: <{}} Exit after last step\n", ARG_EXIT_AFTER_LAST, WIDTH)
              << std::format("  {: <{}} Suppress error dialogs and messages\n", ARG_SILENT, WIDTH)
              << std::format("  {: <{}} Show this help message\n\n", "-h, --help", WIDTH)
              << "Examples:\n"
              << std::format("  {} config.txt\n", appName)
              << std::format("  {} /path/to/model/directory\n", appName)
              << std::format("  {} config.txt {}=MyModel\n", appName, ARG_STARTING_MODEL)
              << std::format("  {} {}=/tmp/movie {}\n", appName, ARG_GENERATE_MOVIE, ARG_EXIT_AFTER_LAST);
}
