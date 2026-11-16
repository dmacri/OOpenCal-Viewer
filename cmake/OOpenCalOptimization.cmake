# ==============================================================================
# OOpenCalOptimization.cmake
#
# One place that decides how the code is optimized. It is shared by
#   - the Viewer itself (CMakeLists.txt),
#   - the model plugins (examples/custom_model_plugin/CMakeLists.txt, built by scripts/build_plugin.sh),
#   - the models the Viewer compiles on the fly (the value is handed over as a compile definition),
# so all of them are optimized the same way. This also matters for precompiled headers: g++ rejects a precompiled
# header created without optimization when it is used with -O2 ("created and used with differing settings").
#
# Usage:
#   cmake -DOOPENCAL_OPTIMIZATION=2 ..
#
# Values:
#   none  - (default) the option adds nothing, so the behaviour is exactly as before: the optimization is the one
#           of CMAKE_BUILD_TYPE (nothing at all when it is empty, i.e. -O0)
#   0 1 2 3 s g fast
#         - -O0 -O1 -O2 -O3 -Os -Og -Ofast (MSVC: see below). The flag is added after the flags of CMAKE_BUILD_TYPE,
#           so it wins over e.g. -O3 of Release. -Ofast allows floating point reorderings (-ffast-math), which can change
#           results of simulations: use it only when that is acceptable.
# ==============================================================================

set(OOPENCAL_OPTIMIZATION_VALUES none 0 1 2 3 s g fast)

# Declares the cache variable OOPENCAL_OPTIMIZATION (once) and validates it.
macro(oopencal_declare_optimization_option)
    # No effect on the value when it is already in the cache (e.g. given with -D), it only sets the type and the help text
    set(OOPENCAL_OPTIMIZATION "none" CACHE STRING
        "Compiler optimization level: none (as before, decided by CMAKE_BUILD_TYPE), 0, 1, 2, 3, s, g or fast")
    set_property(CACHE OOPENCAL_OPTIMIZATION PROPERTY STRINGS ${OOPENCAL_OPTIMIZATION_VALUES})

    string(TOLOWER "${OOPENCAL_OPTIMIZATION}" _oopencal_optimization_lower)
    # accept "O2" / "-O2" as well
    string(REGEX REPLACE "^-?o" "" _oopencal_optimization_lower "${_oopencal_optimization_lower}")
    if(_oopencal_optimization_lower STREQUAL "")
        set(_oopencal_optimization_lower "none")
    endif()
    if(NOT _oopencal_optimization_lower IN_LIST OOPENCAL_OPTIMIZATION_VALUES)
        message(FATAL_ERROR
            "OOPENCAL_OPTIMIZATION='${OOPENCAL_OPTIMIZATION}' is not supported. Use one of: ${OOPENCAL_OPTIMIZATION_VALUES}")
    endif()
    set(OOPENCAL_OPTIMIZATION_NORMALIZED "${_oopencal_optimization_lower}")
endmacro()

# Compiler options (a list, empty for "none") for the normalized value of OOPENCAL_OPTIMIZATION
function(oopencal_optimization_options out_var)
    set(options "")
    set(level "${OOPENCAL_OPTIMIZATION_NORMALIZED}")

    if(level STREQUAL "none")
        # nothing: keep the behaviour of CMAKE_BUILD_TYPE
    elseif(MSVC)
        # MSVC has no -O3 / -Og / -Ofast: the nearest equivalents
        if(level STREQUAL "0" OR level STREQUAL "g")
            set(options /Od)
        elseif(level STREQUAL "1" OR level STREQUAL "s")
            set(options /O1)
        elseif(level STREQUAL "fast")
            set(options /O2 /fp:fast)
        else() # 2, 3
            set(options /O2)
        endif()
    else()
        # GCC, Clang (-O0, -O1, -O2, -O3, -Os, -Og, -Ofast)
        set(options "-O${level}")
    endif()

    set(${out_var} "${options}" PARENT_SCOPE)
endfunction()

# Adds the optimization to a target (does nothing for "none")
function(oopencal_apply_optimization target)
    oopencal_optimization_options(_options)
    if(_options)
        target_compile_options(${target} PRIVATE ${_options})
    endif()
endfunction()
