add_executable(x86box
    cpu/cpux86.cpp
    bus/io.cpp
    bus/memory.cpp
    main.cpp
    hw/vga.cpp
    hw/keyboard.cpp
    hw/ata.cpp
    hw/pic.cpp
    hw/pit.cpp
    hw/dma.cpp
    hw/ppi.cpp
    hw/rtc.cpp
    hw/fdc.cpp
    platform/imagelibrary.cpp
    platform/tickprovider.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h)
target_include_directories(x86box PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(x86box PRIVATE ${SDL2_LIBRARIES})
target_sources(x86box PRIVATE platform/hostio.cpp)
target_sources(x86box PRIVATE cpu/disassembler.cpp)

target_link_libraries(x86box PRIVATE spdlog::spdlog argparse)
target_link_libraries(x86box PRIVATE capstone)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../doc/font2h.pl < ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08 > ${CMAKE_CURRENT_BINARY_DIR}/vgafont.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../doc/vga-rom.f08
)
target_include_directories(x86box PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
