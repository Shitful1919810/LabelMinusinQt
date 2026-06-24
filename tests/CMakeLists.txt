find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Test)

qt_add_executable(LabelQtTests
    LabelTests.cpp
    ../src/core/ApplicationTheme.cpp
    ../src/core/AppPreferences.cpp
    ../src/core/LabelPlusDocument.cpp
    ../src/core/Label.cpp
    ../src/core/Project.cpp
    ../src/services/AutomationOperationApplier.cpp
    ../src/services/LabelNavigator.cpp
    ../src/services/ProjectMergeService.cpp
    ../src/services/ProjectPageOrderService.cpp
)

target_include_directories(LabelQtTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(LabelQtTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

labelqt_enable_warnings(LabelQtTests)

add_test(NAME LabelQtTests COMMAND $<TARGET_FILE:LabelQtTests>)
