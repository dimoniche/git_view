set(LIBVTERM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(libvterm STATIC
    ${LIBVTERM_SRC_DIR}/vterm.c
    ${LIBVTERM_SRC_DIR}/encoding.c
    ${LIBVTERM_SRC_DIR}/keyboard.c
    ${LIBVTERM_SRC_DIR}/mouse.c
    ${LIBVTERM_SRC_DIR}/parser.c
    ${LIBVTERM_SRC_DIR}/pen.c
    ${LIBVTERM_SRC_DIR}/screen.c
    ${LIBVTERM_SRC_DIR}/state.c
    ${LIBVTERM_SRC_DIR}/unicode.c
)

target_include_directories(libvterm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBVTERM_SRC_DIR}
)

set_target_properties(libvterm PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    LINKER_LANGUAGE C
)

target_compile_features(libvterm PUBLIC c_std_99)

if(UNIX AND NOT APPLE)
    target_link_libraries(libvterm PUBLIC util)
endif()
