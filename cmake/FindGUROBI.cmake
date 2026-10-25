# Use headers and libraries from the same installed SDK, never archived headers.
set(GUROBI_HOME "$ENV{GUROBI_HOME}" CACHE PATH "Gurobi platform directory, e.g. C:/gurobi1203/win64")
find_path(GUROBI_INCLUDE_DIR gurobi_c.h HINTS "${GUROBI_HOME}/include")

if(GUROBI_INCLUDE_DIR)
    file(STRINGS "${GUROBI_INCLUDE_DIR}/gurobi_c.h" _major REGEX "^#define GRB_VERSION_MAJOR +[0-9]+")
    file(STRINGS "${GUROBI_INCLUDE_DIR}/gurobi_c.h" _minor REGEX "^#define GRB_VERSION_MINOR +[0-9]+")
    string(REGEX MATCH "[0-9]+$" _major "${_major}")
    string(REGEX MATCH "[0-9]+$" _minor "${_minor}")
    find_library(GUROBI_C_LIBRARY NAMES "gurobi${_major}${_minor}" HINTS "${GUROBI_HOME}/lib")
endif()

if(MSVC)
    # /MD and /MDd wrappers. Override these cache paths for a different SDK toolset.
    find_library(GUROBI_CXX_LIBRARY_RELEASE NAMES gurobi_c++md2017 HINTS "${GUROBI_HOME}/lib")
    find_library(GUROBI_CXX_LIBRARY_DEBUG NAMES gurobi_c++mdd2017 HINTS "${GUROBI_HOME}/lib")
else()
    find_library(GUROBI_CXX_LIBRARY_RELEASE NAMES gurobi_c++ HINTS "${GUROBI_HOME}/lib")
    set(GUROBI_CXX_LIBRARY_DEBUG "${GUROBI_CXX_LIBRARY_RELEASE}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GUROBI
    REQUIRED_VARS GUROBI_INCLUDE_DIR GUROBI_C_LIBRARY GUROBI_CXX_LIBRARY_RELEASE
    REASON_FAILURE_MESSAGE "Install the Gurobi C++ SDK and set GUROBI_HOME to its platform directory. For other toolsets set GUROBI_CXX_LIBRARY_RELEASE explicitly.")
if(GUROBI_FOUND AND NOT TARGET GUROBI::GUROBI)
    if(NOT EXISTS "${GUROBI_INCLUDE_DIR}/gurobi_c++.h")
        message(FATAL_ERROR "The selected Gurobi SDK is missing gurobi_c++.h")
    endif()
    if(MSVC AND NOT GUROBI_CXX_LIBRARY_DEBUG)
        if(CMAKE_CONFIGURATION_TYPES)
            # No unsafe Release-wrapper/Debug-runtime combination.
            set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;MinSizeRel" CACHE STRING "Configurations" FORCE)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(FATAL_ERROR "Gurobi Debug wrapper missing: set GUROBI_CXX_LIBRARY_DEBUG or build Release")
        endif()
        set(GUROBI_CXX_LIBRARY_DEBUG "${GUROBI_CXX_LIBRARY_RELEASE}")
    endif()
    add_library(GUROBI::GUROBI INTERFACE IMPORTED)
    set_target_properties(GUROBI::GUROBI PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GUROBI_INCLUDE_DIR}")
    target_link_libraries(GUROBI::GUROBI INTERFACE
        optimized "${GUROBI_CXX_LIBRARY_RELEASE}"
        debug "${GUROBI_CXX_LIBRARY_DEBUG}"
        "${GUROBI_C_LIBRARY}")
endif()
