# CMake
cmake_minimum_required(VERSION 3.14) 
project(Privilege-Auction-OPD VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Decomment for quick DLL assembly on Windows.
# set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # for .lib / .a
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)  # for .so / .dylib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)  # for .dll (Windows)

include(FetchContent)
include(GNUInstallDirs)
include(CTest)

# <CPM_DEPENDENCIES_START>
# </CPM_DEPENDENCIES_END>

set(SOURCES
    src/engine.cpp
    # src/algorithm.cpp
    # src/auction.cpp
    # src/pricing.cpp
)

# STATIC or SHARED
add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Добавляем демо для cls тестов
add_executable(pa_demo example/main.cpp)

# Линкуем нашу библиотеку с демо-приложением!
# PRIVATE означает, что pa_demo использует либу, но не передает её дальше
target_link_libraries(pa_demo PRIVATE ${PROJECT_NAME})

if(BUILD_TESTING)
    add_test(NAME pa_demo COMMAND pa_demo)
endif()

install(TARGETS ${PROJECT_NAME} pa_demo
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# <CPM_LINK_START>
# </CPM_LINK_START>
