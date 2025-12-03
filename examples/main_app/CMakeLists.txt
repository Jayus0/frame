set(SOURCES
    main.cpp
    MainWindow.cpp
)

set(HEADERS
    MainWindow.h
)

set(UI_FILES
    MainWindow.ui
)

qt5_wrap_ui(UI_SOURCES ${UI_FILES})

add_executable(EagleApp ${SOURCES} ${UI_SOURCES})

target_link_libraries(EagleApp
    EagleCore
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
)

target_include_directories(EagleApp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(EagleApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
