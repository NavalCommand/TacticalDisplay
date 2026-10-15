find_package(imgui CONFIG REQUIRED)
add_library(NavalCommand.TacticalDisplay STATIC NavalCommand/TacticalDisplay/Display.cpp)
target_include_directories(NavalCommand.TacticalDisplay PUBLIC "${PROJECT_SOURCE_DIR}/public")
target_link_libraries(NavalCommand.TacticalDisplay PUBLIC NavalCommand.Units.Api NavalCommand.Observations.Api
    NavalCommand.Operations.Api NavalCommand.Environment.Api NavalCommand.Simulation.Api)
target_link_libraries(NavalCommand.TacticalDisplay PUBLIC NavalCommand.Symbology.Nato.Api)
target_link_libraries(NavalCommand.TacticalDisplay PRIVATE NavalCommand.Symbology.Nato.Api.Domain imgui::imgui d3d11 dxgi)
NavalCommandConfigureTarget(NavalCommand.TacticalDisplay)
