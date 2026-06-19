find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

qt_add_executable(LabelMinusTests
    LabelTests.cpp
    ../src/core/Label.cpp
)

target_include_directories(LabelMinusTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(LabelMinusTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

labelminus_enable_warnings(LabelMinusTests)

add_test(NAME LabelMinusTests COMMAND $<TARGET_FILE:LabelMinusTests>)
