/** @file ReductionManager.cpp
 * @brief Implementation of the ReductionManager class.
 */

#include "ReductionManager.h"
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <iostream>
#include <qregularexpression.h>

ReductionManager::ReductionManager(const QString& reductionFilePath, const QString& reductionConfig)
{
    // Parse expected reduction types from config
    if (!reductionConfig.isEmpty())
    {
        const auto reductions = reductionConfig.split(',', Qt::SkipEmptyParts);
        expectedReductions.assign(reductions.begin(), reductions.end());
        for (auto& reduction : expectedReductions)
        {
            reduction = reduction.trimmed();
        }
    }

    // Load reduction data from file
    loadReductionData(reductionFilePath, reductionConfig);
}

void ReductionManager::loadReductionData(const QString& filePath, const QString& reductionConfig)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorMessage = QString("Failed to open reduction file: %1").arg(filePath);
        dataLoaded = false;
        return;
    }

    QTextStream in(&file);
    int lineNumber = 0;

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        ++lineNumber;

        if (line.isEmpty())
            continue;

        int stepNumber = -1;
        std::map<QString, QString> values;

        if (!parseLine(line, stepNumber, values))
        {
            errorMessage = QString("Failed to parse line %1 in reduction file: %2").arg(lineNumber).arg(line);
            file.close();
            dataLoaded = false;
            return;
        }

        reductionDataByStep[stepNumber] = {values};
    }

    file.close();
    dataLoaded = true;
    std::cout << "Loaded reduction data for " << reductionDataByStep.size() << " steps" << std::endl;
}

bool ReductionManager::parseLine(const QString& line, int& stepNumber, std::map<QString, QString>& values) const
{
    // Line format: "0  sum=12057,min=1,max=1"
    // Split by whitespace first to get step number
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return false;

    // First part is step number
    bool ok = false;
    stepNumber = parts[0].toInt(&ok);
    if (!ok)
        return false;

    // Second part contains reduction values separated by commas
    QString reductionPart = parts[1];
    QStringList reductionPairs = reductionPart.split(',', Qt::SkipEmptyParts);

    for (const QString& pair : reductionPairs)
    {
        QStringList keyValue = pair.split('=', Qt::SkipEmptyParts);
        if (keyValue.size() == 2)
        {
            QString key = keyValue[0].trimmed();
            QString value = keyValue[1].trimmed();
            values[key] = value;
        }
    }

    return !values.empty();
}

ReductionData ReductionManager::getReductionForStep(int stepNumber) const
{
    auto it = reductionDataByStep.find(stepNumber);
    if (it != reductionDataByStep.end())
    {
        return it->second;
    }
    return ReductionData{}; // Return empty data if step not found
}

QString ReductionManager::getFormattedReductionString(int stepNumber) const
{
    if (!dataLoaded)
        return QString();

    ReductionData data = getReductionForStep(stepNumber);
    if (data.values.empty())
        return QString();

    // Format as "sum=12057, min=1, max=1"
    QStringList parts;
    for (const auto& [key, value] : data.values)
    {
        parts.append(QString("%1=%2").arg(key, value));
    }

    return parts.join(", ");
}
