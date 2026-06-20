cmake_minimum_required(VERSION 3.16)
project(Z80 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(macro src/macro.cpp)

add_executable(asm src/asm.cpp)

add_executable(link src/link.cpp)

add_executable(exec src/exec.cpp)

foreach(target macro asm link exec)
    target_compile_options(${target} PRIVATE -O2)
endforeach()

option(BUILD_GUI "Construir a interface grafica Qt5 (gui)" ON)

if(BUILD_GUI)
    find_package(Qt5 COMPONENTS Widgets QUIET)

    if(Qt5_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        add_executable(gui
            gui/main.cpp
            gui/window.cpp
            include/window.hpp
        )

        target_include_directories(gui PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/gui
        )

        target_compile_definitions(gui PRIVATE QT_NO_KEYWORDS)
        target_compile_options(gui PRIVATE -O2)

        target_link_libraries(gui PRIVATE Qt5::Widgets)

        message(STATUS "Qt5 encontrado: a GUI (gui) sera construida.")
    else()
        message(WARNING
            "Qt5Widgets nao encontrado: a GUI (gui) NAO sera construida. "
            "Instale 'qtbase5-dev' (Debian/Ubuntu) ou defina Qt5_DIR para "
            "ativar a interface grafica. As ferramentas de linha de comando "
            "(macro, asm, link, exec) continuam disponiveis.")
    endif()
endif()

install(TARGETS macro asm link exec RUNTIME DESTINATION bin)
if(BUILD_GUI AND Qt5_FOUND)
    install(TARGETS gui RUNTIME DESTINATION bin)
endif()

message(STATUS "")
message(STATUS "Z80 Virtual Machine - alvos configurados:")
message(STATUS "  macro   - macro-montador (uma passagem, macros aninhadas)")
message(STATUS "  asm     - montador de dois passos (chama o macro-montador)")
message(STATUS "  link    - ligador de dois passos (relocador / carregador)")
message(STATUS "  exec    - executor/emulador Z80 (linha de comando)")
if(BUILD_GUI AND Qt5_FOUND)
    message(STATUS "  gui     - interface grafica Qt5 (pipeline completo)")
endif()
message(STATUS "")