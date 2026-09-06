# Phase's source list. Overscan includes this from colossus/CMakeLists.txt.
set(COLOSSUS_RENDER_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/render.c
    ${CMAKE_CURRENT_LIST_DIR}/body.c
    ${CMAKE_CURRENT_LIST_DIR}/scene_chapters.c
    ${CMAKE_CURRENT_LIST_DIR}/scene_hand.c
    ${CMAKE_CURRENT_LIST_DIR}/scene_material_test.c
    ${CMAKE_CURRENT_LIST_DIR}/assets/painted_assets.c
    ${CMAKE_CURRENT_LIST_DIR}/assets/engine_assets.c
)
# Link libm; compile host targets with HOST_BUILD=1. render_host.c is optional
# standalone WSL review code and must NOT be linked into firmware.
