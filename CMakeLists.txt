cmake_minimum_required(VERSION 3.21)
project(SimpleQtApp LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(simpleqt WIN32
    main.cpp
    src/common/Dto.h
    src/core/CoreService.cpp
    src/core/CoreService.h
    src/core/workers/SelfTestWorker.cpp
    src/core/workers/SelfTestWorker.h
    src/core/workers/ScanWorker.cpp
    src/core/workers/ScanWorker.h
    src/core/workers/JobWorker.cpp
    src/core/workers/JobWorker.h
    src/core/workers/EnvWorker.cpp
    src/core/workers/EnvWorker.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/DynamicForm.cpp
    src/ui/DynamicForm.h
)

target_include_directories(simpleqt PRIVATE src)
target_link_libraries(simpleqt PRIVATE Qt6::Widgets)
