cmake_minimum_required(VERSION 3.16)

project(test_runner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(GTest REQUIRED)

set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
set(APPROVAL_TESTS_HEADER ${VENDOR_DIR}/ApprovalTests.hpp)

if(NOT EXISTS ${APPROVAL_TESTS_HEADER})
    message(STATUS "Downloading ApprovalTests.hpp...")
    file(DOWNLOAD
        "https://github.com/approvals/ApprovalTests.cpp/releases/download/v.10.13.0/ApprovalTests.v.10.13.0.hpp"
        ${APPROVAL_TESTS_HEADER}
        STATUS DOWNLOAD_STATUS
    )
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/checker
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/logging
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/codegen
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/loader
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/preprocessor
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/json_export
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

set(CODEGEN_SOURCES
    ../src/codegen/ir/ir_printer.cpp
    ../src/codegen/ir/ir_generator.cpp
    ../src/codegen/ir/ir_visitor.cpp
    ../src/codegen/x86_64/asm.cpp
)

set(CHECKER_SOURCES
    ../src/checker/typechecker.cpp
)

set(PARSER_SOURCES
    ../src/parser/parser.cpp
    ../src/parser/ast.cpp
    ../src/parser/symtab.cpp
    ../src/parser/visitor.cpp
    ../src/parser/types.cpp
)

set(LEXER_SOURCES
    ../src/lexer/lexer.cpp
    ../src/lexer/token.cpp
)

set(SHARED_SOURCES
    ../src/loader/source_loader.cpp
    ../src/memory/arena.cpp
    ../src/preprocessor/preprocessor.cpp
)

set(LOGGING_SOURCES
    ../src/logging/diagnostic.cpp
    ../src/logging/logger.cpp
)

add_executable(${PROJECT_NAME}
    main.cpp
    ../src/driver.cpp
    ../src/json_export/json_exporter.cpp
    test_lexer.cpp
    test_parser.cpp
    test_symtab.cpp
    test_ir.cpp
    test_x86_64_gen.cpp
    ${LEXER_SOURCES}
    ${PARSER_SOURCES}
    ${LOGGING_SOURCES}
    ${CHECKER_SOURCES}
    ${CODEGEN_SOURCES}
    ${SHARED_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
