add_executable(x86box cpux86.cpp io.cpp main.cpp memory.cpp vga.cpp keyboard.cpp ata.cpp pic.cpp pit.cpp dma.cpp ppi.cpp rtc.cpp fdc.cpp ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h)
target_include_directories(x86box PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(x86box PRIVATE ${SDL2_LIBRARIES})
target_sources(x86box PRIVATE hostio.cpp)
target_sources(x86box PRIVATE disassembler.cpp)

target_link_libraries(x86box PRIVATE spdlog::spdlog argparse)
target_link_libraries(x86box PRIVATE capstone)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../doc/font2h.pl < ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08 > ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08
)
target_include_directories(x86box PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
