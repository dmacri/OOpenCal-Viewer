#include <gtest/gtest.h>

#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <stdexcept>
#include <string>

#include "data/ReductionManager.h"

namespace
{
/** Writes `content` to a reduction file in a temporary directory and returns its path. */
QString writeReductionFile(QTemporaryDir& dir, const QString& content)
{
    const QString path = dir.filePath("model-red.txt");
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream(&file) << content;
    return path;
}
} // namespace

TEST(ReductionManager, LoadsValuesOfEveryStep)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeReductionFile(dir, "0  sum=12057,min=1\n1  sum=12058,min=2\n");

    const ReductionManager manager(path, "sum,min");

    ASSERT_TRUE(manager.isAvailable());
    EXPECT_EQ(manager.getReductionForStep(1).values.at(QString("sum")), QString("12058"));
    EXPECT_EQ(manager.getFormattedReductionString(0).toStdString(), "min=1, sum=12057");
}

// HydroCAL3D wrote "step  name=,name=,..." - keys without any value. The error must say so.
TEST(ReductionManager, EmptyValuesGiveExplainingError)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeReductionFile(dir, "91081  celleTotSup=,teta_Subsurface_Somma=,convergenceMinSup=\n");

    try
    {
        [[maybe_unused]] const ReductionManager manager(path, "(celleTotSup,%f),(teta_Subsurface_Somma,%f)");
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        const std::string message = e.what();
        EXPECT_NE(message.find("line 1"), std::string::npos) << message;
        EXPECT_NE(message.find("empty value"), std::string::npos) << message;
        EXPECT_NE(message.find("celleTotSup="), std::string::npos) << message; // the offending line is quoted
    }
}

TEST(ReductionManager, LineWithoutStepNumberGivesExplainingError)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = writeReductionFile(dir, "sum=1,min=2\n");

    try
    {
        [[maybe_unused]] const ReductionManager manager(path, "sum,min");
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("step number"), std::string::npos) << e.what();
    }
}
