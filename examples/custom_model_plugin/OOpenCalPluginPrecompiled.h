/** @file OOpenCalPluginPrecompiled.h
 * @brief Headers which every plugin built from Plugin_FullTemplate.cpp includes.
 *
 * They are precompiled once and reused by all plugins (see CMakeLists.txt, OOPENCAL_PLUGIN_PCH_ONLY,
 * and doc/PRECOMPILED_HEADER.md). Plugins do not include this file explicitly: the compiler is told to use it. */

#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "visualiserProxy/SceneWidgetVisualizerProxy.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
