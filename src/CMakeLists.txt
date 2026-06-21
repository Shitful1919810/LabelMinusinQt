set(LABELMINUS_CORE_SOURCES
    core/ApplicationTheme.cpp
    core/AppPreferences.cpp
    core/Label.cpp
    core/LabelPlusDocument.cpp
    core/Project.cpp
    core/UndoStack.cpp
)

set(LABELMINUS_SERVICE_SOURCES
    services/ArchiveReader.cpp
    services/LabelEditController.cpp
    services/OcrProcess.cpp
    services/ProjectController.cpp
    services/SessionStateStore.cpp
)

set(LABELMINUS_UI_SOURCES
    ui/GroupFilterComboBox.cpp
    ui/ImageCanvas.cpp
    ui/LabelEditDelegates.cpp
    ui/LabelTableModel.cpp
    ui/MainWindow.cpp
    ui/PreferenceDialog.cpp
    ui/ThemeManager.cpp
)

qt_add_resources(LABELMINUS_RESOURCE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../resources/themes/breeze/breeze.qrc"
)

qt_add_executable(LabelMinus
    WIN32
    MACOSX_BUNDLE
    main.cpp
    ${LABELMINUS_CORE_SOURCES}
    ${LABELMINUS_SERVICE_SOURCES}
    ${LABELMINUS_UI_SOURCES}
    ${LABELMINUS_RESOURCE_SOURCES}
)

target_include_directories(LabelMinus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(LabelMinus PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(TARGET Qt6::Svg)
    target_link_libraries(LabelMinus PRIVATE Qt6::Svg)
endif()

if(LibArchive_FOUND)
    target_link_libraries(LabelMinus PRIVATE LibArchive::LibArchive)
    target_compile_definitions(LabelMinus PRIVATE LABELMINUS_HAS_LIBARCHIVE=1)
endif()

if(COMMAND qt_add_translations)
    qt_add_translations(LabelMinus
        TS_FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_zh_CN.ts"
            "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_en_US.ts"
        RESOURCE_PREFIX "/i18n"
    )
endif()

add_custom_command(TARGET LabelMinus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../preference.json"
        "$<TARGET_FILE_DIR:LabelMinus>/preference.json"
    VERBATIM
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy_windows
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                "$<TARGET_FILE:LabelMinus>"
            DEPENDS LabelMinus
            COMMENT "Deploying Qt runtime dependencies with windeployqt"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt was not found. Add the Qt bin directory to PATH to enable the deploy_windows target.")
    endif()
endif()

labelminus_enable_warnings(LabelMinus)

set_target_properties(LabelMinus PROPERTIES
    OUTPUT_NAME "labelminus"
    MACOSX_BUNDLE_BUNDLE_NAME "LabelMinus"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.labelminus.LabelMinus"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
)

if(LABELMINUS_ENABLE_STATIC_QT_EXPERIMENT)
    if(NOT WIN32)
        message(FATAL_ERROR "LABELMINUS_ENABLE_STATIC_QT_EXPERIMENT is only supported on Windows.")
    endif()

    qt_add_executable(LabelMinusStatic
        WIN32
        main.cpp
        ${LABELMINUS_CORE_SOURCES}
        ${LABELMINUS_SERVICE_SOURCES}
        ${LABELMINUS_UI_SOURCES}
        ${LABELMINUS_RESOURCE_SOURCES}
    )

    target_include_directories(LabelMinusStatic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(LabelMinusStatic PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

    if(TARGET Qt6::Svg)
        target_link_libraries(LabelMinusStatic PRIVATE Qt6::Svg)
    endif()

    if(LibArchive_FOUND)
        target_link_libraries(LabelMinusStatic PRIVATE LibArchive::LibArchive)
        target_compile_definitions(LabelMinusStatic PRIVATE LABELMINUS_HAS_LIBARCHIVE=1)
    endif()

    if(COMMAND qt_add_translations)
        qt_add_translations(LabelMinusStatic
            TS_FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_zh_CN.ts"
                "${CMAKE_CURRENT_SOURCE_DIR}/../translations/labelminus_en_US.ts"
            RESOURCE_PREFIX "/i18n"
        )
    endif()

    if(COMMAND qt_import_plugins AND TARGET Qt6::QWindowsIntegrationPlugin)
        qt_import_plugins(LabelMinusStatic INCLUDE Qt6::QWindowsIntegrationPlugin)
    endif()

    if(MSVC)
        set_target_properties(LabelMinusStatic PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    else()
        target_link_options(LabelMinusStatic PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    labelminus_enable_warnings(LabelMinusStatic)

    set_target_properties(LabelMinusStatic PROPERTIES
        OUTPUT_NAME "labelminus-static"
    )
endif()

install(TARGETS LabelMinus
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
