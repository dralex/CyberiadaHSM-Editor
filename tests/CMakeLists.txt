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
  # no plugin of the machine reaches a case (EDIT-PLUG-1)
  "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data"
  # the same on every system: the preferences in an ini file, the user plugins in
  # the build dir (Windows keeps them in the registry and %APPDATA% otherwise)
  "CYBERIADA_PRO_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/config"
  "CYBERIADA_PRO_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data"
  "CYBERIADA_PRO_PLUGIN_PATH="
  "LD_LIBRARY_PATH=${L0_LIBRARY_PATH}")
if(EXISTS "${QT5_LIB_DIR}/qt5/plugins")
  list(APPEND L0_ENVIRONMENT "QT_PLUGIN_PATH=${QT5_LIB_DIR}/qt5/plugins")
endif()
if(WIN32)
  # the offscreen platform of Windows reads its fonts from a folder, not fontconfig:
  # the bundled ones, as fonts.conf gives them on Linux
  list(APPEND L0_ENVIRONMENT "QT_QPA_FONTDIR=${TEST_FONT_DIR}")
endif()

# ctest re-reads this file on every run: the log then names the commit it tested
set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrintRevision.cmake)

function(add_l0_test name expected_code)
  add_test(NAME l0-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
    components
    cyb-geometry
    empty-actions
    geometry
    hierarchy
    label-geometry
    lift
    meta
    multiline-actions
    overlap
    polyline
    resize-hold
    sources
    submachine-points
    two-sm-bordered
    two-sms)
  add_l0_test(${diagram} 0)
endforeach()

# broken documents must fail with the load-error code, not hang or crash
foreach(diagram
    broken-xml
    broken-format)
  add_l0_test(${diagram} 2)
endforeach()

# the default mode reads the point of a comment as a loose rect (the Lapki dialect)
add_l0_test(broken-point-comment 0)

# the strict standard checks reject what the default mode accepts
function(add_l0_strict_test name expected_code)
  add_test(NAME l0-strict-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
# the final PNST 1044 text: an unnamed entry point (8.3.1), defer on a transition (6.8.1)
add_l0_strict_test(unnamed-point 2)
add_l0_strict_test(defer-on-transition 2)
add_l0_strict_test(broken-point-comment 2)

# the reconstruction mode repairs malformed geometry instead
add_test(NAME l0-point-comment-reconstruct
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
    -DEXPECTED=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(l0-usage PROPERTIES TIMEOUT 60 ENVIRONMENT "${L0_ENVIRONMENT}")

# The L1 dump test suite: the canonical document/scene dump of every valid
# diagram must match the reviewed good file (see docs/TESTING.md); the input
# path is relative so the dumped file name stays machine-independent
function(add_l1_test name)
  add_test(NAME l1-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
    components
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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

# the tabs tier: a script with the document verbs; the dump carries the tab
# line and the active document, or the script is refused with the exit code
function(add_tabs_test name diagram expected_code)
  set(_good)
  if(expected_code EQUAL 0)
    set(_good -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/tabs-${name}.out
              -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/tabs-${name}-output.txt)
  endif()
  add_test(NAME tabs-${name}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
      -DINPUT=diagrams/${diagram}.graphml
      -DSCRIPT=scripts/tabs-${name}.script
      -DDUMP_TABS=1
      -DEXPECTED=${expected_code}
      -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
      ${_good}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  set_tests_properties(tabs-${name} PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "${L0_ENVIRONMENT}")
endfunction()

add_tabs_test(open hierarchy 0)
add_tabs_test(switch hierarchy 0)
add_tabs_test(reopen hierarchy 0)
add_tabs_test(new-document hierarchy 0)
add_tabs_test(copy-paste hierarchy 0)

# the batch mode is English whatever the interface language and the locale
# (EDIT-UI-2): a profile with Russian chosen and a Russian locale change nothing
set(UI_CONFIG_HOME ${CMAKE_CURRENT_BINARY_DIR}/config-ui)
file(WRITE "${UI_CONFIG_HOME}/${CYBERIADA_VENDOR}/${CYBERIADA_APP_NAME}.ini"
  "[interface]\nlanguage=ru\n")
set(UI_ENVIRONMENT ${L0_ENVIRONMENT})
list(FILTER UI_ENVIRONMENT EXCLUDE REGEX "^(XDG_CONFIG_HOME|CYBERIADA_PRO_CONFIG_HOME)=")
list(APPEND UI_ENVIRONMENT "XDG_CONFIG_HOME=${UI_CONFIG_HOME}"
  "CYBERIADA_PRO_CONFIG_HOME=${UI_CONFIG_HOME}" "LANG=ru_RU.UTF-8" "LANGUAGE=ru")
add_test(NAME ui-batch-english
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
    -DINPUT=diagrams/hierarchy.graphml
    -DSCRIPT=scripts/ui-batch-english.script
    -DDUMP_TABS=1
    -DEXPECTED=0
    -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/ui-batch-english.out
    -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/ui-batch-english-output.txt
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(ui-batch-english PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "${UI_ENVIRONMENT}")
add_tabs_test(undo hierarchy 0)
add_tabs_test(close-discard hierarchy 0)
add_tabs_test(close-refused hierarchy 4)

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
      ENVIRONMENT "${L0_ENVIRONMENT};POLYGON_INSPECTOR=$<TARGET_FILE:CyberiadaPro>")

    # The polygon regression tier: one case per open problem of the register,
    # green while the recorded failure reproduces (see docs/POLYGON.md)
    function(add_polygon_case id)
      add_test(NAME polygon-${id}
        COMMAND ${Python3_EXECUTABLE} -m polygon check --problem ${id}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/polygon)
      set_tests_properties(polygon-${id} PROPERTIES
        TIMEOUT 120
        ENVIRONMENT "${L0_ENVIRONMENT};POLYGON_INSPECTOR=$<TARGET_FILE:CyberiadaPro>")
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
add_l2_test(nested-comment-link geometry)
add_l2_test(vertices choice)
add_l2_test(history empty)
add_l2_test(submachine empty)
add_l2_test(update-meta meta)
add_l2_test(update-id geometry)
add_l2_test(subjects hierarchy)
add_l2_test(transition-subjects comment-transition)
add_l2_test(subject-draw comment-transition)
add_l2_test(subject-verbs comment-transition)
add_l2_test(subject-reconstruct comment-transition)
add_l2_test(subject-delete comment-transition)
add_l2_test(subject-handles comment-transition)
add_l2_test(gestures geometry)
add_l2_test(transition-notation geometry)
add_l2_test(edge-points polyline)
add_l2_test(copy-paste empty)
add_l2_test(set-color empty)
add_l2_test(reconstruct-two-sms two-sms)
add_l2_test(update-geometry partial-geometry)
add_l2_test(snap-transition polyline)
add_l2_test(snap-alt polyline)

# a batch run starts with snap off whatever the settings hold (EDIT-TOOL-14):
# under a profile with snap on, the snap-transition gestures give the snap-alt
# result, so the case owns no reference file. The settings file is named after
# CYBERIADA_VENDOR/CYBERIADA_APP_NAME (the root CMakeLists.txt)
set(SNAP_CONFIG_HOME ${CMAKE_CURRENT_BINARY_DIR}/config-snap)
file(WRITE "${SNAP_CONFIG_HOME}/${CYBERIADA_VENDOR}/${CYBERIADA_APP_NAME}.ini"
  "[display]\nsnapMode=true\ngridSpacing=20\n")
set(SNAP_ENVIRONMENT ${L0_ENVIRONMENT})
list(FILTER SNAP_ENVIRONMENT EXCLUDE REGEX "^(XDG_CONFIG_HOME|CYBERIADA_PRO_CONFIG_HOME)=")
list(APPEND SNAP_ENVIRONMENT "XDG_CONFIG_HOME=${SNAP_CONFIG_HOME}"
  "CYBERIADA_PRO_CONFIG_HOME=${SNAP_CONFIG_HOME}")
add_test(NAME l2-snap-default
  COMMAND ${CMAKE_COMMAND}
    -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
    -DINPUT=diagrams/polyline.graphml
    -DSCRIPT=scripts/snap-default.script
    -DEXPECTED=0
    -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/l2-snap-default.out
    -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/snap-alt-output.txt
    -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/l2-snap-default.graphml
    -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/snap-alt-output.graphml
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
set_tests_properties(l2-snap-default PROPERTIES
  TIMEOUT 60
  ENVIRONMENT "${SNAP_ENVIRONMENT}")

# The L2 text cases run with the text shown: the canvas text verbs need the
# text items on the canvas (see docs/TESTING.md)
function(add_l2_text_test case diagram)
  add_test(NAME l2-${case}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
add_l2_text_test(subject-fragment comment-transition)
add_l2_text_test(comment-selection comment-transition)

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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
# the Lapki IDE dialect for lapki-client and lapki-compiler
add_save_format_test(meta lapki)

# the yEd formats keep a single state machine only
function(add_save_reject_test diagram format)
  add_test(NAME save-reject-${format}-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
# the Lapki format needs the platform name
add_save_reject_test(geometry lapki)

# The L3 render test suite: the exported scene image must match the good
# image within the comparison tolerance (see docs/TESTING.md)
function(add_l3_test diagram)
  add_test(NAME l3-${diagram}
    COMMAND ${CMAKE_COMMAND}
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
      -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro>
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
add_l3_test(comment-transition)

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
add_l4_test(links)
add_l4_test(dialog)
add_l4_test(text)
add_l4_test(properties)
add_l4_test(highlight)
add_l4_test(window)
add_l4_test(log)
add_l4_test(tabs)
add_l4_test(language)
add_l4_test(translation)

# The plugin layer: the batch plugin stage over the example plugins of
# plugins/<set>/, which all run one helper with no dependencies (see
# docs/TESTING.md, Plugins)
set(PLUGIN_TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/plugins)
add_executable(plugin-helper plugin-helper/plugin_helper.cpp)
set_target_properties(plugin-helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_TEST_DIR}/bin)
foreach(_config ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${_config} _config)
  set_target_properties(plugin-helper PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${_config} ${PLUGIN_TEST_DIR}/bin)
endforeach()
# the descriptors call ${plugin_dir}/../../bin/plugin-helper: the sets sit next to it
file(GLOB_RECURSE _plugin_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/plugins
  ${CMAKE_CURRENT_SOURCE_DIR}/plugins/*)
foreach(_file ${_plugin_files})
  configure_file(plugins/${_file} ${PLUGIN_TEST_DIR}/${_file} COPYONLY)
endforeach()
# a settings profile naming the interpreter of the interp plugin (EDIT-PLUG-4)
set(PLUGIN_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/config-plugins)
file(WRITE "${PLUGIN_CONFIG_DIR}/${CYBERIADA_VENDOR}/${CYBERIADA_APP_NAME}.ini"
  "[plugin-interpreters]\ncyb-test-helper=${PLUGIN_TEST_DIR}/bin/plugin-helper${CMAKE_EXECUTABLE_SUFFIX}\n")
# a user plugin folder for the default search path (EDIT-PLUG-1)
set(PLUGIN_USER_DATA ${CMAKE_CURRENT_BINARY_DIR}/data-user)
configure_file(plugins/main/echo/plugin.json
  "${PLUGIN_USER_DATA}/${CYBERIADA_VENDOR}/${CYBERIADA_APP_NAME}/plugins/echo/plugin.json" COPYONLY)

# add_plugin_test(<case> [INPUT f] [PLUGIN id[:action]] [PARAMS p...] [PATHS set...]
#   [SCRIPT f] [EXPECTED n] [FORBID text] [SEED src>dst] [CONFIG dir] [DATA dir]
#   [ENV var=value...] [DUMP] [DUMP_PLUGIN] [DUMP_STACK] [DUMP_TABS] [LIST] [NO_DUMP]
#   [SAVE] [INSPECT] [CLEAN] [NOPATH])
# compares good/plugin-<case>-output.txt when a section is dumped and
# good/plugin-<case>-output.graphml with SAVE; the work dir is plugin-work/<case>
function(add_plugin_test name)
  cmake_parse_arguments(P "DUMP;DUMP_PLUGIN;DUMP_STACK;DUMP_TABS;LIST;NO_DUMP;SAVE;INSPECT;CLEAN;NOPATH"
    "INPUT;PLUGIN;SCRIPT;EXPECTED;FORBID;SEED;CONFIG;DATA" "PARAMS;PATHS;ENV" ${ARGN})
  if(NOT DEFINED P_EXPECTED)
    set(P_EXPECTED 0)
  endif()
  if(NOT P_PATHS)
    set(P_PATHS main)
  endif()
  set(_work ${CMAKE_CURRENT_BINARY_DIR}/plugin-work/${name})
  set(_vars -DBATCH_BIN=$<TARGET_FILE:CyberiadaPro> -DEXPECTED=${P_EXPECTED}
    -DWORKDIR=${CMAKE_CURRENT_SOURCE_DIR})
  if(NOT P_NOPATH)
    set(_paths)
    foreach(_set ${P_PATHS})
      list(APPEND _paths ${PLUGIN_TEST_DIR}/${_set})
    endforeach()
    string(REPLACE ";" "|" _paths "${_paths}")
    list(APPEND _vars "-DPLUGIN_PATH=${_paths}")
  endif()
  if(P_PARAMS)
    string(REPLACE ";" "|" _params "${P_PARAMS}")
    list(APPEND _vars "-DPLUGIN_PARAMS=${_params}")
  endif()
  string(REPLACE "<work>" "${_work}" P_INPUT "${P_INPUT}")
  string(REPLACE "<work>" "${_work}" _vars "${_vars}")
  foreach(_option INPUT PLUGIN SCRIPT FORBID)
    if(P_${_option})
      list(APPEND _vars "-D${_option}=${P_${_option}}")
    endif()
  endforeach()
  foreach(_flag DUMP_PLUGIN DUMP_STACK DUMP_TABS INSPECT NO_DUMP)
    if(P_${_flag})
      list(APPEND _vars -D${_flag}=1)
    endif()
  endforeach()
  if(P_LIST)
    list(APPEND _vars -DLIST_PLUGINS=1)
  endif()
  if(P_CLEAN OR P_SEED)
    list(APPEND _vars -DCLEAN_DIR=${_work})
  endif()
  if(P_SEED)
    string(REPLACE ">" ";" _seed "${P_SEED}")
    list(GET _seed 0 _src)
    list(GET _seed 1 _dst)
    if(NOT IS_ABSOLUTE "${_src}")
      set(_src ${CMAKE_CURRENT_SOURCE_DIR}/${_src})
    endif()
    list(APPEND _vars "-DSEED=${_src}>${_work}/${_dst}")
  endif()
  if(P_DUMP OR P_DUMP_PLUGIN OR P_DUMP_STACK OR P_DUMP_TABS OR P_LIST OR P_NO_DUMP)
    list(APPEND _vars -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/plugin-${name}.out
      -DDUMP_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/plugin-${name}-output.txt)
  endif()
  if(P_SAVE)
    list(APPEND _vars -DSAVE_OUT=${CMAKE_CURRENT_BINARY_DIR}/plugin-${name}.graphml
      -DSAVE_GOOD=${CMAKE_CURRENT_SOURCE_DIR}/good/plugin-${name}-output.graphml)
  endif()
  add_test(NAME plugin-${name}
    COMMAND ${CMAKE_COMMAND} ${_vars} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunBatchTest.cmake)
  # hermetic: only --plugin-path folders unless the case sets the variable itself
  set(_env)
  foreach(_entry ${L0_ENVIRONMENT})
    if(P_CONFIG AND _entry MATCHES "^(XDG_CONFIG_HOME|CYBERIADA_PRO_CONFIG_HOME)=")
      string(REGEX REPLACE "=.*" "=${P_CONFIG}" _entry "${_entry}")
    endif()
    # the case sets the data home and the plugin path below
    if(NOT _entry MATCHES "^(XDG_DATA_HOME|CYBERIADA_PRO_DATA_HOME|CYBERIADA_PRO_PLUGIN_PATH)=")
      list(APPEND _env "${_entry}")
    endif()
  endforeach()
  if(P_DATA)
    list(APPEND _env "XDG_DATA_HOME=${P_DATA}" "CYBERIADA_PRO_DATA_HOME=${P_DATA}")
  else()
    list(APPEND _env "XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data"
      "CYBERIADA_PRO_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/data")
  endif()
  set(_has_path OFF)
  foreach(_entry ${P_ENV})
    if(_entry MATCHES "^CYBERIADA_PRO_PLUGIN_PATH=")
      set(_has_path ON)
    endif()
  endforeach()
  if(NOT _has_path)
    list(APPEND _env "CYBERIADA_PRO_PLUGIN_PATH=")
  endif()
  list(APPEND _env ${P_ENV})
  set_tests_properties(plugin-${name} PROPERTIES TIMEOUT 60 ENVIRONMENT "${_env}")
endfunction()

# discovery (EDIT-PLUG-1..4)
add_plugin_test(list-main LIST)
add_plugin_test(list-rejected LIST PATHS rejected)
add_plugin_test(list-shadow LIST PATHS shadow main)
add_plugin_test(list-env-user LIST NOPATH DATA ${PLUGIN_USER_DATA}
  ENV "CYBERIADA_PRO_PLUGIN_PATH=${PLUGIN_TEST_DIR}/shadow")
add_plugin_test(list-prefs LIST CONFIG ${PLUGIN_CONFIG_DIR})
# a fresh profile holding only the pre-release store gives the same list (EDIT-UI-5)
set(LEGACY_SETTINGS ${CMAKE_CURRENT_BINARY_DIR}/legacy-settings.ini)
file(WRITE ${LEGACY_SETTINGS}
  "[plugin-interpreters]\ncyb-test-helper=${PLUGIN_TEST_DIR}/bin/plugin-helper${CMAKE_EXECUTABLE_SUFFIX}\n")
add_plugin_test(list-prefs-adopted LIST
  SEED "${LEGACY_SETTINGS}>config/${CYBERIADA_VENDOR}/${CYBERIADA_LEGACY_APP_NAME}.ini"
  CONFIG ${CMAKE_CURRENT_BINARY_DIR}/plugin-work/list-prefs-adopted/config)
# the run, stdout and the variables (EDIT-PLUG-8/9, EDIT-PLUG-27/28)
add_plugin_test(log INPUT diagrams/hierarchy.graphml PLUGIN echo DUMP_PLUGIN)
add_plugin_test(log-empty PLUGIN echo DUMP_PLUGIN)
add_plugin_test(log-stdout INPUT diagrams/hierarchy.graphml PLUGIN echo NO_DUMP)
add_plugin_test(vars INPUT diagrams/geometry.graphml SCRIPT scripts/plugin-select.script
  PLUGIN vars:all DUMP_PLUGIN)
add_plugin_test(vars-params INPUT diagrams/hierarchy.graphml PLUGIN vars:all DUMP_PLUGIN
  PARAMS flag=true count=7 mode=slow name=abc)
add_plugin_test(vars-untitled PLUGIN vars:all DUMP_PLUGIN)
add_plugin_test(meta INPUT diagrams/meta.graphml PLUGIN vars:meta DUMP_PLUGIN)
add_plugin_test(unknown-var INPUT diagrams/hierarchy.graphml PLUGIN vars:unknown-var
  DUMP_PLUGIN EXPECTED 6)
# the language reaches the plugin, en in the batch mode (EDIT-PLUG-33)
add_plugin_test(language INPUT diagrams/hierarchy.graphml PLUGIN vars:language DUMP_PLUGIN)
# the declared metainformation fills an absent key before the run (EDIT-PLUG-34)
add_plugin_test(meta-set INPUT diagrams/hierarchy.graphml PLUGIN vars:fill DUMP_PLUGIN DUMP_STACK SAVE)
add_plugin_test(meta-kept INPUT diagrams/code-highlight.graphml PLUGIN vars:fill DUMP_PLUGIN DUMP_STACK)
add_plugin_test(meta-same INPUT diagrams/markdown-comments.graphml PLUGIN vars:fill DUMP_PLUGIN DUMP_STACK)
add_plugin_test(meta-empty PLUGIN vars:fill DUMP_PLUGIN DUMP_STACK)
add_plugin_test(meta-inspect INPUT diagrams/hierarchy.graphml PLUGIN vars:fill DUMP_PLUGIN INSPECT)
# request.json, the parameter files and the secrets (EDIT-PLUG-9/10)
add_plugin_test(request INPUT diagrams/hierarchy.graphml PLUGIN request DUMP_PLUGIN
  PARAMS prompt=@plugins/data/prompt.md key=env:PLUGIN_TEST_SECRET
  ENV PLUGIN_TEST_SECRET=s3cr3t FORBID s3cr3t)
add_plugin_test(secret-file INPUT diagrams/hierarchy.graphml PLUGIN request DUMP_PLUGIN
  PARAMS prompt=@@literal key=file:${CMAKE_CURRENT_SOURCE_DIR}/plugins/data/key.txt FORBID s3cr3t)
add_plugin_test(secret-missing INPUT diagrams/hierarchy.graphml PLUGIN request DUMP_PLUGIN
  PARAMS prompt=x key=env:PLUGIN_TEST_UNSET EXPECTED 6)
# the usage errors (EDIT-PLUG-27)
add_plugin_test(usage-required INPUT diagrams/hierarchy.graphml PLUGIN request EXPECTED 1)
add_plugin_test(usage-bad-int INPUT diagrams/hierarchy.graphml PLUGIN vars:all PARAMS count=x EXPECTED 1)
add_plugin_test(usage-range INPUT diagrams/hierarchy.graphml PLUGIN vars:all PARAMS count=11 EXPECTED 1)
add_plugin_test(usage-enum INPUT diagrams/hierarchy.graphml PLUGIN vars:all PARAMS mode=medium EXPECTED 1)
add_plugin_test(usage-unknown-param INPUT diagrams/hierarchy.graphml PLUGIN vars:all PARAMS nope=1 EXPECTED 1)
add_plugin_test(usage-unknown-plugin INPUT diagrams/hierarchy.graphml PLUGIN nope EXPECTED 1)
add_plugin_test(usage-unknown-action INPUT diagrams/hierarchy.graphml PLUGIN vars:nope EXPECTED 1)
add_plugin_test(usage-ambiguous INPUT diagrams/hierarchy.graphml PLUGIN vars EXPECTED 1)
add_plugin_test(usage-rejected INPUT diagrams/hierarchy.graphml PLUGIN api2 PATHS rejected EXPECTED 1)
# the results (EDIT-PLUG-12..17)
add_plugin_test(replace INPUT diagrams/hierarchy.graphml PLUGIN edit:rename DUMP SAVE)
add_plugin_test(replace-stack INPUT diagrams/hierarchy.graphml PLUGIN edit:rename DUMP_PLUGIN DUMP_STACK)
add_plugin_test(replace-same INPUT diagrams/hierarchy.graphml PLUGIN edit:same DUMP_PLUGIN DUMP_STACK)
add_plugin_test(replace-inspect INPUT diagrams/hierarchy.graphml PLUGIN edit:rename DUMP_PLUGIN
  INSPECT EXPECTED 6)
add_plugin_test(diagnostics-inspect INPUT diagrams/choice.graphml PLUGIN checker:check DUMP_PLUGIN INSPECT)
add_plugin_test(new-document INPUT diagrams/hierarchy.graphml PLUGIN maker DUMP_PLUGIN DUMP_TABS)
add_plugin_test(new-document-empty PLUGIN maker DUMP_PLUGIN DUMP_TABS)
add_plugin_test(new-document-save INPUT diagrams/hierarchy.graphml PLUGIN maker DUMP_PLUGIN SAVE)
add_plugin_test(diagnostics INPUT diagrams/choice.graphml PLUGIN checker:check DUMP_PLUGIN)
add_plugin_test(diagnostics-bad INPUT diagrams/choice.graphml PLUGIN checker:bad DUMP_PLUGIN EXPECTED 6)
add_plugin_test(files INPUT diagrams/hierarchy.graphml PLUGIN files PARAMS dir=<work>/out DUMP_PLUGIN CLEAN)
add_plugin_test(files-changed INPUT diagrams/hierarchy.graphml PLUGIN files PARAMS dir=<work>
  SEED plugins/data/prompt.md>README DUMP_PLUGIN)
# the failures leave the document untouched (EDIT-PLUG-18..20)
add_plugin_test(exit-code INPUT diagrams/hierarchy.graphml PLUGIN edit:exit3 DUMP_PLUGIN DUMP_STACK EXPECTED 6)
add_plugin_test(exit-ok INPUT diagrams/hierarchy.graphml PLUGIN codes DUMP_PLUGIN)
add_plugin_test(crash INPUT diagrams/hierarchy.graphml PLUGIN edit:crash DUMP_PLUGIN EXPECTED 6)
add_plugin_test(timeout INPUT diagrams/hierarchy.graphml PLUGIN edit:slow DUMP_PLUGIN DUMP_STACK EXPECTED 6)
add_plugin_test(garbage INPUT diagrams/hierarchy.graphml PLUGIN edit:garbage DUMP_PLUGIN DUMP_STACK EXPECTED 6)
add_plugin_test(empty-output INPUT diagrams/hierarchy.graphml PLUGIN edit:empty DUMP_PLUGIN EXPECTED 6)
add_plugin_test(missing-output INPUT diagrams/hierarchy.graphml PLUGIN edit:missing DUMP_PLUGIN EXPECTED 6)
add_plugin_test(load-strict INPUT diagrams/hierarchy.graphml PLUGIN edit:strict DUMP_PLUGIN EXPECTED 6)
add_plugin_test(load-lenient INPUT diagrams/hierarchy.graphml PLUGIN edit:lenient DUMP_PLUGIN)
add_plugin_test(load-reconstruct INPUT diagrams/hierarchy.graphml PLUGIN edit:repair DUMP_PLUGIN SAVE)
add_plugin_test(load-plain INPUT diagrams/hierarchy.graphml PLUGIN edit:no-repair DUMP_PLUGIN EXPECTED 6)
# save and cwd: document (EDIT-PLUG-25), the program lookup (EDIT-PLUG-1/4)
add_plugin_test(save-cwd INPUT <work>/hierarchy.graphml SEED diagrams/hierarchy.graphml>hierarchy.graphml
  SCRIPT scripts/plugin-rename.script PLUGIN docdir DUMP_PLUGIN DUMP_STACK)
add_plugin_test(save-untitled PLUGIN docdir DUMP_PLUGIN EXPECTED 6)
add_plugin_test(relative INPUT diagrams/hierarchy.graphml PLUGIN relative DUMP_PLUGIN)
add_plugin_test(interp-prefs INPUT diagrams/hierarchy.graphml PLUGIN interp DUMP_PLUGIN CONFIG ${PLUGIN_CONFIG_DIR})
add_plugin_test(interp-missing INPUT diagrams/hierarchy.graphml PLUGIN nointerp DUMP_PLUGIN EXPECTED 6)
add_plugin_test(shadow-run INPUT diagrams/hierarchy.graphml PLUGIN echo PATHS shadow main DUMP_PLUGIN)

# the plugin core in process: the descriptors, the values and the runner over the helper
add_l4_test(plugin-core)
target_compile_definitions(l4-plugin-core PRIVATE
  PLUGIN_HELPER="${PLUGIN_TEST_DIR}/bin/plugin-helper${CMAKE_EXECUTABLE_SUFFIX}")
add_dependencies(l4-plugin-core plugin-helper)

# the plugin window: the menu, the toolbars, the dialog, the lock and the dock
add_l4_test(plugins)
target_compile_definitions(l4-plugins PRIVATE
  PLUGIN_TEST_DIR="${PLUGIN_TEST_DIR}"
  PLUGIN_HELPER="${PLUGIN_TEST_DIR}/bin/plugin-helper${CMAKE_EXECUTABLE_SUFFIX}")
add_dependencies(l4-plugins plugin-helper)
