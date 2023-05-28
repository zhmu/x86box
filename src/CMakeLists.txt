add_executable(x86box cpux86.cc dos.cc file.cc hostio.cc io.cc main.cc memory.cc vectors.cc vga.cc ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h)
target_include_directories(x86box PRIVATE ${SDL_INCLUDE_DIRS})
target_link_libraries(x86box ${SDL_LIBRARIES})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../doc/font2h.pl < ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08 > ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08
)
target_include_directories(x86box PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
