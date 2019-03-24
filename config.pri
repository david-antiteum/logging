QTDIR = $$[QT_INSTALL_PREFIX]
APP_LFLAGS	= -rpath @executable_path -rpath @executable_path/libs

QMAKE_LFLAGS += $${APP_LFLAGS}

CPPREST_INCL = $$QTDIR/extras/cpprestsdk/include
CPPREST_LIBPATH = -L$$QTDIR/extras/cpprestsdk/lib
CPPREST_LIB = -lcpprest
CPPREST_LIBS = $$CPPREST_LIBPATH $$CPPREST_LIB

BOOST_INCL = $$QTDIR/extras/boost/include
BOOST_LIBS = -L$$QTDIR/extras/boost/lib
BOOST_LIBS += -l$${BOOST_EXTRASTATICLIBPREFIX}boost_system$${BOOSTLIBSUFFIX}
BOOST_LIBS += -l$${BOOST_EXTRASTATICLIBPREFIX}boost_thread$${BOOSTLIBSUFFIX}
BOOST_LIBS += -l$${BOOST_EXTRASTATICLIBPREFIX}boost_chrono$${BOOSTLIBSUFFIX}

SSL_INCL = $$QTDIR/extras/openssl/include
SSL_LIBS = -L$$QTDIR/extras/openssl/lib -l$${EXTRASTATICLIBPREFIX}crypto -l$${EXTRASTATICLIBPREFIX}ssl

JAEGER_INCL += $$QTDIR/extras/yaml-cpp/include
JAEGER_INCL += $$QTDIR/extras/jaeger/include
JAEGER_INCL += $$QTDIR/extras/opentracing/include
JAEGER_INCL += $$QTDIR/extras/thrift/include

JAEGER_LIBS += -L$$QTDIR/extras/yaml-cpp/lib -lyaml-cpp
JAEGER_LIBS += -L$$QTDIR/extras/jaeger/lib -ljaegertracing
JAEGER_LIBS += -L$$QTDIR/extras/opentracing/lib -lopentracing
JAEGER_LIBS += -L$$QTDIR/extras/thrift/lib -lthrift

CONFIG *= qt c++11 c++14 c++17

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
