cmake_minimum_required(VERSION 3.10)
project(Einkaufsprojekt C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

file(GLOB SRC_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)

add_executable(einkaufsprojekt ${SRC_FILES})

target_include_directories(einkaufsprojekt PRIVATE ${CMAKE_SOURCE_DIR}/include)

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/data" DATA_DIR_PATH)
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/web" WEB_DIR_PATH)
file(TO_CMAKE_PATH "${DATA_DIR_PATH}/einkaufsliste.txt" SHOPPING_LIST_PATH_VALUE)

target_compile_definitions(einkaufsprojekt PRIVATE
        DATEN_VERZEICHNIS="${DATA_DIR_PATH}"
        WEB_VERZEICHNIS="${WEB_DIR_PATH}"
        EINKAUFSLISTE_PFAD="${SHOPPING_LIST_PATH_VALUE}"
        DATA_DIRECTORY="${DATA_DIR_PATH}"
        WEB_DIRECTORY="${WEB_DIR_PATH}"
        SHOPPING_LIST_PATH="${SHOPPING_LIST_PATH_VALUE}")

set_target_properties(einkaufsprojekt PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
    target_link_libraries(einkaufsprojekt PRIVATE ws2_32)
endif()
