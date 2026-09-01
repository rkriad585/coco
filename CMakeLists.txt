cmake_minimum_required(VERSION 3.25)
project(coco CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # feeds clangd/LSP

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_library(coco_support INTERFACE)
target_include_directories(coco_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(coco_lex STATIC
    src/lex/lexer.cpp
)
target_link_libraries(coco_lex PUBLIC coco_support)

add_library(coco_ast STATIC
    src/ast/ast.cpp
    src/ast/ast_dump.cpp
)
target_link_libraries(coco_ast PUBLIC coco_support)

add_library(coco_parser STATIC
    src/parser/parser.cpp
)
target_link_libraries(coco_parser PUBLIC coco_ast coco_lex)

add_library(coco_sema STATIC
    src/sema/checker.cpp
)
target_link_libraries(coco_sema PUBLIC coco_parser)

add_library(coco_vm STATIC
    src/vm/compiler.cpp
)
target_link_libraries(coco_vm PUBLIC coco_sema)

add_library(coco_interp STATIC
    src/interp/runtime.cpp
)
target_link_libraries(coco_interp PUBLIC coco_vm)

add_executable(cocolex tools/cocolex.cpp)
target_link_libraries(cocolex PRIVATE coco_lex)

add_executable(cocoparse tools/cocoparse.cpp)
target_link_libraries(cocoparse PRIVATE coco_parser)

add_executable(cococheck tools/cococheck.cpp)
target_link_libraries(cococheck PRIVATE coco_sema)

add_executable(cocorun tools/cocorun.cpp)
target_link_libraries(cocorun PRIVATE coco_interp)

add_executable(coco tools/coco.cpp)
target_link_libraries(coco PRIVATE coco_interp ws2_32)

foreach(tgt coco_lex coco_ast coco_parser coco_sema coco_vm coco_interp)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
