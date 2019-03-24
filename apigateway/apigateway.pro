include( ../config.pri )

TEMPLATE = app
TARGET = ../bin/apigateway

CONFIG += console
CONFIG -= app_bundle

# Input
SOURCES += main.cpp

# ----

INCLUDEPATH += . ../include $$CPPREST_INCL $$BOOST_INCL $$SSL_INCL $$JAEGER_INCL
LIBS += $$CPPREST_LIBS $$BOOST_LIBS $$SSL_LIBS $$JAEGER_LIBS
