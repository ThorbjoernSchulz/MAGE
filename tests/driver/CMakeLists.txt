
add_library(TestDriver STATIC test_driver.c testing.h)
target_include_directories(TestDriver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(LINKER_FLAGS -Wl,-T ${CMAKE_CURRENT_SOURCE_DIR}/LinkerScript.ld)
target_link_libraries(TestDriver ${LINKER_FLAGS} Interna)
