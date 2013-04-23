APP_NAME = PjsuaBB

CONFIG += qt warn_on cascades10

include(config.pri)

SOURCES +=  ../../pjsua_app.c \
            ../../pjsua_app_cli.c \
            ../../pjsua_app_common.c \
            ../../pjsua_app_config.c \
            ../../pjsua_app_legacy.c

device {
    CONFIG(debug, debug|release) {
        # Device-Debug custom configuration
        include(../../../../pjsip.pri)
        LIBS += -lbb
    }

    CONFIG(release, debug|release) {
        # Device-Release custom configuration
        include(../../../../pjsip.pri)
        LIBS += -lbb
    }
}

simulator {
    CONFIG(debug, debug|release) {
        # Simulator-Debug custom configuration
    }
}
