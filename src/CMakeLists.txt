set(GIT_VIEW_SOURCES
    main.cpp
    AppLaunchOptions.cpp
    core/Commit.cpp
    core/Branch.cpp
    core/Repo.cpp
    core/GraphLayout.cpp
    core/CommitDetails.cpp
    core/WorkingTreeChange.cpp
    git/PathUtils.cpp
    git/GitProcessRunner.cpp
    git/LogParser.cpp
    git/DiffParser.cpp
    git/StatusParser.cpp
    git/GitService.cpp
    ui/MainWindow.cpp
    ui/CommitGraphWidget.cpp
    ui/CommitHistoryView.cpp
    ui/CommitDetailsPanel.cpp
    ui/CommitDetailsDialog.cpp
    ui/WorkingChangesPanel.cpp
    ui/WorkingFileEditorDialog.cpp
    ui/RemotesDialog.cpp
    ui/RepoTerminalPanel.cpp
    ui/PtySession.cpp
    ui/VtermTerminalWidget.cpp
    ui/CodeSyntax.cpp
    ui/EditorTheme.cpp
    ui/DiffHighlighter.cpp
    ui/DialogTitleBar.cpp
    ui/DiffViewerDialog.cpp
    ui/FileHistoryDialog.cpp
    ui/DiffViewerWidget.cpp
    ui/SourceChangeHighlighter.cpp
    ui/SourceCodeView.cpp
)

include(GNUInstallDirs)

set(GIT_VIEW_RESOURCES ${CMAKE_SOURCE_DIR}/resources/git_view.qrc)

add_executable(git_view ${GIT_VIEW_SOURCES} ${GIT_VIEW_RESOURCES})

target_include_directories(git_view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GIT_VIEW_GENERATED_DIR}
)

target_link_libraries(git_view PRIVATE Qt6::Widgets)

if(UNIX)
    target_link_libraries(git_view PRIVATE libvterm)
    target_compile_definitions(git_view PRIVATE GIT_VIEW_PTY_TERMINAL=1)
endif()

if(APPLE)
    set_target_properties(git_view PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gitview.app"
        MACOSX_BUNDLE_BUNDLE_NAME "git_view"
        MACOSX_BUNDLE_DISPLAY_NAME "git_view"
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_COPYRIGHT "Copyright © git_view contributors"
    )
    set(_git_view_icon_png ${CMAKE_SOURCE_DIR}/resources/icons/git_view.png)
    target_sources(git_view PRIVATE ${_git_view_icon_png})
    set_source_files_properties(${_git_view_icon_png} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    install(TARGETS git_view BUNDLE DESTINATION .)
else()
    install(TARGETS git_view RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/git_view.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME git_view.svg)
    foreach(_icon_size 48 128 256 512)
        install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/git_view_${_icon_size}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_icon_size}x${_icon_size}/apps
                RENAME git_view.png)
    endforeach()
endif()
