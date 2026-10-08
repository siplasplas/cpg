cmake_minimum_required(VERSION 3.26)
project(cpg VERSION 1.1.0)

add_definitions("-Werror=return-type -Werror=unused-result")

set(CMAKE_CXX_STANDARD 17)

# Tools, tests and install rules are on by default only when cpg is the
# top-level project, so that a parent project (add_subdirectory /
# FetchContent) gets just the library and does not need GoogleTest.
option(CPG_BUILD_TOOLS "Build convcp, tableCreator and cpg_corpus" ${PROJECT_IS_TOP_LEVEL})
option(CPG_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(CPG_INSTALL "Generate install rules" ${PROJECT_IS_TOP_LEVEL})

find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(ZLIB REQUIRED)

# Biblioteka cpg
add_library(cpg STATIC
        src/Codepage.cpp
        src/TableCodepage.cpp
        src/BigTableCodepage.cpp
        src/UtfCodepage.cpp
        src/CpManager.cpp
        src/Converter.cpp
        src/PolishDosCodepages.cpp
        src/Language.cpp
        src/Detector.cpp
        src/NgramModel.cpp)

target_include_directories(cpg PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(cpg PUBLIC ICU::uc ICU::i18n ZLIB::ZLIB)
add_library(cpg::cpg ALIAS cpg)

# Runtime data for detection: language metadata and n-gram models. A parent
# project reads these paths after add_subdirectory / FetchContent; the
# installed package sets them in cpgConfig.cmake.
set(CPG_LANGUAGES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/languages.txt"
        CACHE INTERNAL "cpg languages.txt")
set(CPG_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/models"
        CACHE INTERNAL "cpg n-gram models directory")

if(CPG_BUILD_TOOLS)
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
endif()

# Install: library, headers, languages.txt and models under share/cpg, and a
# CMake package so that find_package(cpg) works.
if(CPG_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    install(TARGETS cpg EXPORT cpgTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY include/cpg DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES languages.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/cpg)
    install(DIRECTORY data/models/ DESTINATION ${CMAKE_INSTALL_DATADIR}/cpg/models
            FILES_MATCHING PATTERN "*.ngram")
    install(EXPORT cpgTargets NAMESPACE cpg::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpg)
    configure_package_config_file(cmake/cpgConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/cpgConfig.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpg
            PATH_VARS CMAKE_INSTALL_DATADIR)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/cpgConfigVersion.cmake
            COMPATIBILITY SameMajorVersion)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cpgConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/cpgConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpg)
endif()

# Testy (GoogleTest)
if(CPG_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(cpg_tests
            test/UtfCodepageTest.cpp
            test/CpManagerTest.cpp
            test/ConverterTest.cpp
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
    if(Python3_Interpreter_FOUND AND TARGET cpg_corpus)
        add_test(NAME CorpusTool.Workflow
                COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test/CorpusToolTest.py
                        $<TARGET_FILE:cpg_corpus>)
    endif()
endif()
