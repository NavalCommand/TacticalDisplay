cmake_minimum_required(VERSION 4.2)
cmake_policy(SET CMP0168 NEW)
project(NavalCommand.TacticalDisplay VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(ncbuild GIT_REPOSITORY https://github.com/NavalCommand/Build.CMake.git GIT_TAG main)
FetchContent_MakeAvailable(ncbuild)
include("${ncbuild_SOURCE_DIR}/NavalCommand.Build.CMake.cmake")

NavalCommandAddDependency(https://github.com/NavalCommand/Domain.Api.git 53e969150af4719f12e94b749edceb0313f91b9a)
NavalCommandAddDependency(https://github.com/NavalCommand/Symbology.Nato.Api.git f4559146c3f1e4c35aea5d08f41d76dcac113172)

NavalCommandConfigureModule()
