CONFIG -= qt
SOURCES = libxslt.cpp
mac {
    QMAKE_CXXFLAGS += -iwithsysroot /usr/include/libxslt -iwithsysroot /usr/include/libxml2
    LIBS += -lxslt
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += libxslt
}
