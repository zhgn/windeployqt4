# Qt core $$PWD module

HEADERS +=  \
        $$PWD/qstandardpaths.h \
    $$PWD/qcommandlineoption.h \
    $$PWD/qcommandlineparser.h

SOURCES += \
        $$PWD/qstandardpaths.cpp \
    $$PWD/qcommandlineoption.cpp \
    $$PWD/qcommandlineparser.cpp

win32 {
    !winrt {
        SOURCES += \
            $$PWD/qstandardpaths_win.cpp

        LIBS += -lmpr
    } else {
        SOURCES += \
                $$PWD/qstandardpaths_winrt.cpp
    }
}
