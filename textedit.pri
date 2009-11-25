INCLUDEPATH += $$PWD

DEFINES += FATAL_ASSUMES
# Input
SOURCES += $$PWD/textedit.cpp $$PWD/textdocument.cpp $$PWD/syntaxhighlighter.cpp $$PWD/textcursor.cpp $$PWD/textlayout_p.cpp $$PWD/textsection.cpp
HEADERS += $$PWD/textedit.h $$PWD/textdocument.h $$PWD/textdocument_p.h $$PWD/syntaxhighlighter.h $$PWD/textcursor.h $$PWD/textlayout_p.h $$PWD/textedit_p.h $$PWD/textcursor_p.h $$PWD/textsection.h $$PWD/weakpointer.h
unix {
    MOC_DIR=.moc
    UI_DIR=.ui
    OBJECTS_DIR=.obj
} else {
    MOC_DIR=tmp/moc
    UI_DIR=tmp/ui
    OBJECTS_DIR=tmp/obj
}
