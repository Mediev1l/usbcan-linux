# Coverage helpers. The libraries holding logic (core, backend) and the tests
# calling them are instrumented; only src/core and src/backend are reported and
# the threshold is enforced by the `coverage` target.
#
# -fno-exceptions: the code never throws, but GCC attributes exception cleanup
# blocks to source lines ("=====" in gcov), which would show as uncovered.
# Tests are instrumented too so inline functions from headers are counted.

function(usbcan_instrument target)
    if(USBCAN_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -O0 -fno-inline -fno-exceptions)
        target_link_options(${target} PUBLIC --coverage)
    endif()
endfunction()

if(USBCAN_COVERAGE)
    find_program(GCOVR_EXECUTABLE gcovr HINTS $ENV{HOME}/.local/bin REQUIRED)
    set(USBCAN_COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${USBCAN_COVERAGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${USBCAN_COVERAGE_DIR}
        COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${GCOVR_EXECUTABLE}
            --root ${CMAKE_SOURCE_DIR}
            --object-directory ${CMAKE_BINARY_DIR}
            --filter ${CMAKE_SOURCE_DIR}/src/core/
            --filter ${CMAKE_SOURCE_DIR}/src/backend/
            --exclude-unreachable-branches
            --exclude-throw-branches
            --print-summary
            --txt ${USBCAN_COVERAGE_DIR}/summary.txt
            --html-details ${USBCAN_COVERAGE_DIR}/index.html
            --fail-under-line 100
            --fail-under-function 100
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and checking 100% line/function coverage of src/core and src/backend"
        VERBATIM)
endif()
