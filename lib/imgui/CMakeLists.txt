project(savepoint_imgui)
add_library(savepoint_imgui
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_impl_sdl3.cpp
    imgui_impl_sdlrenderer3.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)
target_include_directories(savepoint_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(savepoint_imgui PROPERTIES CXX_STANDARD 23)
target_link_libraries(savepoint_imgui PUBLIC SDL3::SDL3)
