find_package(GTest CONFIG REQUIRED)
include(GoogleTest)
NavalCommandAddDependency(https://github.com/NavalCommand/Symbology.Nato.ImGui.git 3ca246c22925f9c64aed587aa8efcea4e866ce69)
find_package(imgui CONFIG REQUIRED)

add_executable(NavalCommand.TacticalDisplay.Test
    NavalCommand/TacticalDisplay/SymbolLayoutTest.cpp
    NavalCommand/TacticalDisplay/VisualSmokeTest.cpp)
target_include_directories(NavalCommand.TacticalDisplay.Test PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(NavalCommand.TacticalDisplay.Test PRIVATE NavalCommand.TacticalDisplay NavalCommand.Symbology.Nato.ImGui imgui::imgui GTest::gtest_main)
NavalCommandConfigureTarget(NavalCommand.TacticalDisplay.Test)
gtest_discover_tests(NavalCommand.TacticalDisplay.Test DISCOVERY_MODE PRE_TEST)
