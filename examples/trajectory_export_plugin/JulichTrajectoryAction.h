/** @file JulichTrajectoryAction.h
 * @brief Action "export pedestrian trajectories" in the format of Forschungszentrum Jülich (PeTrack / JuPedSim / PedPy).
 *
 * The file is text with CRLF line ends: comment lines starting with `#`, an empty line, and then one row per
 * pedestrian and frame, sorted by id and frame:
 * @code
 * # id frame x/m y/m z/m
 *
 * 1	0	2.2786	2.6407	1.76
 * 1	1	2.2752	2.6359	1.76
 * @endcode
 *
 * The model keeps pedestrians in the cells of a grid, so the position of a pedestrian is the centre of its cell.
 * The grid has no scale, therefore the size of a cell and the duration of a step are parameters.
 * Depends on the Action API only (plugins/ActionApi.h). */

#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "plugins/ActionApi.h"

namespace viz::examples
{
class JulichTrajectoryAction final : public viz::actions::IAction
{
    using Result = viz::actions::Result;
    using ParameterInfo = viz::actions::ParameterInfo;

public:
    static constexpr char ID[] = "export-trajectories-julich";

    viz::actions::ActionInfo info() const override
    {
        viz::actions::ActionInfo info;
        info.id = ID;
        info.pluginName = "Trajectory export";
        info.title = "Pedestrian trajectories (Jülich format)...";
        info.category = "Export";
        info.description = "Writes the trajectories of pedestrians (id, frame, x, y, z) in the text format of "
                           "Forschungszentrum Jülich, readable by PeTrack, JuPedSim and PedPy.";
        info.supportedModels = {}; // any model which gives pedestrian ids in a substate, see "id_substate"
        return info;
    }

    std::vector<ParameterInfo> parameters() const override
    {
        using Type = ParameterInfo::Type;
        ParameterInfo output{.name = "output_file", .label = "Output file", .description = "File to write the trajectories to",
                             .type = Type::SaveFilePath, .defaultValue = "", .required = true};
        output.fileFilter = "Trajectories (*.txt);;All files (*)";

        return {
            output,
            {.name = "cell_size_m", .label = "Cell size [m]", .description = "Side of a cell of the grid in metres; x and y are given in metres",
             .type = Type::Real, .defaultValue = "0.4"},
            {.name = "step_duration_s", .label = "Step duration [s]",
             .description = "Simulated time of one step; the frame rate in the header is 1/step_duration (frame = step number)",
             .type = Type::Real, .defaultValue = "0.4"},
            {.name = "height_m", .label = "Height z [m]", .description = "The z column: constant height of a pedestrian in metres",
             .type = Type::Real, .defaultValue = "1.75"},
            {.name = "id_substate", .label = "Substate with the pedestrian id",
             .description = "Substate whose encoding is the id of the pedestrian standing in the cell (a negative number or text = empty cell)",
             .type = Type::String, .defaultValue = "pedestriansIds"},
            {.name = "id_offset", .label = "Id offset", .description = "Added to every id (PeTrack numbers pedestrians from 1)",
             .type = Type::Integer, .defaultValue = "0"},
            {.name = "flip_y", .label = "Y axis up",
             .description = "y grows upwards, as the picture in the Viewer (row 0 is the top row); otherwise y grows with the row number",
             .type = Type::Boolean, .defaultValue = "true"},
            {.name = "duplicate_ids", .label = "Pedestrian id in several cells at once",
             .description = "What to do if one id occurs in more than one cell of a step: keep the first one (row by row) and warn, or fail",
             .type = Type::String, .defaultValue = "keep_first", .choices = {"keep_first", "fail"}},
        };
    }

    Result setParameter(const std::string& name, const std::string& value) override
    {
        using namespace viz::actions;
        if (name == "output_file")
        {
            if (trimmed(value).empty())
                return Result::failure("give the output file");
            outputFile_ = value;
        }
        else if (name == "cell_size_m")
            return positiveReal(value, cellSize_, "cell size");
        else if (name == "step_duration_s")
            return positiveReal(value, stepDuration_, "step duration");
        else if (name == "height_m")
        {
            if (! parseReal(value, height_))
                return Result::failure("height has to be a number (with a dot), got '" + value + "'");
        }
        else if (name == "id_substate")
        {
            if (trimmed(value).empty())
                return Result::failure("give the name of the substate");
            idSubstate_ = trimmed(value);
        }
        else if (name == "id_offset")
        {
            if (! parseInteger(value, idOffset_))
                return Result::failure("id offset has to be a whole number, got '" + value + "'");
        }
        else if (name == "flip_y")
        {
            if (! parseBoolean(value, flipY_))
                return Result::failure("expected true or false, got '" + value + "'");
        }
        else if (name == "duplicate_ids")
        {
            if (value != "keep_first" && value != "fail")
                return Result::failure("expected keep_first or fail, got '" + value + "'");
            failOnDuplicates_ = value == "fail";
        }
        else
            return Result::failure("unknown parameter");
        return Result::success();
    }

    Result start(viz::actions::IActionContext& context) override
    {
        trajectories_.clear();
        seenInStep_.clear();
        duplicates_ = 0;
        context_ = &context;
        model_ = context.model();
        if (std::filesystem::path(outputFile_).has_parent_path() && ! std::filesystem::is_directory(std::filesystem::path(outputFile_).parent_path()))
            return Result::failure("directory of the output file does not exist: " + std::filesystem::path(outputFile_).parent_path().string());
        return Result::success();
    }

    Result beginStep(viz::actions::Step) override
    {
        seenInStep_.clear();
        return Result::success();
    }

    Result processCell(const viz::actions::CellRef& cell) override
    {
        const std::string text = cell.cell.substate(idSubstate_.c_str());

        long long id = 0;
        if (! viz::actions::parseInteger(text, id) || id < 0) // "-1" is an empty cell
            return Result::success();

        if (! seenInStep_.insert(id).second)
        {
            ++duplicates_;
            const std::string where = std::format("pedestrian {} is in more than one cell of step {} (again at row {}, column {})",
                                                  id, cell.step, cell.row, cell.column);
            if (failOnDuplicates_)
                return Result::failure(where);
            if (duplicates_ <= MAX_LOGGED_DUPLICATES)
                context_->log(viz::actions::LogLevel::Warning, where + "; the first position is kept");
            return Result::success();
        }

        trajectories_[id].push_back(Position{cell.step, cell.row, cell.column});
        return Result::success();
    }

    Result finish(bool completed) override
    {
        const auto collected = std::move(trajectories_);
        trajectories_.clear();
        if (! completed)
            return Result::success();

        if (collected.empty())
            return Result::failure("no pedestrians found: substate '" + idSubstate_ + "' gave no id >= 0 in any cell");

        if (duplicates_ > MAX_LOGGED_DUPLICATES)
            context_->log(viz::actions::LogLevel::Warning,
                          std::format("{} more repetitions of an id in a step were skipped (only the first {} are listed)",
                                      duplicates_ - MAX_LOGGED_DUPLICATES, MAX_LOGGED_DUPLICATES));

        std::ofstream file(outputFile_, std::ios::binary | std::ios::trunc);
        if (! file)
            return Result::failure("cannot open the output file for writing: " + outputFile_);

        const std::string eol = "\r\n"; // as in files of PeTrack
        file << "#This is a trajectory file exported from OOpenCal-Viewer" << eol
             << "# model: " << model_.modelName << eol
             << "# simulation: " << model_.modelDirectory << eol
             << "# framerate: " << number(1.0 / stepDuration_) << " fps" << eol
             << "# cell size: " << number(cellSize_) << " m, position = centre of the cell" << eol
             << "# z: height of person" << eol
             << "# id frame x/m y/m z/m" << eol << eol;

        std::size_t rows = 0;
        for (const auto& [id, positions] : collected) // std::map: sorted by id, positions by frame
        {
            for (const Position& p : positions)
            {
                const double x = (p.column + 0.5) * cellSize_;
                const double y = ((flipY_ ? model_.rows - 1 - p.row : p.row) + 0.5) * cellSize_;
                file << (id + idOffset_) << '\t' << p.frame << '\t' << number(x) << '\t' << number(y) << '\t' << number(height_) << eol;
                ++rows;
            }
        }
        file.close();
        if (! file)
            return Result::failure("writing the output file failed: " + outputFile_);

        return Result::success(std::format("Saved {} trajectories ({} positions) to {}", collected.size(), rows, outputFile_));
    }

private:
    struct Position
    {
        viz::actions::Step frame;
        int row;
        int column;
    };

    static constexpr std::size_t MAX_LOGGED_DUPLICATES = 10;

    static Result positiveReal(const std::string& text, double& target, const char* what)
    {
        double value = 0;
        if (! viz::actions::parseReal(text, value) || ! (value > 0))
            return Result::failure(std::string(what) + " has to be a positive number (with a dot), got '" + text + "'");
        target = value;
        return Result::success();
    }

    /// Up to 4 decimal places without trailing zeros, always with a dot: "2.275", "1.76", "0.4".
    static std::string number(double value)
    {
        std::string text = std::format("{:.4f}", value);
        text.erase(text.find_last_not_of('0') + 1);
        if (text.ends_with('.'))
            text.pop_back();
        return text == "-0" ? "0" : text;
    }

    std::string outputFile_;
    double cellSize_ = 0.4;
    double stepDuration_ = 0.4;
    double height_ = 1.75;
    std::string idSubstate_ = "pedestriansIds";
    long long idOffset_ = 0;
    bool flipY_ = true;
    bool failOnDuplicates_ = false;

    // state of a run
    viz::actions::IActionContext* context_ = nullptr;
    viz::actions::ModelDescription model_;
    std::map<long long, std::vector<Position>> trajectories_;
    std::unordered_set<long long> seenInStep_;
    std::size_t duplicates_ = 0;
};
} // namespace viz::examples
