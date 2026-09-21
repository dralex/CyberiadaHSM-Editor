# The L0 smoke test suite: open every diagram in batch mode offscreen and
# check the exit code (see docs/TESTING.md)

find_package(Qt5 COMPONENTS Test REQUIRED)

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

# the good text dumps depend on the glyph metrics, and the fontconfig
# substitution differs between machines: expose the bundled fonts only
set(TEST_FONT_DIR ${PROJECT_SOURCE_DIR}/fonts)
set(TEST_FONT_CACHE ${CMAKE_CURRENT_BINARY_DIR}/fontcache)
configure_file(fonts.conf.in fonts.conf @ONLY)

set(L0_ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen"
  "FONTCONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/fonts.conf"
  "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
  "LD_LIBRARY_PATH=${L0_LIBRARY_PATH}")
if(EXISTS "${QT5_LIB_DIR}/qt5/plugins")
  list(APPEND L0_ENVIRONMENT "QT_PLUGIN_PATH=${QT5_LIB_DIR}/qt5/plugins")
endif()

# ctest re-reads this file on every run: the log then names the commit it tested
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrintRevision.cmake)

function(add_l0_test name expected_code)
  add_test(NAME l0-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
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
    comment-transition
    cyb-geometry
    empty-actions
    geometry
    hierarchy
    label-geometry
    lift
    meta
    multiline-actions
    polyline
    sources
    two-sms)
  add_l0_test(${diagram} 0)
endforeach()

# broken documents must fail with the load-error code, not hang or crash
foreach(diagram
    broken-xml
    broken-format
    broken-point-comment)
  add_l0_test(${diagram} 2)
endforeach()

# the strict standard checks reject what the default mode accepts
function(add_l0_strict_test name expected_code)
  add_test(NAME l0-strict-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/diagrams/${name}.graphml
      -DEXPECTED=${expected_code}
      -DSTRICT=1
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l0-strict-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

# the metainformation graph of meta.graphml misses the first dStateMachine key
add_l0_strict_test(hierarchy 0)
add_l0_strict_test(meta 2)

# the reconstruction mode repairs malformed geometry instead
add_test(NAME l0-point-comment-reconstruct
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
    -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/diagrams/broken-point-comment.graphml
    -DEXPECTED=0
    -DRECONSTRUCT=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(l0-point-comment-reconstruct PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "${L0_ENVIRONMENT}")

# a missing file argument is a usage error
add_test(NAME l0-usage
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
    -DEXPECTED=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(l0-usage PROPERTIES TIMEOUT 60 ENVIRONMENT "${L0_ENVIRONMENT}")

# The L1 dump test suite: the canonical document/scene dump of every valid
# diagram must match the reviewed good file (see docs/TESTING.md); the input
# path is relative so the dumped file name stays machine-independent
function(add_l1_test name)
  add_test(NAME l1-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
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
    comment-transition
    cyb-geometry
    empty-actions
    geometry
    hierarchy
    label-geometry
    lift
    meta
    multiline-actions
    polyline
    sources
    two-sms)
  add_l1_test(${diagram})
endforeach()

# The text metrics test suite: the font and the layout of every text item
# must match the reviewed good file. The glyphs are never compared: their
# rasterization follows the freetype build, while the metrics with the pinned
# font, the pinned dpi and the hinting turned off follow the font file only
# (see docs/TESTING.md)
function(add_text_dump_test name)
  add_test(NAME text-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${name}.graphml
      -DTEXT=1
      -DDUMP_TEXT=1
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/text-${name}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-text-output.txt
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(text-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_text_dump_test(comment-transition)
add_text_dump_test(empty-actions)
add_text_dump_test(geometry)
add_text_dump_test(hierarchy)
add_text_dump_test(internal-transition)
add_text_dump_test(label-geometry)
add_text_dump_test(multiline-actions)

# The stack test suite: the undo stack section after an optional script
# (the count, the index and the clean state) vs the good file (see docs/TESTING.md)
function(add_stack_dump_test name diagram)
  set(_script)
  if(ARGC GREATER 2)
    set(_script -DSCRIPT=scripts/${ARGV2}.script)
  endif()
  add_test(NAME stack-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      ${_script}
      -DDUMP_STACK=1
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/stack-${name}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-stack-output.txt
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(stack-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_stack_dump_test(hierarchy hierarchy)
add_stack_dump_test(add-elements hierarchy add-elements)

# The polygon is an optional exploratory framework with its own runner
# (tests/polygon/run-polygon.sh, python -m polygon). It needs Python 3.11+
# (tomllib) and a configured backend, so it is NOT part of the default test
# run; enable its ctest tier with -DPOLYGON_TESTS=ON (see docs/POLYGON.md).
option(POLYGON_TESTS "register the polygon unit tests and regression cases in ctest" OFF)
if(POLYGON_TESTS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME polygon-unit
      COMMAND ${Python3_EXECUTABLE} -m unittest discover -s tests -t .
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/polygon)
    set_tests_properties(polygon-unit PROPERTIES
      TIMEOUT 300
      ENVIRONMENT "${L0_ENVIRONMENT};POLYGON_INSPECTOR=$<TARGET_FILE:CyberiadaEditor>")

    # The polygon regression tier: one case per open problem of the register,
    # green while the recorded failure reproduces (see docs/POLYGON.md)
    function(add_polygon_case id)
      add_test(NAME polygon-${id}
        COMMAND ${Python3_EXECUTABLE} -m polygon check --problem ${id}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/polygon)
      set_tests_properties(polygon-${id} PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "${L0_ENVIRONMENT};POLYGON_INSPECTOR=$<TARGET_FILE:CyberiadaEditor>")
    endfunction()
    include(${CMAKE_CURRENT_SOURCE_DIR}/polygon/problems/cases.cmake OPTIONAL)
  endif()
endif()

# The L2 editing test suite: run the edit script, then compare both the dump
# and the saved document with the good files, and re-open the saved document
# (see docs/TESTING.md)
function(add_l2_test case diagram)
  add_test(NAME l2-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
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
add_l2_test(actions empty-actions)
add_l2_test(transition-action lift)
add_l2_test(comments hierarchy)
add_l2_test(vertices choice)
add_l2_test(history empty)
add_l2_test(submachine empty)
add_l2_test(update-meta meta)
add_l2_test(update-id geometry)
add_l2_test(subjects hierarchy)
add_l2_test(transition-subjects comment-transition)
add_l2_test(gestures geometry)
add_l2_test(transition-notation geometry)
add_l2_test(edge-points polyline)
add_l2_test(copy-paste empty)
add_l2_test(set-color empty)

# The L2 text cases run with the text shown: the canvas text verbs need the
# text items on the canvas (see docs/TESTING.md)
function(add_l2_text_test case diagram)
  add_test(NAME l2-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DSCRIPT=scripts/${case}.script
      -DTEXT=1
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

add_l2_text_test(text-edit geometry)

# The undo test suite: a script edits and undoes (or redoes) through the
# batch verbs, and the dump must equal an EXISTING good file - the untouched
# diagram's L1 dump after the undo, the edited L2 dump after the redo - so the
# cases own no reference file (see docs/TESTING.md)
function(add_undo_test case diagram good)
  set(_save)
  if(ARGC GREATER 3)
    set(_save
      -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/undo-${case}.graphml
      -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${ARGV3}-output.graphml)
  endif()
  add_test(NAME undo-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DSCRIPT=scripts/${case}.script
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/undo-${case}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${good}-output.txt
      ${_save}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(undo-${case} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_undo_test(undo-all hierarchy hierarchy)
add_undo_test(redo-all hierarchy add-elements add-elements)
add_undo_test(gestures-undo geometry geometry)

# The reconstruction test suite: the document is loaded with the library's
# geometry reconstruction, then the scene dump and the saved document are
# compared with the good files; the -sm cases create the state machine border
# as well (see docs/TESTING.md)
function(add_reconstruct_test name)
  set(_sm)
  set(_suffix reconstruct)
  if(ARGC GREATER 1 AND ARGV1 STREQUAL "SM")
    set(_sm -DRECONSTRUCT_SM=1)
    set(_suffix reconstruct-sm)
  endif()
  add_test(NAME ${_suffix}-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${name}.graphml
      -DRECONSTRUCT=1 ${_sm}
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${_suffix}-${name}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-${_suffix}-output.txt
      -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/${_suffix}-${name}.graphml
      -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-${_suffix}-output.graphml
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(${_suffix}-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_reconstruct_test(hierarchy)
add_reconstruct_test(hierarchy SM)
add_reconstruct_test(broken-point-comment)
add_reconstruct_test(geometry)

# The inspection test suite: the read-only mode draws the document geometry -
# the region rectangles and the coordinate origins - so the render differs
# from the editing one; an edit script must be refused (see docs/TESTING.md)
function(add_inspect_render_test name)
  add_test(NAME inspect-render-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${name}.graphml
      -DINSPECT=1
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DIMAGE_OUT=${CMAKE_CURRENT_BINARY_DIR}/inspect-render-${name}.png
      -DIMAGE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${name}-inspect-render.png
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(inspect-render-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_inspect_render_test(geometry)
add_inspect_render_test(cyb-geometry)

# a refused mutation is a script error
function(add_inspect_reject_test case diagram)
  add_test(NAME inspect-reject-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DSCRIPT=scripts/${case}.script
      -DINSPECT=1
      -DEXPECTED=4
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(inspect-reject-${case} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_inspect_reject_test(add-elements hierarchy)
add_inspect_reject_test(delete geometry)
add_inspect_reject_test(rename-move geometry)
add_inspect_reject_test(update-meta meta)
add_inspect_reject_test(vertices choice)

# The save format suite: the document is written in the chosen format and must
# re-open cleanly (see docs/TESTING.md)
function(add_save_format_test diagram format)
  add_test(NAME save-${format}-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DSAVE_FORMAT=${format}
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/save-${format}-${diagram}.graphml
      -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${diagram}-${format}-output.graphml
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(save-${format}-${diagram} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_save_format_test(geometry yed-ostranna)
add_save_format_test(geometry yed-berloga)

# the yEd formats keep a single state machine only
function(add_save_reject_test diagram format)
  add_test(NAME save-reject-${format}-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DSAVE_FORMAT=${format}
      -DEXPECTED=3
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/save-reject-${format}-${diagram}.graphml
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(save-reject-${format}-${diagram} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_save_reject_test(two-sms yed-ostranna)

# The L3 render test suite: the exported scene image must match the good
# image within the comparison tolerance (see docs/TESTING.md)
function(add_l3_test diagram)
  add_test(NAME l3-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
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

# The vector export suite: the svg is compared byte by byte, as the saved
# documents are; the pdf embeds a creation date, so its shape is checked
# (see docs/TESTING.md)
function(add_l3_svg_test diagram)
  add_test(NAME l3-svg-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DIMAGE_OUT=${CMAKE_CURRENT_BINARY_DIR}/l3-${diagram}.svg
      -DSVG_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/${diagram}-render.svg
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l3-svg-${diagram} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

function(add_l3_pdf_test diagram)
  add_test(NAME l3-pdf-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaEditor>
      -DINPUT=diagrams/${diagram}.graphml
      -DEXPECTED=0
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DIMAGE_OUT=${CMAKE_CURRENT_BINARY_DIR}/l3-${diagram}.pdf
      -DPDF_MAGIC=1
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(l3-pdf-${diagram} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_l3_svg_test(geometry)
add_l3_svg_test(choice)
add_l3_pdf_test(geometry)

add_l3_test(empty-actions)
add_l3_test(cyb-geometry)
add_l3_test(geometry)
add_l3_test(sources)
add_l3_test(choice)
add_l3_test(polyline)

# The L4 in-process test suite: QtTest binaries linked against the core
# library, checking the model contract and the scene structure directly
# (see docs/TESTING.md)
function(add_l4_test name)
  add_executable(l4-${name} l4/l4-${name}.cpp)
  target_link_libraries(l4-${name} CyberiadaEditorCore Qt5::Test)
  add_test(NAME l4-${name} COMMAND l4-${name})
  set_tests_properties(l4-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

add_l4_test(model)
add_l4_test(scene)
add_l4_test(dialog)
add_l4_test(text)
add_l4_test(properties)
add_l4_test(window)
add_l4_test(log)
