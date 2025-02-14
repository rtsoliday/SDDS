include ../Makefile.rules

LIBRARY = mdblib

LIBRARY_SRC = array.c \
    backspace.c \
    binary.c \
    binsert.c \
    bomb.c \
    buffer.c \
    clean_filename.c \
    compress.c \
    cp_str.c \
    data_scan.c \
    delete_bnd.c \
    delete_chars.c \
    edit_string.c \
    fexists.c \
    filestat.c \
    fill_array.c \
    fopen_e.c \
    get_token_tq.c \
    headers.c \
    insert.c \
    interpret_escapes.c \
    is_blank.c \
    match_string.c \
    mkdir.c \
    non_dominated_sort.c \
    pad_with_spaces.c \
    query.c \
    rcdelete.c \
    replace_chars.c \
    replace_string.c \
    replacefile.c \
    report_stats.c \
    scanitemlist.c \
    searchPath.c \
    sortfunctions.c \
    str_copy.c \
    str_in.c \
    str_inn.c \
    str_tolower.c \
    str_toupper.c \
    strslide.c \
    substituteTagValue.c \
    time.c \
    timeconvert.c \
    timer.c \
    tmpname.c \
    trim_spaces.c \
    unpack.c \
    wild_match.c

CFLAGS += -I../include

ifeq ($(OS), Windows)
  CFLAGS += -DEXPORT_MDBLIB /wd4244 /wd4267
endif


include ../Makefile.build


