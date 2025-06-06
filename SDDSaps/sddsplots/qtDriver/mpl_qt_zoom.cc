#include "mpl_qt.h"

void newzoom() {
  char *original = NULL;
  char *op = NULL;
  char *tmp = NULL;
  char *cmd = NULL;
  char *_tmp1 = NULL;
  char *_tmp2 = NULL;
  long is_global;
  int len0 = strlen(sddsplotCommandline2) + 1024;
 
  cmd = (char*)malloc(sizeof(char) * len0);
  strcpy(cmd, "");

  if (!(userx0 == 0 && userx1 == 0 && usery0 == 0 && usery1 == 0)) {
    double xminLimit, xmaxLimit, yminLimit, ymaxLimit, xmult, ymult, xoff, yoff;
    double xmult_global, ymult_global, xoff_global, yoff_global;
    int xlog, ylog;
    int xlog_global, ylog_global;
    xminLimit = MTRACKX(userx0);
    yminLimit = MTRACKY(usery0);
    xmaxLimit = MTRACKX(userx1);
    ymaxLimit = MTRACKY(usery1);
    xmult = 1.0;
    ymult = 1.0;
    xoff = 0.0;
    yoff = 0.0;
    xlog = 0;
    ylog = 0;
    xmult_global = 1.0;
    ymult_global = 1.0;
    xoff_global = 0.0;
    yoff_global = 0.0;
    xlog_global = 0;
    ylog_global = 0;

    original = (char*)malloc(sizeof(char) * (int)(strlen(sddsplotCommandline2) + 256));
    strcpy(original, sddsplotCommandline2);
    is_global = 1;
    while ((op = get_token_t(original, (char*)"'"))) {
      tmp = (char*)malloc(sizeof(char) * (int)(strlen(op) + 256));
      str_tolower(strcpy(tmp, op));
      if (is_blank(op)) {
        free(tmp);
        free(op);
        continue;
      }
      op = trim_spaces(op);
      if ((strncmp(tmp, "-lim", 4) == 0) ||
          (strncmp(tmp, "-sc", 3) == 0) ||
          (strncmp(tmp, "-zo", 3) == 0)) {
        free(tmp);
        free(op);
        continue;
      }

      if (strncmp(tmp, "-mo", 3) == 0) {
        if (strstr(tmp, "=n") || strstr(tmp, "=o") || strstr(tmp, "=e") || strstr(tmp, "=m") || strstr(tmp, "=co") || strstr(tmp, "=ce") || strstr(tmp, "=f")) {
          fprintf(stderr, "New Zoom feature can not handle option -mode with normalize, offset, eoffset,center, meanCenter,coffset,coffset, and fractionalDeviation keywords\n");
          free(tmp);
          free(op);
          free(cmd);
          free(original);
          return;
        }
        if (strstr(tmp, "=lo")) {
          if (strstr(tmp, "y=lo")) {
            ylog = 1;
            if (is_global)
              ylog_global = 1;
          }
          if (strstr(tmp, "x=lo")) {
            xlog = 1;
            if (is_global)
              xlog_global = 1;
          }

          snprintf(tmp, strlen(op) + 256,
                   "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                   xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                   xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                   ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                   ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);
          cmd = (char*)realloc(cmd, sizeof(char) * (len0 += 128));
          strcat(cmd, tmp);
        }
      }

      if ((strncmp(tmp, "-col", 4) == 0) || (strncmp(tmp, "-par", 4) == 0)) {
        snprintf(tmp, strlen(op) + 256,
                 "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                 xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                 xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                 ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                 ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

        cmd = (char*)realloc(cmd, sizeof(char) * (len0 += 128));
        strcat(cmd, tmp);
        if (is_global)
          is_global = 0;
        xmult = xmult_global;
        ymult = ymult_global;
        yoff = yoff_global;
        xoff = xoff_global;
        xlog = xlog_global;
        ylog = ylog_global;
      }

      if (strncmp(tmp, "-fa", 3) == 0) {
        if ((_tmp1 = strstr(tmp, "ym"))) {
          _tmp2 = strstr(_tmp1, "=");
          ymult = atof(_tmp2 + 1);
          if (is_global)
            ymult_global = atof(_tmp2 + 1);
        }
        if ((_tmp1 = strstr(tmp, "xm"))) {
          _tmp2 = strstr(_tmp1, "=");
          xmult = atof(_tmp2 + 1);
          if (is_global)
            xmult_global = atof(_tmp2 + 1);
        }
        snprintf(tmp, strlen(op) + 256,
                 "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                 xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                 xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                 ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                 ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

        cmd = (char*)realloc(cmd, sizeof(char) * (len0 += 128));
        strcat(cmd, tmp);
      }

      if (strncmp(tmp, "-of", 3) == 0) {
        if ((_tmp1 = strstr(tmp, "yc"))) {
          _tmp2 = strstr(_tmp1, "=");
          yoff = atof(_tmp2 + 1);
          if (is_global)
            yoff_global = atof(_tmp2 + 1);
        }
        if ((_tmp1 = strstr(tmp, "xc"))) {
          _tmp2 = strstr(_tmp1, "=");
          xoff = atof(_tmp2 + 1);
          if (is_global)
            xoff_global = atof(_tmp2 + 1);
        }
        snprintf(tmp, strlen(op) + 256,
                 "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                 xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                 xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                 ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                 ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);

        cmd = (char*)realloc(cmd, sizeof(char) * (len0 += 128));
        strcat(cmd, tmp);
      }

      if (op[0] != '"') {
        strcat(cmd, "'");
        strcat(cmd, op);
        strcat(cmd, "' ");
      } else {
        strcat(cmd, " ");
        strcat(cmd, op);
        strcat(cmd, " ");
      }
      free(tmp);
      free(op);
    }

    free(original);
  } else { /* restore to original command */
    strcpy(cmd, sddsplotCommandline2);
    userx1 = XMAX;
    usery1 = YMAX;
  }

  QTemporaryFile tempFile;
  if (tempFile.open()) {}
  
  strcat(cmd, " -output=");
  strcat(cmd, tempFile.fileName().toUtf8().constData());
  int sysret = system(cmd);
  (void)sysret;
  free(cmd);
  do {
    if (fexists(tempFile.fileName().toUtf8().constData()))
      break;
  } while (1);

  if (tempFile.size()) {
    ifp = fopen(tempFile.fileName().toUtf8().constData(), "rb");
    currentPlot = destroyallplotrec();
    readdata();
    fclose(ifp);
  }
}
