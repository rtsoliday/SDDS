include ../Makefile.rules

LIBRARY = xls

LIBRARY_SRC = assert_assist.cpp \
	blank.cpp \
	boolean.cpp \
	cbridge.cpp \
	cell.cpp \
	colinfo.cpp \
	colors.cpp \
	continue.cpp \
	datast.cpp \
	docsumminfo.cpp \
	err.cpp \
	extformat.cpp \
	font.cpp \
	format.cpp \
	formula.cpp \
	formula_cell.cpp \
	formula_estimate.cpp \
	formula_expr.cpp \
	globalrec.cpp \
	HPSF.cpp \
	index.cpp \
	label.cpp \
	merged.cpp \
	note.cpp \
	number.cpp \
	range.cpp \
	recdef.cpp \
	record.cpp \
	row.cpp \
	sheetrec.cpp \
	summinfo.cpp \
	unit.cpp \
	workbook.cpp \
	binfile.cpp \
	oledoc.cpp \
	olefs.cpp \
	oleprop.cpp

CCFLAGS += -DHAVE_CONFIG_H=1 -DEXPORT_XLS -I../include -I.

ifeq ($(OS), Windows)
  CCFLAGS += /wd4003 /wd4267 /wd4068 /wd4018 -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
endif

include ../Makefile.build

