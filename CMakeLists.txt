cmake_minimum_required(VERSION 3.10)

project(CyberiadaPro VERSION 1.0.0 LANGUAGES CXX)

# the release names: the package, install folder and translation domain, and
# the vendor and application names that place the settings and the user data
set(CYBERIADA_PACKAGE "cyberiada-pro")
set(CYBERIADA_VENDOR "Cyberiada")
set(CYBERIADA_APP_NAME "CyberiadaPro")
# the pre-release name whose settings are adopted once (EDIT-UI-5)
set(CYBERIADA_LEGACY_APP_NAME "Cyberiada State Machine Editor")

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

# the sources are UTF-8: MSVC reads a file without a BOM in the code page of the
# system, which garbles QStringLiteral("…") and the u8 literals of the tests
if(MSVC)
  add_compile_options(/utf-8)
endif()

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

# the interface translations (docs/TRANSLATION.md): English is the source
# language, each code below has translations/<domain>_<code>.ts
find_package(Qt5LinguistTools REQUIRED)
set(EDITOR_TRANSLATIONS ru)
set(EDITOR_TS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(EDITOR_I18N_DIR ${CMAKE_CURRENT_BINARY_DIR}/i18n)
set(EDITOR_TS_FILES)
set(PROPERTYBROWSER_TS_FILES)
foreach(_lang ${EDITOR_TRANSLATIONS})
  list(APPEND EDITOR_TS_FILES ${EDITOR_TS_DIR}/${CYBERIADA_PACKAGE}_${_lang}.ts)
  list(APPEND PROPERTYBROWSER_TS_FILES ${EDITOR_TS_DIR}/qtpropertybrowser_${_lang}.ts)
endforeach()
set_source_files_properties(${EDITOR_TS_FILES} ${PROPERTYBROWSER_TS_FILES}
  PROPERTIES OUTPUT_LOCATION ${EDITOR_I18N_DIR})
qt5_add_translation(EDITOR_QM_FILES ${EDITOR_TS_FILES} ${PROPERTYBROWSER_TS_FILES})

# Qt's own strings (the standard buttons and dialogs) travel in the resources:
# the Windows and macOS bundles carry no Qt translations
set(QT_TRANSLATIONS_DIR "" CACHE PATH "the translations of Qt (default: asked from qmake)")
set(_qt_tr_dir ${QT_TRANSLATIONS_DIR})
if(NOT _qt_tr_dir)
  get_target_property(_qmake Qt5::qmake IMPORTED_LOCATION)
  execute_process(COMMAND ${_qmake} -query QT_INSTALL_TRANSLATIONS
    OUTPUT_VARIABLE _qt_tr_dir OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
foreach(_lang ${EDITOR_TRANSLATIONS})
  if(EXISTS "${_qt_tr_dir}/qtbase_${_lang}.qm")
    configure_file(${_qt_tr_dir}/qtbase_${_lang}.qm ${EDITOR_I18N_DIR}/qtbase_${_lang}.qm COPYONLY)
    list(APPEND EDITOR_QM_FILES ${EDITOR_I18N_DIR}/qtbase_${_lang}.qm)
  else()
    message(STATUS "qtbase_${_lang}.qm is not in '${_qt_tr_dir}': the Qt dialogs stay English")
  endif()
endforeach()

# the compiled translations become the :/i18n resources of the core library
set(_qrc "<RCC>\n  <qresource prefix=\"/i18n\">\n")
foreach(_qm ${EDITOR_QM_FILES})
  get_filename_component(_name ${_qm} NAME)
  string(APPEND _qrc "    <file alias=\"${_name}\">i18n/${_name}</file>\n")
endforeach()
string(APPEND _qrc "  </qresource>\n</RCC>\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc.new "${_qrc}")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc.new ${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc COPYONLY)
qt5_add_resources(EDITOR_I18N_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc)

# refresh the .ts files from the sources (run by hand, never by the build);
# the property browser labels need its checkout: -DQTPROPERTYBROWSER_SOURCE_DIR=...
set(QTPROPERTYBROWSER_SOURCE_DIR "" CACHE PATH "the QtPropertyBrowser checkout for translations-update")
file(GLOB _tr_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h ${CMAKE_CURRENT_SOURCE_DIR}/*.ui
  ${CMAKE_CURRENT_SOURCE_DIR}/dialogs/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/dialogs/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dialogs/*.ui)
string(REPLACE ";" "\n" _tr_list "${_tr_sources}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/translations-sources.lst "${_tr_list}\n")
set(_tr_commands COMMAND Qt5::lupdate -locations relative
  @${CMAKE_CURRENT_BINARY_DIR}/translations-sources.lst -ts ${EDITOR_TS_FILES})
if(QTPROPERTYBROWSER_SOURCE_DIR)
  list(APPEND _tr_commands COMMAND Qt5::lupdate -locations relative
    ${QTPROPERTYBROWSER_SOURCE_DIR}/src -ts ${PROPERTYBROWSER_TS_FILES})
endif()
add_custom_target(translations-update ${_tr_commands} VERBATIM
  COMMENT "Updating the translation sources")

# the core library: everything but the entry point, linkable from the
# in-process tests (see docs/TESTING.md, L4)
add_library(CyberiadaEditorCore STATIC
  smeditor_window.ui
  # the resources live in the core so the in-process tests get the bundled
  # font and the icons too; FontManager pulls them in with Q_INIT_RESOURCE
  smeditor.qrc
  # the compiled translations (:/i18n), initialised by interface_language.cpp
  ${EDITOR_I18N_SOURCES}
  myassert.cpp
  cyberiadasm_model.cpp
    cyberiadasm_undo.cpp
  cyberiadasm_view.cpp
  smeditor_window.cpp
  cyberiadasm_editor_document.cpp
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
  edge_geometry.h edge_geometry.cpp
  subject_fragment.h subject_fragment.cpp
  cyberiadasm_editor_subject_item.h cyberiadasm_editor_subject_item.cpp
  cyberiadasm_editor_sm_item.h cyberiadasm_editor_sm_item.cpp
  cyberiadasm_editor_choice_item.h cyberiadasm_editor_choice_item.cpp
  cyberiadasm_editor_comment_item.h cyberiadasm_editor_comment_item.cpp
  fontmanager.h fontmanager.cpp
  code_highlighter.h code_highlighter.cpp
  meta_choice_menu.h meta_choice_menu.cpp
  dialogs/stateactiondialog.h dialogs/stateactiondialog.cpp
  dialogs/multilinetextdialog.h dialogs/multilinetextdialog.cpp
  dialogs/fragmentdialog.h dialogs/fragmentdialog.cpp
  dialogs/about_dialog.h dialogs/about_dialog.cpp
  dialogs/export_image_dialog.h dialogs/export_image_dialog.cpp
  dialogs/file_dialog.h dialogs/file_dialog.cpp
  dialogs/open_file_dialog.h dialogs/open_file_dialog.cpp dialogs/open_file_dialog.ui
  dialogs/save_file_dialog.h dialogs/save_file_dialog.cpp dialogs/save_file_dialog.ui
  dialogs/preferences_dialog.h dialogs/preferences_dialog.cpp dialogs/preferences_dialog.ui
  settings_manager.h settings_manager.cpp
  interface_language.h interface_language.cpp
  gesture_log.h gesture_log.cpp
  plugin_descriptor.h plugin_descriptor.cpp
  plugin_values.h plugin_values.cpp
  plugin_registry.h plugin_registry.cpp
  plugin_runner.h plugin_runner.cpp
  plugin_channel.h plugin_channel.cpp
  plugin_session.h plugin_session.cpp
  batch_session.h batch_session.cpp
  plugins_menu.h plugins_menu.cpp
  dialogs/plugin_parameter_dialog.h dialogs/plugin_parameter_dialog.cpp
  dialogs/plugin_manager_dialog.h dialogs/plugin_manager_dialog.cpp
  plugin_dock.h plugin_dock.cpp
  plugin_session_view.h plugin_session_view.cpp
)

# the forms are compiled into the core, so its generated headers must be reachable
# by everything that links it; a multi-config generator (Visual Studio) writes them
# into include_<config>
get_property(CORE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(CORE_MULTI_CONFIG)
  set(CORE_FORMS_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/CyberiadaEditorCore_autogen/include_$<CONFIG>)
else()
  set(CORE_FORMS_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/CyberiadaEditorCore_autogen/include)
endif()
target_include_directories(CyberiadaEditorCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  # the generated version.h lives in the build dir
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CORE_FORMS_INCLUDE}
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

add_executable(CyberiadaPro
  main.cpp
)
target_link_libraries(CyberiadaPro CyberiadaEditorCore)

if(WIN32)
  # build as a GUI-subsystem app so no console window opens behind the editor;
  # MinGW passes -mwindows while main() still runs via mainCRTStartup, and batch
  # mode still returns its exit code (the tests launch it with start /wait)
  set_target_properties(CyberiadaPro PROPERTIES WIN32_EXECUTABLE ON)
  if(TARGET Qt5::WinMain)
    target_link_libraries(CyberiadaPro Qt5::WinMain)
  endif()
endif()

if(APPLE)
  # build a .app bundle so macdeployqt can turn it into a self-contained .dmg
  set_target_properties(CyberiadaPro PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
    MACOSX_BUNDLE_BUNDLE_NAME "CyberiadaPro"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.cyberiada.pro"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "cyberiada.icns")
  # include the icon in Contents/Resources when it has been generated
  # (packaging/macos/make-icns.sh, run by build-macos.sh before configuring)
  set(_cyb_icns ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/cyberiada.icns)
  if(EXISTS ${_cyb_icns})
    set_source_files_properties(${_cyb_icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(CyberiadaPro PRIVATE ${_cyb_icns})
  endif()
endif()

# installation and Debian packaging (mirrors the Cyberiada libraries)
include(GNUInstallDirs)
install(TARGETS CyberiadaPro
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
if(UNIX AND NOT APPLE)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/${CYBERIADA_PACKAGE}.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT runtime)
  # the MIME type of the desktop entry (shared-mime-info's trigger registers it)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/${CYBERIADA_PACKAGE}-mime.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages
    RENAME ${CYBERIADA_PACKAGE}.xml COMPONENT runtime)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/images/cyberiada.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
    RENAME ${CYBERIADA_PACKAGE}.png COMPONENT runtime)
  # user-facing docs travel with the package (the editor has no ChangeLog yet)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${CYBERIADA_PACKAGE} COMPONENT runtime)

  # the machine-readable (DEP-5) copyright, where the Debian policy wants it
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/debian-copyright
      DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${CYBERIADA_PACKAGE} RENAME copyright COMPONENT runtime)
  # QtPropertyBrowser has no distribution package: bundle it privately and point
  # the installed binary at it through an rpath, so the .deb needs no extra package
  install(FILES ${QTPROPERTYBROWSER_LIBRARY}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CYBERIADA_PACKAGE} COMPONENT runtime)
  set_target_properties(CyberiadaPro PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/${CYBERIADA_PACKAGE}")
endif()

set(CPACK_PACKAGE_NAME "${CYBERIADA_PACKAGE}")
# optional per-release suffix (e.g. ubuntu2404) for the package version, so the
# distro-dependent editor .deb is distinguishable across Ubuntu releases
set(DISTRO_TAG "" CACHE STRING "distribution suffix appended to the package version")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/CyberiadaProPackOptions.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/CyberiadaProPackOptions.cmake" @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CyberiadaProPackOptions.cmake")
include(CPack)

enable_testing()
add_subdirectory(tests)
