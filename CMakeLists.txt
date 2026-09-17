cmake_minimum_required(VERSION 3.10)

project(CyberiadaEditor VERSION 1.0.0 LANGUAGES CXX)

# the system CMake package configs live here on Debian/Ubuntu; on other
# platforms the Cyberiada dependencies are found through CMAKE_PREFIX_PATH
if(UNIX)
  list(APPEND CMAKE_MODULE_PATH /usr/lib/cmake)
endif()

# capture the git revision at configure time for the session-log header
find_package(Git QUIET)
set(CYBERIADA_GIT_REVISION "unknown")
if(Git_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE _rev OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
  if(_rc EQUAL 0)
    set(CYBERIADA_GIT_REVISION "${_rev}")
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} status --porcelain -uno
      OUTPUT_VARIABLE _dirty OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_dirty)
      set(CYBERIADA_GIT_REVISION "${_rev}-dirty")
    endif()
  endif()
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

# the dependencies are found in the system prefix or in CMAKE_PREFIX_PATH
find_package(cyberiadamlpp REQUIRED)
find_package(cyberiadaml REQUIRED)

find_package(QtPropertyBrowser REQUIRED)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)
# the Svg module may live outside the Qt5 tree, so it is looked up on its own;
# the Qt5 components above must be found first, the Svg config depends on them
find_package(Qt5Svg REQUIRED)

# the core library: everything but the entry point, linkable from the
# in-process tests (see docs/TESTING.md, L4)
add_library(CyberiadaEditorCore STATIC
  smeditor_window.ui
  # the resources live in the core so the in-process tests get the bundled
  # font and the icons too; FontManager pulls them in with Q_INIT_RESOURCE
  smeditor.qrc
  myassert.cpp
  cyberiadasm_model.cpp
    cyberiadasm_undo.cpp
  cyberiadasm_view.cpp
  smeditor_window.cpp
  cyberiadasm_properties_widget.cpp
  cyberiadasm_editor_view.cpp
  cyberiadasm_editor_scene.cpp
  cyberiadasm_editor_items.cpp
  batch_driver.h batch_driver.cpp
  batch_script.h batch_script.cpp
  cyberiadasm_dump.h cyberiadasm_dump.cpp
  cyberiadasm_render.h cyberiadasm_render.cpp
  dotsignal.h dotsignal.cpp
  editable_text_item.h editable_text_item.cpp
  cyberiadasm_editor_vertex_item.h cyberiadasm_editor_vertex_item.cpp
  cyberiadasm_editor_state_item.h cyberiadasm_editor_state_item.cpp
  cyberiadasm_editor_transition_item.h cyberiadasm_editor_transition_item.cpp
  cyberiadasm_editor_sm_item.h cyberiadasm_editor_sm_item.cpp
  cyberiadasm_editor_choice_item.h cyberiadasm_editor_choice_item.cpp
  cyberiadasm_editor_comment_item.h cyberiadasm_editor_comment_item.cpp
  fontmanager.h fontmanager.cpp
  dialogs/stateactiondialog.h dialogs/stateactiondialog.cpp
  dialogs/about_dialog.h dialogs/about_dialog.cpp
  dialogs/export_image_dialog.h dialogs/export_image_dialog.cpp
  dialogs/file_dialog.h dialogs/file_dialog.cpp
  dialogs/open_file_dialog.h dialogs/open_file_dialog.cpp dialogs/open_file_dialog.ui
  dialogs/save_file_dialog.h dialogs/save_file_dialog.cpp dialogs/save_file_dialog.ui
  dialogs/preferences_dialog.h dialogs/preferences_dialog.cpp dialogs/preferences_dialog.ui
  settings_manager.h settings_manager.cpp
  gesture_log.h gesture_log.cpp
)

target_include_directories(CyberiadaEditorCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  # the generated version.h lives in the build dir
  ${CMAKE_CURRENT_BINARY_DIR}
  # the forms are compiled into the core, so its generated headers must be
  # reachable by everything that links it
  ${CMAKE_CURRENT_BINARY_DIR}/CyberiadaEditorCore_autogen/include
  ${QTPROPERTYBROWSER_INCLUDE_DIR}
  ${cyberiadaml_INCLUDE_DIRS}
  ${cyberiadamlpp_INCLUDE_DIRS}
  )
target_link_libraries(CyberiadaEditorCore PUBLIC
  Qt5::Widgets
  Qt5::Svg
  ${QTPROPERTYBROWSER_LIBRARY}
  ${cyberiadaml_LIBRARIES}
  ${cyberiadamlpp_LIBRARIES}
  )

add_executable(CyberiadaEditor
  main.cpp
)
target_link_libraries(CyberiadaEditor CyberiadaEditorCore)

# installation and Debian packaging (mirrors the Cyberiada libraries)
include(GNUInstallDirs)
install(TARGETS CyberiadaEditor
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
if(UNIX AND NOT APPLE)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/cyberiada-editor.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT runtime)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/images/cyberiada.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    RENAME cyberiada-editor.png COMPONENT runtime)
  # QtPropertyBrowser has no distribution package: bundle it privately and point
  # the installed binary at it through an rpath, so the .deb needs no extra package
  install(FILES ${QTPROPERTYBROWSER_LIBRARY}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cyberiada-editor COMPONENT runtime)
  set_target_properties(CyberiadaEditor PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/cyberiada-editor")
endif()

set(CPACK_PACKAGE_NAME "cyberiada-editor")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CyberiadaEditorPackOptions.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/CyberiadaEditorPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CyberiadaEditorPackOptions.cmake")
include(CPack)

enable_testing()
add_subdirectory(tests)
