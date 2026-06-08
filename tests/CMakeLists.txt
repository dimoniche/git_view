find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_log_parser test_log_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/LogParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Commit.cpp
)

set_target_properties(test_log_parser PROPERTIES AUTOMOC ON)

target_include_directories(test_log_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_log_parser PRIVATE Qt6::Test)

add_test(NAME test_log_parser COMMAND test_log_parser)

add_executable(test_graph_layout test_graph_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/core/GraphLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Commit.cpp
)

set_target_properties(test_graph_layout PROPERTIES AUTOMOC ON)

target_include_directories(test_graph_layout PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_graph_layout PRIVATE Qt6::Test)

add_test(NAME test_graph_layout COMMAND test_graph_layout)

add_executable(test_status_parser test_status_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/StatusParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WorkingTreeChange.cpp
)

set_target_properties(test_status_parser PROPERTIES AUTOMOC ON)

target_include_directories(test_status_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_status_parser PRIVATE Qt6::Test)

add_test(NAME test_status_parser COMMAND test_status_parser)

add_executable(test_diff_parser test_diff_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/DiffParser.cpp
)

set_target_properties(test_diff_parser PROPERTIES AUTOMOC ON)

target_include_directories(test_diff_parser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_diff_parser PRIVATE Qt6::Test)

add_test(NAME test_diff_parser COMMAND test_diff_parser)

add_executable(test_git_staged_diff test_git_staged_diff.cpp
    ${CMAKE_SOURCE_DIR}/src/git/GitService.cpp
    ${CMAKE_SOURCE_DIR}/src/git/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/git/GitProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/git/LogParser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/DiffParser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/StatusParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Commit.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WorkingTreeChange.cpp
)

set_target_properties(test_git_staged_diff PROPERTIES AUTOMOC ON)

target_include_directories(test_git_staged_diff PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_git_staged_diff PRIVATE Qt6::Test)

add_test(NAME test_git_staged_diff COMMAND test_git_staged_diff)

add_executable(test_live_staged test_live_staged.cpp
    ${CMAKE_SOURCE_DIR}/src/git/GitService.cpp
    ${CMAKE_SOURCE_DIR}/src/git/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/git/GitProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/git/LogParser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/DiffParser.cpp
    ${CMAKE_SOURCE_DIR}/src/git/StatusParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Commit.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WorkingTreeChange.cpp
)

set_target_properties(test_live_staged PROPERTIES AUTOMOC ON)

target_include_directories(test_live_staged PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_live_staged PRIVATE Qt6::Test)

# Run manually when README.md is staged: GIT_VIEW_LIVE_REPO=$PWD ./build/tests/test_live_staged
