cmake_minimum_required(VERSION 3.26)
project(tableCreator)

add_definitions("-Werror=return-type -Werror=unused-result")

set(CMAKE_CXX_STANDARD 17)

include_directories(utf/include)

add_executable(tableCreator tableCreator.cpp
        Language.h
        Language.cpp)

add_executable(convcp converter.cpp Args.cpp
        CpManager.h
        Language.h
        Language.cpp
        CpManager.cpp
        Codepage.h
        UtfCodepage.h
        TableCodepage.h
        UtfCodepage.cpp
        TableCodepage.cpp
        BigTableCodepage.cpp
        Codepage.cpp)