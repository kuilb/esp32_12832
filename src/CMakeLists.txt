# 1. 搜寻当前目录下的 main.c 等文件
file(GLOB app_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")

# 2. 自动搜寻渲染库和驱动库下的所有 C 文件（包括字体）
file(GLOB_RECURSE gfx_sources "${CMAKE_SOURCE_DIR}/lib/display_gfx/*.c")
file(GLOB_RECURSE drv_sources "${CMAKE_SOURCE_DIR}/lib/st7567_driver/*.c")

idf_component_register(
    SRCS
        ${app_sources}
        ${gfx_sources}
        ${drv_sources}
    INCLUDE_DIRS
        "."
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/lib/display_gfx"
        "${CMAKE_SOURCE_DIR}/lib/st7567_driver"
)