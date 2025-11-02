cmake_minimum_required(VERSION 3.31)
project(qrb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if (MSVC)
    add_compile_options(/utf-8)
endif ()

if (MINGW)
    add_compile_options(-finput-charset=utf-8 -fexec-charset=utf-8)
    add_link_options(-municode "LINKER:--no-insert-timestamp")
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
                -O2
                -DNDEBUG
                -ftree-vectorize
                -ffunction-sections
                -fdata-sections
                -fvisibility=hidden
                -fvisibility-inlines-hidden
        )
        add_link_options(
                "LINKER:-O2"
                "LINKER:--as-needed"
                "LINKER:--strip-all"
                "LINKER:--gc-sections"
        )
    endif ()
endif ()

set(OpenCV_STATIC ON)
find_package(OpenCV REQUIRED)

file(GLOB ZXING_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zxing/src/*.cpp")
add_library(zxing STATIC ${ZXING_SOURCE})
target_include_directories(zxing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zxing/include")

file(GLOB_RECURSE QRB_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${QRB_SOURCE})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE zxing ${OpenCV_LIBS})