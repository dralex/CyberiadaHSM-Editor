cmake_minimum_required(VERSION 3.10)

project(CyberiadaInspector VERSION 1.0.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH /usr/lib/cmake)

set(cyberiadamlpp_DIR /usr/lib/cmake)
find_package(cyberiadamlpp REQUIRED)

set(cyberiadaml_DIR /usr/lib/cmake)
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

add_executable(CyberiadaInspector
  smeditor_window.ui
  myassert.cpp
  cyberiadasm_model.cpp
  cyberiadasm_view.cpp
  smeditor_window.cpp
  cyberiadasm_properties_widget.cpp
  main.cpp
  smeditor.qrc
)

target_include_directories(CyberiadaInspector PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${QTPROPERTYBROWSER_INCLUDE_DIR}
  ${cyberiadaml_INCLUDE_DIRS}
  ${cyberiadamlpp_INCLUDE_DIRS}
  )
target_link_directories(CyberiadaInspector PUBLIC
  ${cyberiadaml_LIBRARY}
  ${cyberiadamlpp_LIBRARY}
  )
target_link_libraries(CyberiadaInspector
  Qt5::Widgets
  ${QTPROPERTYBROWSER_LIBRARY}
  ${cyberiadaml_LIBRARIES}
  ${cyberiadamlpp_LIBRARIES}
  )
