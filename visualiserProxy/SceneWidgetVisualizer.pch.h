#pragma once

/** @file SceneWidgetVisualizer.pch.h
 * @brief Precompiled header for plugin compilation.
 *
 * This header is precompiled and bundled with AppImage and Debian packages
 * to avoid the need to distribute all VTK and project headers.
 *
 * When compiling plugins from directories, the wrapper code will:
 * 1. Try to include this precompiled header (if available)
 * 2. Fall back to including individual headers (if precompiled not found)
 *
 * This allows plugins to be compiled without needing the full development
 * environment, as long as this precompiled header is available. */


// Standard library headers - these are safe to precompile
#include <iostream>
#include <memory> // std::unique_ptr

// Project headers which indirectly includes VTK
#include "visualiserProxy/SceneWidgetVisualizerAdapter.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
