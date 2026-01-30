cmake_minimum_required(VERSION 3.8)
project(MYACTUA_EtherCat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IGH_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ethercat)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../SOEM/build/install/include
    ${IGH_PATH}/include
)

link_directories(
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../SOEM/build/install/lib
    ${IGH_PATH}/lib
)

add_library(MYACTUA_EtherCat
    # src/Motor.cpp
    src/motor_control.cpp
    src/CiA402.cpp
    # src/EthercatAdapterSOEM.cpp
    src/EthercatAdapterIGH.cpp
)

target_include_directories(MYACTUA_EtherCat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MYACTUA_EtherCat
    # soem
    ethercat
    pthread
    rt
)

add_executable(simple_test examples/simple_test.cpp)
add_executable(debug_tool examples/debug_tool.cpp)

target_link_libraries(simple_test
    MYACTUA_EtherCat
)

target_link_libraries(debug_tool
    MYACTUA_EtherCat
)