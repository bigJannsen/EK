cmake_minimum_required(VERSION 3.10)

# Projektname und Sprache
project(Einkaufsprojekt C)

# C-Standard (modern, stabil)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optional: kompiliere mit Warnungen (empfohlen für sauberen Code)
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# ---- Quellcode-Dateien ----
set(SOURCES
        main.c
        druck_einkauf.c
        edit_list.c
        edit_data.c
        crud_database.c
)

set(HEADERS
        druck_einkauf.h
        edit_list.h
        edit_data.h
        crud_database.h
)

# ---- Ausführbare Datei erstellen ----
add_executable(einkaufsprojekt ${SOURCES} ${HEADERS})

# ---- (Optional) Ausgabepfad für Executable festlegen ----
# Alles landet in /build/bin/
set_target_properties(einkaufsprojekt PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ---- (Optional) zusätzliche Definitionen für Windows/Linux ----
if (WIN32)
    message(STATUS "Zielplattform: Windows")
else()
    message(STATUS "Zielplattform: Linux / macOS")
endif()


