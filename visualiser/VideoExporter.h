/** @file VideoExporter.h
 * @brief Declaration of the VideoExporter class for exporting VTK renderings to video. */

#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include "core/types.h"

class vtkRenderWindow;

/** @class VideoExporter
 * @brief Handles exporting of VTK render window content to video format (OGG).
 * 
 * This class provides functionality to capture frames from a VTK render window
 * and save them as a video file. It's designed to work asynchronously and provides
 * progress feedback through signals and callbacks. */
class VideoExporter : public QObject
{
    Q_OBJECT

public:
    /** @brief Constructs a VideoExporter with the given parent
     *  @param parent Parent QObject (optional) */
    explicit VideoExporter(QObject* parent = nullptr);

    /** @brief Exports the current VTK render window to a video file.
     * 
     * This method captures frames from the provided render window and encodes them
     * into a video file. It supports progress tracking and cancellation.
     * 
     * @param renderWindow The VTK render window to capture frames from
     * @param outputFilePath Path where the video file will be saved (should end with .ogv)
     * @param fps Frames per second for the output video
     * @param totalSteps Total number of frames to capture
     * @param updateStepCallback Called before capturing each frame (frame number as parameter)
     * @param progressCallback Called to report export progress (current, total)
     * @param cancelledCallback Called to check if export was cancelled
     * @throws std::runtime_error if export fails or is cancelled */
    void exportVideo(
        vtkRenderWindow* renderWindow,
        const QString& outputFilePath,
        int fps,
        StepIndex totalSteps,
        std::function<void (StepIndex)> updateStepCallback,
        std::function<void (StepIndex, StepIndex)> progressCallback,
        std::function<bool()> cancelledCallback
    );

signals:
    /** @brief Emitted when export progress changes.
     *  @param currentStep Current step being processed
     *  @param totalSteps Total number of steps to process */
    void progressChanged(StepIndex currentStep, StepIndex totalSteps);

    /// @brief Emitted when export completes successfully
    void exportCompleted();

    /// @brief Emitted when export fails with description of the error that occurred
    void exportFailed(const QString& errorMessage);
};
