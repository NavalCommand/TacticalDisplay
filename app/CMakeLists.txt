if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

find_package(GTest CONFIG REQUIRED)
add_executable(NavalCommand.TacticalDisplay.Development NavalCommand/TacticalDisplay/Development.cpp)
target_link_libraries(NavalCommand.TacticalDisplay.Development PRIVATE NavalCommand.TacticalDisplay GTest::gmock)
NavalCommandGetDependencySource(https://github.com/NavalCommand/Domain.Api.git domainSource)
target_include_directories(NavalCommand.TacticalDisplay.Development PRIVATE "${domainSource}/mock")
NavalCommandConfigureTarget(NavalCommand.TacticalDisplay.Development)
if(BUILD_TESTING)
    add_test(NAME NavalCommand.TacticalDisplay.LocalProviders
        COMMAND NavalCommand.TacticalDisplay.Development --duration-ms 1000)
endif()
