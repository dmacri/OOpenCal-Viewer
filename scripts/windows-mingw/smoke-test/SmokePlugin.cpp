/** @file SmokePlugin.cpp
 * @brief The same thing which ModelLoader::generateWrapper() generates for every model, with the example cell.
 *
 * It is used by the CI of the Windows (MinGW) package and by scripts/windows-mingw/package-headers.sh to prove that the compiler,
 * the headers and the libraries which are shipped in the package really are enough to build a model plugin.
 * Keep it in sync with ModelLoader::generateWrapper() and pch::commonIncludeDirectives(). */

#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "visualiserProxy/SceneWidgetVisualizerProxy.h"
#include "visualiserProxy/SceneWidgetVisualizerFactory.h"
#include "CustomCell.h"

#define MODEL_NAME "SmokeModel"

#ifdef _WIN32
    #ifdef BUILDING_DLL
        #define DLL_EXPORT __declspec(dllexport)
    #else
        #define DLL_EXPORT __declspec(dllimport)
    #endif
#else
    #define DLL_EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
DLL_EXPORT void registerPlugin()
{
    SceneWidgetVisualizerFactory::registerModel<CustomCell>(MODEL_NAME);
}

DLL_EXPORT const char* getModelName()
{
    return MODEL_NAME;
}
} // extern "C"
