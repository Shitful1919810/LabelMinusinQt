set(LABELMINUS_CORE_SOURCES
    core/AppPreferences.cpp
    core/Label.cpp
    core/LabelPlusDocument.cpp
    core/Project.cpp
    core/UndoStack.cpp
)

set(LABELMINUS_SERVICE_SOURCES
    services/ArchiveReader.cpp
    services/OcrProcess.cpp
)

set(LABELMINUS_UI_SOURCES
    ui/GroupFilterComboBox.cpp
    ui/ImageCanvas.cpp
    ui/LabelEditDelegates.cpp
    ui/LabelTableModel.cpp
    ui/MainWindow.cpp
    ui/PreferenceDialog.cpp
)

qt_add_executable(LabelMinus
    WIN32
    MACOSX_BUNDLE
    main.cpp
    ${LABELMINUS_CORE_SOURCES}
    ${LABELMINUS_SERVICE_SOURCES}
    ${LABELMINUS_UI_SOURCES}
)

target_include_directories(LabelMinus
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(LabelMinus
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

if(LibArchive_FOUND)
    target_link_libraries(LabelMinus PRIVATE LibArchive::LibArchive)
    target_compile_definitions(LabelMinus PRIVATE LABELMINUS_HAS_LIBARCHIVE=1)
endif()

if(COMMAND qt_add_translations)
    qt_add_translations(LabelMinus
        TS_FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_zh_CN.ts"
            "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_en_US.ts"
        RESOURCE_PREFIX
            "/i18n"
    )
endif()

add_custom_command(TARGET LabelMinus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../preference.json"
        "$<TARGET_FILE_DIR:LabelMinus>/preference.json"
    VERBATIM
)

labelminus_enable_warnings(LabelMinus)

set_target_properties(LabelMinus PROPERTIES
    OUTPUT_NAME "labelminus"
    MACOSX_BUNDLE_BUNDLE_NAME "LabelMinus"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.labelminus.LabelMinus"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

install(TARGETS LabelMinus
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
