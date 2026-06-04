set(GIT_VIEW_SOURCES
    main.cpp
    core/Commit.cpp
    core/Branch.cpp
    core/Repo.cpp
    core/GraphLayout.cpp
    core/CommitDetails.cpp
    core/WorkingTreeChange.cpp
    git/GitProcessRunner.cpp
    git/LogParser.cpp
    git/DiffParser.cpp
    git/StatusParser.cpp
    git/GitService.cpp
    ui/MainWindow.cpp
    ui/CommitHistoryView.cpp
    ui/CommitDetailsPanel.cpp
    ui/WorkingChangesPanel.cpp
    ui/RemotesDialog.cpp
    ui/RepoTerminalPanel.cpp
    ui/DiffHighlighter.cpp
)

add_executable(git_view ${GIT_VIEW_SOURCES})

target_include_directories(git_view PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(git_view PRIVATE Qt6::Widgets)

install(TARGETS git_view RUNTIME DESTINATION bin)
