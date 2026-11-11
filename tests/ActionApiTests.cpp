// Tests of the action plugin mechanism (plugins/ActionApi.h, ActionRegistry) and of the example action which exports
// trajectories of pedestrians in the Jülich format. No Qt, no VTK: the simulation is replaced with a fake context.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

#include "JulichTrajectoryAction.h"
#include "plugins/ActionRegistry.h"

using namespace viz::actions;
namespace fs = std::filesystem;

namespace
{
// ---------------------------------------------------------------- registry
class DummyAction final : public IAction
{
public:
    DummyAction(std::string id, std::vector<std::string> models = {})
        : id_(std::move(id))
        , models_(std::move(models))
    {
    }
    ActionInfo info() const override
    {
        ActionInfo info;
        info.id = id_;
        info.supportedModels = models_;
        return info;
    }
    Result start(IActionContext&) override { return Result::success(); }
    Result processCell(const CellRef&) override { return Result::success(); }
    Result finish(bool) override { return Result::success(); }

private:
    std::string id_;
    std::vector<std::string> models_;
};

TEST(ActionRegistryTest, RegistersAndFindsActions)
{
    ActionRegistry registry;
    EXPECT_TRUE(registry.registerAction(std::make_unique<DummyAction>("a")));
    EXPECT_TRUE(registry.registerAction(std::make_unique<DummyAction>("b")));

    EXPECT_NE(registry.find("a"), nullptr);
    EXPECT_EQ(registry.find("missing"), nullptr);
    EXPECT_EQ(registry.ids(), (std::vector<std::string>{"a", "b"}));
}

TEST(ActionRegistryTest, RefusesDuplicatesEmptyIdsAndNull)
{
    ActionRegistry registry;
    EXPECT_TRUE(registry.registerAction(std::make_unique<DummyAction>("a")));
    EXPECT_FALSE(registry.registerAction(std::make_unique<DummyAction>("a")));
    EXPECT_FALSE(registry.registerAction(std::make_unique<DummyAction>("")));
    EXPECT_FALSE(registry.registerAction(nullptr));
    EXPECT_EQ(registry.ids().size(), 1u);
}

TEST(ActionRegistryTest, RemovesOnlyActionsOfTheUnloadedPlugin)
{
    ActionRegistry registry;
    registry.setCurrentOwner("/plugins/one.so");
    registry.registerAction(std::make_unique<DummyAction>("one-a"));
    registry.registerAction(std::make_unique<DummyAction>("one-b"));
    registry.setCurrentOwner("/plugins/two.so");
    registry.registerAction(std::make_unique<DummyAction>("two"));
    registry.setCurrentOwner({});

    EXPECT_EQ(registry.removeOwnedBy("/plugins/one.so"), 2u);
    EXPECT_EQ(registry.ids(), (std::vector<std::string>{"two"}));
    EXPECT_EQ(registry.removeOwnedBy("/plugins/one.so"), 0u);
    EXPECT_EQ(registry.removeOwnedBy(""), 0u);
}

TEST(ActionRegistryTest, FiltersActionsByModel)
{
    ActionRegistry registry;
    registry.registerAction(std::make_unique<DummyAction>("any"));
    registry.registerAction(std::make_unique<DummyAction>("crowd", std::vector<std::string>{"crowd_simulation"}));

    EXPECT_EQ(registry.actionsFor("crowd_simulation").size(), 2u);
    ASSERT_EQ(registry.actionsFor("SciddicaT").size(), 1u);
    EXPECT_EQ(registry.actionsFor("SciddicaT").front()->info().id, "any");
}

// ---------------------------------------------------------------- parsing of values
TEST(ActionApiParsingTest, ParsesNumbersLocaleIndependently)
{
    double real = 0;
    EXPECT_TRUE(parseReal("0.4", real));
    EXPECT_DOUBLE_EQ(real, 0.4);
    EXPECT_TRUE(parseReal("  2.5 ", real));
    EXPECT_DOUBLE_EQ(real, 2.5);
    EXPECT_FALSE(parseReal("0,4", real));
    EXPECT_FALSE(parseReal("", real));
    EXPECT_FALSE(parseReal("1.5m", real));

    long long integer = 0;
    EXPECT_TRUE(parseInteger("-3", integer));
    EXPECT_EQ(integer, -3);
    EXPECT_FALSE(parseInteger("3.5", integer));
    EXPECT_FALSE(parseInteger("", integer));
}

TEST(ActionApiParsingTest, ParsesBooleans)
{
    bool value = false;
    for (const char* yes : {"true", "TRUE", "1", "yes", "on"})
    {
        value = false;
        EXPECT_TRUE(parseBoolean(yes, value)) << yes;
        EXPECT_TRUE(value) << yes;
    }
    for (const char* no : {"false", "0", "No", "off"})
    {
        value = true;
        EXPECT_TRUE(parseBoolean(no, value)) << no;
        EXPECT_FALSE(value) << no;
    }
    EXPECT_FALSE(parseBoolean("maybe", value));
}

// ---------------------------------------------------------------- fake simulation
class FakeContext final : public IActionContext
{
public:
    FakeContext(int rows, int columns, std::vector<Step> steps)
        : steps_(std::move(steps))
    {
        description_.modelName = "fake_crowd";
        description_.modelDirectory = "/sim";
        description_.rows = rows;
        description_.columns = columns;
    }
    const ModelDescription& model() const override { return description_; }
    const std::vector<Step>& steps() const override { return steps_; }
    std::optional<std::string> headerValue(const std::string&, const std::string&) const override { return std::nullopt; }
    void log(LogLevel level, const std::string& message) override
    {
        if (level == LogLevel::Warning)
            warnings.push_back(message);
    }
    std::vector<std::string> warnings;

private:
    ModelDescription description_;
    std::vector<Step> steps_;
};

/// Pedestrian ids of the cells; every other cell answers "-1" as the CrowdSimulation model does.
class FakeCells final : public ICellReader
{
public:
    std::string substate(const char* name) const override
    {
        requestedSubstates.push_back(name ? name : "");
        const auto it = ids.find({row, column});
        return it == ids.end() ? "-1" : std::to_string(it->second);
    }
    std::map<std::pair<int, int>, long long> ids;
    int row = 0, column = 0;
    mutable std::vector<std::string> requestedSubstates;
};

/// Drives an action the way ActionRunner does.
Result drive(IAction& action, FakeContext& context, const std::function<void(Step, FakeCells&)>& placePedestrians, bool finishCompleted = true)
{
    if (auto r = action.start(context); ! r)
        return r;
    FakeCells cells;
    for (const Step step : context.steps())
    {
        cells.ids.clear();
        placePedestrians(step, cells);
        if (auto r = action.beginStep(step); ! r)
            return r;
        for (int row = 0; row < context.model().rows; ++row)
            for (int column = 0; column < context.model().columns; ++column)
            {
                cells.row = row;
                cells.column = column;
                if (auto r = action.processCell(CellRef{step, 0, row, column, cells}); ! r)
                {
                    action.finish(false);
                    return r;
                }
            }
        if (auto r = action.endStep(step); ! r)
            return r;
    }
    return action.finish(finishCompleted);
}

class JulichTrajectoryActionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        path = (fs::temp_directory_path() / ("julich_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                                              ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".txt"))
                   .string();
        fs::remove(path);
        ASSERT_TRUE(action.setParameter("output_file", path));
    }
    void TearDown() override { fs::remove(path); }

    std::string fileText() const
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        return text.str();
    }

    /// Lines of the data part (after the empty line that ends the header), without line ends
    std::vector<std::string> dataRows() const
    {
        std::vector<std::string> rows;
        const std::string text = fileText();
        std::size_t position = text.find("\r\n\r\n");
        if (position == std::string::npos)
            return rows;
        std::istringstream in(text.substr(position + 4));
        for (std::string line; std::getline(in, line, '\n');)
        {
            EXPECT_TRUE(line.ends_with('\r')) << "not CRLF: " << line;
            rows.push_back(line.substr(0, line.size() - 1));
        }
        return rows;
    }

    viz::examples::JulichTrajectoryAction action;
    std::string path;
};

TEST_F(JulichTrajectoryActionTest, WritesPositionsAsCellCentresSortedByIdAndFrame)
{
    FakeContext context(/*rows=*/3, /*columns=*/4, {0, 1, 2});
    // pedestrian 7 walks to the right along the top row, pedestrian 2 stands in the bottom-left corner
    const auto result = drive(action, context, [](Step step, FakeCells& cells)
                              {
                                  cells.ids[{0, static_cast<int>(step)}] = 7;
                                  cells.ids[{2, 0}] = 2;
                              });
    ASSERT_TRUE(result) << result.message;

    const std::vector<std::string> expected = {
        "2\t0\t0.2\t0.2\t1.75", "2\t1\t0.2\t0.2\t1.75", "2\t2\t0.2\t0.2\t1.75",
        // top row 0 -> y = (3 - 1 - 0 + 0.5) * 0.4 = 1; x = (column + 0.5) * 0.4
        "7\t0\t0.2\t1\t1.75", "7\t1\t0.6\t1\t1.75", "7\t2\t1\t1\t1.75"};
    EXPECT_EQ(dataRows(), expected);

    const std::string text = fileText();
    EXPECT_TRUE(text.starts_with("#"));
    EXPECT_NE(text.find("# framerate: 2.5 fps\r\n"), std::string::npos);
    EXPECT_NE(text.find("# id frame x/m y/m z/m\r\n\r\n"), std::string::npos);
    EXPECT_EQ(text.find('\n'), text.find("\r\n") + 1) << "only CRLF line ends";
}

TEST_F(JulichTrajectoryActionTest, FrameIsTheStepNumberEvenWhenStepsAreSparse)
{
    FakeContext context(1, 3, {0, 10, 20});
    const auto result = drive(action, context, [](Step step, FakeCells& cells) { cells.ids[{0, static_cast<int>(step / 10)}] = 1; });
    ASSERT_TRUE(result) << result.message;
    EXPECT_EQ(dataRows(), (std::vector<std::string>{"1\t0\t0.2\t0.2\t1.75", "1\t10\t0.6\t0.2\t1.75", "1\t20\t1\t0.2\t1.75"}));
}

TEST_F(JulichTrajectoryActionTest, OptionsChangeScaleHeightIdsAndYDirection)
{
    ASSERT_TRUE(action.setParameter("cell_size_m", "0.5"));
    ASSERT_TRUE(action.setParameter("step_duration_s", "0.04"));
    ASSERT_TRUE(action.setParameter("height_m", "1.76"));
    ASSERT_TRUE(action.setParameter("id_offset", "1"));
    ASSERT_TRUE(action.setParameter("flip_y", "false"));

    FakeContext context(2, 2, {0});
    const auto result = drive(action, context, [](Step, FakeCells& cells) { cells.ids[{1, 1}] = 0; });
    ASSERT_TRUE(result) << result.message;

    EXPECT_EQ(dataRows(), (std::vector<std::string>{"1\t0\t0.75\t0.75\t1.76"})); // row 1 counted from the top, id 0 -> 1
    EXPECT_NE(fileText().find("# framerate: 25 fps\r\n"), std::string::npos);
}

TEST_F(JulichTrajectoryActionTest, ReadsTheConfiguredSubstate)
{
    ASSERT_TRUE(action.setParameter("id_substate", "myIds"));
    FakeContext context(1, 1, {0});
    FakeCells probe;
    ASSERT_TRUE(action.start(context));
    action.beginStep(0);
    action.processCell(CellRef{0, 0, 0, 0, probe});
    ASSERT_EQ(probe.requestedSubstates.size(), 1u);
    EXPECT_EQ(probe.requestedSubstates.front(), "myIds");
    action.finish(false);
}

TEST_F(JulichTrajectoryActionTest, DuplicatedIdKeepsFirstPositionAndWarns)
{
    FakeContext context(1, 3, {0});
    const auto result = drive(action, context, [](Step, FakeCells& cells)
                              {
                                  cells.ids[{0, 0}] = 5;
                                  cells.ids[{0, 2}] = 5; // the same pedestrian again
                              });
    ASSERT_TRUE(result) << result.message;
    EXPECT_EQ(dataRows(), (std::vector<std::string>{"5\t0\t0.2\t0.2\t1.75"}));
    ASSERT_EQ(context.warnings.size(), 1u);
    EXPECT_NE(context.warnings.front().find("pedestrian 5"), std::string::npos);
}

TEST_F(JulichTrajectoryActionTest, DuplicatedIdCanBeAnError)
{
    ASSERT_TRUE(action.setParameter("duplicate_ids", "fail"));
    FakeContext context(1, 3, {0});
    const auto result = drive(action, context, [](Step, FakeCells& cells)
                              {
                                  cells.ids[{0, 0}] = 5;
                                  cells.ids[{0, 2}] = 5;
                              });
    EXPECT_FALSE(result);
    EXPECT_NE(result.message.find("pedestrian 5"), std::string::npos);
    EXPECT_FALSE(fs::exists(path)) << "nothing is written after a failure";
}

TEST_F(JulichTrajectoryActionTest, FailsWhenThereAreNoPedestrians)
{
    FakeContext context(2, 2, {0, 1});
    const auto result = drive(action, context, [](Step, FakeCells&) {});
    EXPECT_FALSE(result);
    EXPECT_NE(result.message.find("pedestriansIds"), std::string::npos);
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(JulichTrajectoryActionTest, CancelledRunWritesNothingAndTheActionCanBeReused)
{
    FakeContext context(1, 2, {0});
    ASSERT_TRUE(drive(action, context, [](Step, FakeCells& cells) { cells.ids[{0, 0}] = 1; }, /*finishCompleted=*/false));
    EXPECT_FALSE(fs::exists(path));

    // data of the cancelled run must not leak into the next one
    ASSERT_TRUE(drive(action, context, [](Step, FakeCells& cells) { cells.ids[{0, 1}] = 2; }));
    EXPECT_EQ(dataRows(), (std::vector<std::string>{"2\t0\t0.6\t0.2\t1.75"}));
}

TEST_F(JulichTrajectoryActionTest, RejectsWrongParameters)
{
    EXPECT_FALSE(action.setParameter("cell_size_m", "0,4"));
    EXPECT_FALSE(action.setParameter("cell_size_m", "0"));
    EXPECT_FALSE(action.setParameter("cell_size_m", "-1"));
    EXPECT_FALSE(action.setParameter("step_duration_s", "abc"));
    EXPECT_FALSE(action.setParameter("id_offset", "1.5"));
    EXPECT_FALSE(action.setParameter("flip_y", "maybe"));
    EXPECT_FALSE(action.setParameter("duplicate_ids", "ignore"));
    EXPECT_FALSE(action.setParameter("id_substate", "  "));
    EXPECT_FALSE(action.setParameter("output_file", ""));
    EXPECT_FALSE(action.setParameter("no_such_parameter", "1"));
    EXPECT_TRUE(action.setParameter("cell_size_m", "0.4")) << "a good value is accepted after bad ones";
}

TEST_F(JulichTrajectoryActionTest, FailsToStartWhenTheOutputDirectoryDoesNotExist)
{
    ASSERT_TRUE(action.setParameter("output_file", (fs::temp_directory_path() / "no_such_dir_for_viz" / "t.txt").string()));
    FakeContext context(1, 1, {0});
    const auto result = action.start(context);
    EXPECT_FALSE(result);
    EXPECT_NE(result.message.find("does not exist"), std::string::npos);
}

TEST_F(JulichTrajectoryActionTest, DeclaresItsParametersConsistently)
{
    const auto parameters = action.parameters();
    ASSERT_FALSE(parameters.empty());
    for (const auto& parameter : parameters)
    {
        // every declared default has to be accepted by the action itself (the Viewer sets all parameters before a run)
        if (parameter.defaultValue.empty())
            continue;
        EXPECT_TRUE(action.setParameter(parameter.name, parameter.defaultValue)) << parameter.name;
    }
    EXPECT_TRUE(parameters.front().required);
}
} // namespace
