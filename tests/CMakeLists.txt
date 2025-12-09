find_package(ZLIB)

add_library(mooreader STATIC
    mooreader.c
    mooreader.h
)

if(ZLIB_FOUND)
    target_compile_definitions(mooreader PRIVATE MOO_USE_ZLIB)
    target_link_libraries(mooreader PRIVATE ZLIB::ZLIB)
endif()

add_executable(cpu_tests
    cpu_test.c
)

target_include_directories(cpu_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Define path to test data directory
target_compile_definitions(cpu_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/ext/80386/v1_ex_real_mode"
)

target_link_libraries(cpu_tests PRIVATE mooreader wbox_cpu)

# Create one test per MOO file for better CTest reporting
set(TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/ext/80386/v1_ex_real_mode")
file(GLOB MOO_TEST_FILES "${TEST_DATA_DIR}/*.MOO.gz")

foreach(MOO_FILE ${MOO_TEST_FILES})
    get_filename_component(FILE_NAME ${MOO_FILE} NAME)
    # Strip .MOO.gz suffix to get test name (e.g., "FF.0" from "FF.0.MOO.gz")
    string(REGEX REPLACE "\\.MOO\\.gz$" "" TEST_NAME "${FILE_NAME}")
    add_test(NAME cpu_${TEST_NAME} COMMAND cpu_tests -f ${MOO_FILE})
endforeach()
