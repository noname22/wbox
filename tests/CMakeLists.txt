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

target_link_libraries(cpu_tests PRIVATE mooreader wbox_cpu)

add_test(NAME cpu_tests COMMAND cpu_tests)
