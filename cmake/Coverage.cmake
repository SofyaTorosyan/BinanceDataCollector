option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    find_program(LCOV_EXECUTABLE lcov REQUIRED)
    find_program(GENHTML_EXECUTABLE genhtml REQUIRED)

    add_compile_options(--coverage)
    add_link_options(--coverage)

    message(STATUS "Coverage instrumentation enabled (lcov: ${LCOV_EXECUTABLE})")
endif()

function(add_coverage_target)
    if(NOT ENABLE_COVERAGE)
        return()
    endif()

    set(COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage-report")
    set(COVERAGE_RAW  "${COVERAGE_DIR}/coverage.info")
    set(COVERAGE_FILTERED "${COVERAGE_DIR}/filtered.info")

    add_custom_target(coverage
        COMMENT "Running tests and generating lcov HTML coverage report..."

        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}

        # ── 1. Zero out counters from any previous run ──────────────────────
        COMMAND ${LCOV_EXECUTABLE}
            --zerocounters
            --directory ${CMAKE_BINARY_DIR}

        # ── 2. Run unit tests ────────────────────────────────────────────────
        COMMAND $<TARGET_FILE:BinanceTests>

        # ── 3. Run integration tests (require live Binance connection) ───────
        COMMAND $<TARGET_FILE:BinanceIntegrationTests>

        # ── 4. Capture coverage data ─────────────────────────────────────────
        COMMAND ${LCOV_EXECUTABLE}
            --capture
            --directory ${CMAKE_BINARY_DIR}
            --output-file ${COVERAGE_RAW}
            --rc branch_coverage=1
            --ignore-errors mismatch

        # ── 5. Filter out noise (system headers, Conan cache, test sources) ──
        COMMAND ${LCOV_EXECUTABLE}
            --remove ${COVERAGE_RAW}
            "/usr/*"
            "$ENV{HOME}/.conan2/*"
            "${CMAKE_SOURCE_DIR}/tests/*"
            --output-file ${COVERAGE_FILTERED}
            --rc branch_coverage=1

        # ── 6. Generate HTML report ──────────────────────────────────────────
        COMMAND ${GENHTML_EXECUTABLE}
            ${COVERAGE_FILTERED}
            --output-directory ${COVERAGE_DIR}/html
            --branch-coverage
            --title "BinanceDataCollector Coverage"

        # ── 7. Print terminal summary ────────────────────────────────────────
        COMMAND ${LCOV_EXECUTABLE}
            --summary ${COVERAGE_FILTERED}
            --rc branch_coverage=1

        DEPENDS BinanceTests BinanceIntegrationTests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        VERBATIM
    )

    message(STATUS "Coverage target registered. After configuring:")
    message(STATUS "  cmake --build <build_dir> --target coverage")
    message(STATUS "  Report: ${COVERAGE_DIR}/html/index.html")
endfunction()
