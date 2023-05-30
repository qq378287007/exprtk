set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}/itemData
    ${CMAKE_SOURCE_DIR}/tree
)

get_filename_component(DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${DIR_NAME})

find_package(Qt5 REQUIRED COMPONENTS Widgets)
link_libraries(Qt5::Widgets)

#QMAKE_CXXFLAGS += "-Wa,-mbig-obj"
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -pedantic-errors -Wall -Wextra -Werror -Wno-long-long -O1 -lstdc++ -lm -Wa,-mbig-obj")

file(GLOB ALL_FILE
    "itemData/*.h" "itemData/*.cpp"
    "tree/*.h" "tree/*.cpp"
    "*.hpp" "*.cpp"
)
add_executable(${DIR_NAME} #WIN32
    ${ALL_FILE}
)
