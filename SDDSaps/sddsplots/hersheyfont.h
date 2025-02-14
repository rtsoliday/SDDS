struct hershey_line {
  unsigned short ncoords;
  short *x;
  short *y;
};

struct hershey_character {
  unsigned short width;
  unsigned short nlines;
  struct hershey_line *line;
};

struct hershey_font_definition {
  struct hershey_character character[256];
};

struct hershey_font_definition * hershey_font_load(const char *fontname);

void hershey_font_free(struct hershey_font_definition *hfd);

void hershey_font_list();
