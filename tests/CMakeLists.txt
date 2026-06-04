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
