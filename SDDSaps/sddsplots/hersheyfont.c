//
// hersheyfont.c - Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hershey.font"
#include "hersheyfont.h"


void loadchar(struct hershey_character *hc, char* string) {
  int len, i, j, k, line;
  char buffer[4];
  unsigned int ncoords;
  char R = 'R';
  int xoffset, yoffset;

  len = strlen(string);

  strncpy(buffer, string+5, 3);
  buffer[3]=0;
  ncoords = strtoul(buffer, NULL, 10);

  len = len - 8;

  hc->width = string[9] - string[8];
  xoffset = R - string[8];
  yoffset = 9;

  ncoords--;
  hc->nlines=0;
  string += 10;
  
  for (i=0; i<ncoords; i++) {
    if (i == ncoords-1) { //last coordinate on character
      hc->nlines++;      
    } else if ((string[i*2] == ' ') && (string[i*2+1] == R)) { //last coordinate for line but character continues
      hc->nlines++;
    }
  }
  hc->line = malloc(sizeof(struct hershey_line) * hc->nlines);

  line=0;
  j=0;
  for (i=0; i<ncoords; i++) {
    if (i == ncoords-1) { //last coordinate on character
      hc->line[line].ncoords = i - j + 1;
      hc->line[line].x = malloc(sizeof(short) * hc->line[line].ncoords);
      hc->line[line].y = malloc(sizeof(short) * hc->line[line].ncoords);
      for (k=0; k < hc->line[line].ncoords; k++) {
	hc->line[line].x[k] = xoffset + (string[(j+k)*2] - R);
	hc->line[line].y[k] = yoffset - (string[(j+k)*2+1] - R);
      }
    } else if ((string[i*2] == ' ') && (string[i*2+1] == R)) { //last coordinate for line but character continues
      hc->line[line].ncoords = i - j - 1 + 1;
      hc->line[line].x = malloc(sizeof(short) * hc->line[line].ncoords);
      hc->line[line].y = malloc(sizeof(short) * hc->line[line].ncoords);
      for (k=0; k < hc->line[line].ncoords; k++) {
	hc->line[line].x[k] = xoffset + (string[(j+k)*2] - R);
	hc->line[line].y[k] = yoffset - (string[(j+k)*2+1] - R);
      }
      j = i+1;
      line++;
    }
  }
  return;
}

struct hershey_font_definition * hershey_font_load( const char *fontname ) {
  struct hershey_font_definition *hfd;
  struct hershey_character *hc;
    char linebuf[4096];
    int linecount = 0;
    char *p;
    char *p2;
    if (strcmp(fontname,"astrology") == 0) {
      p = (char*)astrology;
    } else if (strcmp(fontname,"cursive") == 0) {
      p = (char*)cursive;
    } else if (strcmp(fontname,"cyrilc_1") == 0) {
      p = (char*)cyrilc_1;
    } else if (strcmp(fontname,"cyrillic") == 0) {
      p = (char*)cyrillic;
    } else if (strcmp(fontname,"futural") == 0) {
      p = (char*)futural;
    } else if (strcmp(fontname,"futuram") == 0) {
      p = (char*)futuram;
    } else if (strcmp(fontname,"gothgbt") == 0) {
      p = (char*)gothgbt;
    } else if (strcmp(fontname,"gothgrt") == 0) {
      p = (char*)gothgrt;
    } else if (strcmp(fontname,"gothiceng") == 0) {
      p = (char*)gothiceng;
    } else if (strcmp(fontname,"gothicger") == 0) {
      p = (char*)gothicger;
    } else if (strcmp(fontname,"gothicita") == 0) {
      p = (char*)gothicita;
    } else if (strcmp(fontname,"gothitt") == 0) {
      p = (char*)gothitt;
    } else if (strcmp(fontname,"greekc") == 0) {
      p = (char*)greekc;
    } else if (strcmp(fontname,"greek") == 0) {
      p = (char*)greek;
    } else if (strcmp(fontname,"greeks") == 0) {
      p = (char*)greeks;
    } else if (strcmp(fontname,"japanese") == 0) {
      p = (char*)japanese;
    } else if (strcmp(fontname,"markers") == 0) {
      p = (char*)markers;
    } else if (strcmp(fontname,"mathlow") == 0) {
      p = (char*)mathlow;
    } else if (strcmp(fontname,"mathupp") == 0) {
      p = (char*)mathupp;
    } else if (strcmp(fontname,"meteorology") == 0) {
      p = (char*)meteorology;
    } else if (strcmp(fontname,"music") == 0) {
      p = (char*)music;
    } else if (strcmp(fontname,"rowmand") == 0) {
      p = (char*)rowmand;
    } else if (strcmp(fontname,"rowmans") == 0) {
      p = (char*)rowmans;
    } else if (strcmp(fontname,"rowmant") == 0) {
      p = (char*)rowmant;
    } else if (strcmp(fontname,"scriptc") == 0) {
      p = (char*)scriptc;
    } else if (strcmp(fontname,"scripts") == 0) {
      p = (char*)scripts;
    } else if (strcmp(fontname,"symbolic") == 0) {
      p = (char*)symbolic;
    } else if (strcmp(fontname,"timesg") == 0) {
      p = (char*)timesg;
    } else if (strcmp(fontname,"timesib") == 0) {
      p = (char*)timesib;
    } else if (strcmp(fontname,"timesi") == 0) {
      p = (char*)timesi;
    } else if (strcmp(fontname,"timesrb") == 0) {
      p = (char*)timesrb;
    } else if (strcmp(fontname,"timesr") == 0) {
      p = (char*)timesr;
    } else {
      return(NULL);
    }

    hfd = calloc(1, sizeof(struct hershey_font_definition));
    linecount = 0;
    p2 = strchr(p, '\n');
    while (p2 != NULL) {
      strncpy(linebuf, p, p2-p);
      linebuf[p2-p] = 0;
      linecount++;

      hc = &(hfd->character[linecount + 31]);
      loadchar(hc, linebuf);

      p=p2+1;
      p2 = strchr(p, '\n');
    }

    return(hfd);
}

void hershey_font_free(struct hershey_font_definition *hfd) {
  int i, j;
  for (i=0; i<256; i++) {
    for (j=0; j < hfd->character[i].nlines; j++) {
      if (hfd->character[i].line[j].ncoords) {
	free(hfd->character[i].line[j].x);
	free(hfd->character[i].line[j].y);
      }
    }
    if (hfd->character[i].nlines)
      free(hfd->character[i].line);
  }
}



void hershey_font_list() {
  fprintf(stderr, "Available fonts are:\n");
  fprintf(stderr, "  astrology\n");
  fprintf(stderr, "  cursive\n");
  fprintf(stderr, "  cyrilc_1\n");
  fprintf(stderr, "  cyrillic\n");
  fprintf(stderr, "  futural\n");
  fprintf(stderr, "  futuram\n");
  fprintf(stderr, "  gothgbt\n");
  fprintf(stderr, "  gothgrt\n");
  fprintf(stderr, "  gothiceng\n");
  fprintf(stderr, "  gothicger\n");
  fprintf(stderr, "  gothicita\n");
  fprintf(stderr, "  gothitt\n");
  fprintf(stderr, "  greekc\n");
  fprintf(stderr, "  greek\n");
  fprintf(stderr, "  greeks\n");
  fprintf(stderr, "  japanese\n");
  fprintf(stderr, "  markers\n");
  fprintf(stderr, "  mathlow\n");
  fprintf(stderr, "  mathupp\n");
  fprintf(stderr, "  meteorology\n");
  fprintf(stderr, "  music\n");
  fprintf(stderr, "  rowmand\n");
  fprintf(stderr, "  rowmans (default)\n");
  fprintf(stderr, "  rowmant\n");
  fprintf(stderr, "  scriptc\n");
  fprintf(stderr, "  scripts\n");
  fprintf(stderr, "  symbolic\n");
  fprintf(stderr, "  timesg\n");
  fprintf(stderr, "  timesib\n");
  fprintf(stderr, "  timesi\n");
  fprintf(stderr, "  timesrb\n");
  fprintf(stderr, "  timesr\n");
  fprintf(stderr, "You can view examples at: http://www.whence.com/hershey-fonts/\n");
  fprintf(stderr, "- The Hershey Fonts were originally created by Dr.\n\
        A. V. Hershey while working at the U. S.\n\
        National Bureau of Standards.\n\
- The format of the Font data in this distribution\n\
 	was originally created by\n\
 		James Hurt\n\
 		Cognition, Inc.\n\
 		900 Technology Park Drive\n\
 		Billerica, MA 01821\n\
 		(mit-eddie!ci-dandelion!hurt)\n");

}

