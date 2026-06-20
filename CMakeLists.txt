cmake_minimum_required(VERSION 3.20)
project(ZGPookingOverlayKit LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ZGPookingOverlayKit SHARED
    src/ZGPookingEngine.cpp
    src/ZGPookingFrameScanner.cpp
    src/ZGPookingOverlayController.mm
    bridges/ZGPookingOverlayExports.mm
)

target_include_directories(ZGPookingOverlayKit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(ZGPookingOverlayKit PRIVATE
    -fobjc-arc
)

target_link_libraries(ZGPookingOverlayKit
    "-framework Foundation"
    "-framework UIKit"
    "-framework QuartzCore"
    "-framework CoreGraphics"
)

set_target_properties(ZGPookingOverlayKit PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION A
    MACOSX_FRAMEWORK_IDENTIFIER com.zg.pookingoverlaykit
    PUBLIC_HEADER "include/ZGPookingOverlayController.h;include/ZGPookingOverlayExports.h;include/ZGPookingEngine.hpp;include/ZGPookingFrameScanner.hpp;include/ZGOverlayTypes.hpp"
)
