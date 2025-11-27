cmake_minimum_required(VERSION 3.21)
project(SimpleQtApp LANGUAGES CXX)

# Application version; can be overridden by -DAPP_VERSION=1.2.3 (used for update feed)
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "0.1.0")
endif()
if(NOT DEFINED UPDATE_FEED_URL)
    # Default to GitHub Releases; override with -DUPDATE_FEED_URL=...
    set(UPDATE_FEED_URL "https://github.com/OWNER/REPO/releases/latest/download/update.json")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(APP_VERSION="${APP_VERSION}" UPDATE_FEED_URL="${UPDATE_FEED_URL}")

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network)

include(FetchContent)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

add_library(corelib STATIC
    src/common/Dto.h
    src/core/CoreService.cpp
    src/core/CoreService.h
    src/core/LoggingBridge.cpp
    src/core/LoggingBridge.h
    src/core/workers/SelfTestWorker.cpp
    src/core/workers/SelfTestWorker.h
    src/core/workers/ScanWorker.cpp
    src/core/workers/ScanWorker.h
    src/core/workers/JobWorker.cpp
    src/core/workers/JobWorker.h
    src/core/workers/EnvWorker.cpp
    src/core/workers/EnvWorker.h
)
target_include_directories(corelib PUBLIC src)
target_link_libraries(corelib PUBLIC Qt6::Core yaml-cpp)
set_target_properties(corelib PROPERTIES AUTOMOC ON)

add_library(uilib STATIC
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/DynamicForm.cpp
    src/ui/DynamicForm.h
    src/ui/ToolWindow.cpp
    src/ui/ToolWindow.h
)
target_include_directories(uilib PUBLIC src)
target_link_libraries(uilib PUBLIC Qt6::Widgets Qt6::Network corelib)
set_target_properties(uilib PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

add_executable(simpleqt WIN32 main.cpp)
target_link_libraries(simpleqt PRIVATE Qt6::Widgets uilib corelib)
