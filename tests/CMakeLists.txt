# The L0 smoke test suite: open every diagram in batch mode offscreen and
# check the exit code (see docs/TESTING.md)

set(L0_ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config")

function(add_l0_test name expected_code)
  add_test(NAME l0-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaInspector>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/diagrams/${name}.graphml
      -DEXPECTED=${expected_code}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l0-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

# valid CyberiadaML-1.0 documents must open cleanly
foreach(diagram
    choice
    cyb-geometry
    empty-actions
    geometry
    hierarchy
    lift
    meta
    sources
    two-sms)
  add_l0_test(${diagram} 0)
endforeach()

# broken documents must fail with the load-error code, not hang or crash
foreach(diagram
    broken-xml
    broken-format)
  add_l0_test(${diagram} 2)
endforeach()

# a missing file argument is a usage error
add_test(NAME l0-usage
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaInspector>
    -DEXPECTED=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(l0-usage PROPERTIES TIMEOUT 60 ENVIRONMENT "${L0_ENVIRONMENT}")
