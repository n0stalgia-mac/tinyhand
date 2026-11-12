cmake_minimum_required(VERSION 3.10)
project(TinyHand C)

set(TINYHAND_FILES
    src/tinyhand.c
    src/TinyHand.r
)

# TinyHand.r owns the 68000 SIZE. On PowerPC that resource is compiled
# first and would override RetroPPCAPPL.r, so append TinyHandPPC.r.
if(CMAKE_SYSTEM_NAME STREQUAL "RetroPPC")
    list(APPEND TINYHAND_FILES src/TinyHandPPC.r)
endif()

add_application(TinyHand
    CREATOR "tHnd"
    TYPE "APPL"
    ${TINYHAND_FILES}
)

target_compile_options(TinyHand PRIVATE -std=gnu99 -Os)

# 68000 and soft-float are Plus flags. A later PowerPC configure uses
# CMAKE_SYSTEM_NAME RetroPPC and must not see them.
if(CMAKE_SYSTEM_NAME STREQUAL "Retro68")
    target_compile_options(TinyHand PRIVATE -m68000 -msoft-float -ffunction-sections)
    set_target_properties(TinyHand PROPERTIES
        LINK_FLAGS "-m68000 -msoft-float -Wl,-gc-sections -Wl,--mac-strip-macsbug"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Retro68" OR CMAKE_SYSTEM_NAME STREQUAL "RetroPPC")
    add_custom_command(TARGET TinyHand_APPL POST_BUILD
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/set_bundle.py
                ${CMAKE_CURRENT_BINARY_DIR}/TinyHand.bin
                ${CMAKE_CURRENT_BINARY_DIR}/TinyHand.dsk
                TinyHand
        COMMENT "Finder bundle bit on TinyHand"
    )
endif()
