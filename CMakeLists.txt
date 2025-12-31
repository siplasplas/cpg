cmake_minimum_required(VERSION 3.26)
project(cpg VERSION 1.0.0)

add_definitions("-Werror=return-type -Werror=unused-result")

set(CMAKE_CXX_STANDARD 17)

# Biblioteka cpg
add_library(cpg STATIC
        src/Codepage.cpp
        src/TableCodepage.cpp
        src/BigTableCodepage.cpp
        src/UtfCodepage.cpp
        src/CpManager.cpp
        src/Language.cpp)

target_include_directories(cpg PUBLIC include utf/include)

# Demo konwerter
add_executable(convcp
        demo/converter.cpp
        demo/Args.cpp)
target_link_libraries(convcp cpg)

# tableCreator (narzędzie do generowania tablic)
add_executable(tableCreator tableCreator.cpp)
target_link_libraries(tableCreator cpg)
