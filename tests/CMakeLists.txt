# The L0 smoke test suite: open every diagram in batch mode offscreen and
# check the exit code (see docs/TESTING.md)

# bake the Qt runtime location into the test environment: the Qt in use may
# live outside the system paths, and RUNPATH does not cover transitive
# dependencies or the dlopen'ed platform plugin
get_target_property(_qt5_core_lib Qt5::Core LOCATION)
get_filename_component(QT5_LIB_DIR "${_qt5_core_lib}" DIRECTORY)
get_filename_component(_qtpb_lib_dir "${QTPROPERTYBROWSER_LIBRARY}" DIRECTORY)

set(L0_LIBRARY_PATH "${QT5_LIB_DIR}:${_qtpb_lib_dir}")
if(DEFINED ENV{LD_LIBRARY_PATH})
  set(L0_LIBRARY_PATH "${L0_LIBRARY_PATH}:$ENV{LD_LIBRARY_PATH}")
endif()

set(L0_ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen"
  "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
  "LD_LIBRARY_PATH=${L0_LIBRARY_PATH}")
if(EXISTS "${QT5_LIB_DIR}/qt5/plugins")
  list(APPEND L0_ENVIRONMENT "QT_PLUGIN_PATH=${QT5_LIB_DIR}/qt5/plugins")
endif()

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

# The L1 dump test suite: the canonical document/scene dump of every valid
# diagram must match the reviewed good file (see docs/TESTING.md); the input
# path is relative so the dumped file name stays machine-independent
function(add_l1_test name)
  add_test(NAME l1-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaInspector>
      -DINPUT=diagrams/${name}.graphml
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/l1-${name}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-output.txt
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l1-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

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
  add_l1_test(${diagram})
endforeach()

# The L2 editing test suite: run the edit script, then compare both the dump
# and the saved document with the good files, and re-open the saved document
# (see docs/TESTING.md)
function(add_l2_test case diagram)
  add_test(NAME l2-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaInspector>
      -DINPUT=diagrams/${diagram}.graphml
      -DSCRIPT=scripts/${case}.script
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/l2-${case}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${case}-output.txt
      -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/l2-${case}.graphml
      -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${case}-output.graphml
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l2-${case} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_l2_test(add-elements hierarchy)
add_l2_test(add-transition geometry)
add_l2_test(rename-move geometry)
add_l2_test(reparent hierarchy)
add_l2_test(delete geometry)

# The L3 render test suite: the exported scene image must match the good
# image within the comparison tolerance (see docs/TESTING.md)
function(add_l3_test diagram)
  add_test(NAME l3-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaInspector>
      -DINPUT=diagrams/${diagram}.graphml
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DIMAGE_OUT=${CMAKE_CURRENT_BINARY_DIR}/l3-${diagram}.png
      -DIMAGE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${diagram}-render.png
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l3-${diagram} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_l3_test(choice)
add_l3_test(cyb-geometry)
add_l3_test(geometry)
add_l3_test(sources)
