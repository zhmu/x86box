add_executable(x86box cpux86.cpp dos.cpp file.cpp hostio.cpp io.cpp main.cpp memory.cpp vectors.cpp vga.cpp keyboard.cpp ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h)
target_include_directories(x86box PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(x86box ${SDL2_LIBRARIES})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../doc/font2h.pl < ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08 > ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08
)
target_include_directories(x86box PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
