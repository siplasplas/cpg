cmake_minimum_required(VERSION 3.26)
project(cpg VERSION 1.0.0)

add_definitions("-Werror=return-type -Werror=unused-result")

set(CMAKE_CXX_STANDARD 17)

find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(ZLIB REQUIRED)

# Biblioteka cpg
add_library(cpg STATIC
        src/Codepage.cpp
        src/TableCodepage.cpp
        src/BigTableCodepage.cpp
        src/UtfCodepage.cpp
        src/CpManager.cpp
        src/PolishDosCodepages.cpp
        src/Language.cpp
        src/Detector.cpp
        src/NgramModel.cpp)

target_include_directories(cpg PUBLIC include)
target_link_libraries(cpg PUBLIC ICU::uc ICU::i18n ZLIB::ZLIB)

# Demo konwerter
add_executable(convcp
        demo/converter.cpp
        demo/Args.cpp)
target_link_libraries(convcp cpg)

# tableCreator (narzędzie do generowania tablic)
add_executable(tableCreator tableCreator.cpp)
target_link_libraries(tableCreator cpg)

add_executable(cpg_corpus tools/corpus.cpp)
target_link_libraries(cpg_corpus cpg)
target_compile_definitions(cpg_corpus PRIVATE
        CPG_LANGUAGES_TXT="${CMAKE_CURRENT_SOURCE_DIR}/languages.txt")

# Testy (GoogleTest)
option(CPG_BUILD_TESTS "Build unit tests" ON)
if(CPG_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(cpg_tests
            test/UtfCodepageTest.cpp
            test/CpManagerTest.cpp
            test/LanguageTest.cpp
            test/DetectorTest.cpp
            test/NgramModelTest.cpp)
    target_link_libraries(cpg_tests cpg GTest::gtest GTest::gtest_main)
    target_compile_definitions(cpg_tests PRIVATE
            CPG_LANGUAGES_TXT="${CMAKE_CURRENT_SOURCE_DIR}/languages.txt"
            CPG_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/models")
    include(GoogleTest)
    gtest_discover_tests(cpg_tests)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME CorpusTool.Workflow
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test/CorpusToolTest.py
                        $<TARGET_FILE:cpg_corpus>)
    endif()
endif()
