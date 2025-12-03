set(SOURCES
    SamplePlugin.cpp
)

set(HEADERS
    SamplePlugin.h
)

add_library(SamplePlugin SHARED ${SOURCES} ${HEADERS})

target_link_libraries(SamplePlugin
    EagleCore
    Qt5::Core
    Qt5::Widgets
)

target_include_directories(SamplePlugin PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(SamplePlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# 定义插件导出宏
target_compile_definitions(SamplePlugin PRIVATE
    EAGLE_PLUGIN_EXPORT
)
