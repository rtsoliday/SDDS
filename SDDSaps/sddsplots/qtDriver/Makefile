#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
# National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
# Operator of Los Alamos National Laboratory.
# This file is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution.
#*************************************************************************

TOP=../../../../..

include $(TOP)/configure/CONFIG

OS := $(shell uname -s)
ARCH := $(shell uname -m)
ifeq ($(findstring CYGWIN, $(OS)),CYGWIN)
    OS := Windows
endif

CMPLR=ANSI

STATIC_BUILD=NO

PROD_HOST = mpl_qt

mpl_qt_SRCS = mpl_qt.cc mpl_qt_utils.cc mpl_qt_zoom.cc mpl_qt_print_save.cc mpl_qt_readdata.cc

ifeq ($(OS), Windows)
mpl_qt_LIBS = mdbplt gd png SDDS1 rpnlib mdbmth mdblib gsl gslcblas z lzma
PROD_SYS_LIBS = Advapi32
USR_CXXFLAGS = /std:c++17 /Zc:__cplusplus /permissive- -I../../../../include -I"C:\Qt\6.8.2\msvc2022_64\include" -I"C:\Qt\6.8.2\msvc2022_64\include\QtCore" -I"C:\Qt\6.8.2\msvc2022_64\include\QtGui" -I"C:\Qt\6.8.2\msvc2022_64\include\QtWidgets"
USR_LDFLAGS =  /LIBPATH:"C:\Qt\6.8.2\msvc2022_64\lib" Qt6PrintSupport.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
MOC = moc.exe
else
mpl_qt_LIBS = mdbplt SDDS1 rpnlib mdbmth mdblib
PROD_SYS_LIBS_DEFAULT = gd png gsl gslcblas z lzma
USR_CXXFLAGS = -I../../../../include $(shell pkg-config --cflags Qt5Widgets) -fPIC
USR_LDFLAGS = $(shell pkg-config --libs Qt5Widgets Qt5PrintSupport)
MOC = $(wildcard /opt/local/libexec/qt5/bin/moc)
ifeq ($(MOC),)
  MOC = moc-qt5
endif
endif

include $(TOP)/configure/RULES

mpl_qt_moc.h:
	$(MOC) ../mpl_qt.cc -o mpl_qt_moc.h

../mpl_qt.cc: mpl_qt_moc.h

