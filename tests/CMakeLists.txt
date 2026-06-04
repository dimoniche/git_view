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
