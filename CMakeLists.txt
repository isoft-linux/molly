project(molly)

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_AUTOMOC ON)
set(QT_MIN_VERSION "5.6.0")

include(FeatureSummary)

# Set version
set(MOLLY_VERSION_MAJOR 0)
set(MOLLY_VERSION_MINOR 0)
set(MOLLY_VERSION_PATCH 1)
set(MOLLY_VERSION_STRING "${MOLLY_VERSION_MAJOR}.${MOLLY_VERSION_MINOR}.${MOLLY_VERSION_PATCH}")
set(PROJECT_VERSION MOLLY_VERSION_STRING)
add_definitions("-DMOLLY_VERSION_STRING=\"${MOLLY_VERSION_STRING}\"")
add_definitions("-DPROJECT_VERSION=\"${PROJECT_VERSION}\"")

# Definitions
set(CMAKE_CXX_STANDARD 11)
set(DATADIR ${CMAKE_INSTALL_PREFIX}/share)
add_definitions("-DDATADIR=\"${DATADIR}\"")
set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
add_definitions("-DLOCALEDIR=\"${LOCALEDIR}\"")

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Handle build type
if(CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
    message(STATUS "Debug build")
    add_definitions(-DDEBUG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
    message(STATUS "Release build")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# Default absolute paths
if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()
if(NOT DEFINED CMAKE_INSTALL_LOCALSTATEDIR)
    set(CMAKE_INSTALL_LOCALSTATEDIR "/var")
endif()

find_package(Qt5 REQUIRED COMPONENTS Core LinguistTools Widgets DBus)
if(CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
    find_package(Qt5Test REQUIRED)
endif()
find_package(udisks2qt5 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPARTCLONE REQUIRED partclone-0.3.5)

find_package(OpenMP)
set_package_properties(OpenMP PROPERTIES DESCRIPTION "The OpenMP libraries"
                       URL "http://www.openmp.org"
                       TYPE OPTIONAL
                       PURPOSE "Only test for GCC 6.x"
                      )
if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(OPENMP_LIBRARIES "-lgomp")
    add_definitions("-DOPENMP_FOUND")
endif()

# Add subdirectories
add_subdirectory(src)
if(CMAKE_BUILD_TYPE MATCHES [Dd]ebug)
    add_subdirectory(tests)
endif()

# Display feature summary
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
