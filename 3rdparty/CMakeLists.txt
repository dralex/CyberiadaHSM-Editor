# the vendored libraries (README.md), compiled as they are

# md4c: the Markdown parser of both readers (docs/MARKDOWN.md, Reading)
add_library(md4c STATIC
  md4c/md4c.c md4c/md4c.h
)
target_include_directories(md4c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/md4c)

if(CYBERIADA_WITH_MATH)
  # JKQTMathText with the parts of JKQTCommon it needs and the XITS fonts
  add_library(CyberiadaMathText STATIC
    jkqtmathtext/jkqtcommon/jkqtpcsstools.cpp
    jkqtmathtext/jkqtcommon/jkqtpdrawingtools.cpp
    jkqtmathtext/jkqtcommon/jkqtpenhancedpainter.cpp
    jkqtmathtext/jkqtcommon/jkqtpstringtools.cpp
    jkqtmathtext/jkqtmathtext/jkqtmathtext.cpp
    jkqtmathtext/jkqtmathtext/jkqtmathtexttools.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextboxinstructionnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextbracenode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextdecoratednode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextfracnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtexthorizontallistnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextinstructionnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextmatrixnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextmodifyenvironmentnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextnodetools.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextnoopnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextsqrtnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextsubsupernode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextsymbolnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtexttextnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextverbatimnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextverticallistnode.cpp
    jkqtmathtext/jkqtmathtext/nodes/jkqtmathtextwhitespacenode.cpp
    jkqtmathtext/jkqtmathtext/parsers/jkqtmathtextlatexparser.cpp
    jkqtmathtext/jkqtmathtext/parsers/jkqtmathtextparser.cpp
    # the fonts become the :/JKQTMathText/fonts resources, Q_INIT_RESOURCE(xits)
    jkqtmathtext/jkqtmathtext/resources/xits.qrc
  )
  target_include_directories(CyberiadaMathText PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/jkqtmathtext)
  target_compile_definitions(CyberiadaMathText PRIVATE JKQTMATHTEXT_COMPILED_WITH_XITS)
  target_link_libraries(CyberiadaMathText PUBLIC Qt5::Widgets)
endif()
