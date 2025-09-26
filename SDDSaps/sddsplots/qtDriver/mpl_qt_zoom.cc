#include "mpl_qt.h"
#include <cstring>
#include <string>

void newzoom() {
  char *original = NULL;
  char *op = NULL;
  char *tmp = NULL;
  char *_tmp1 = NULL;
  char *_tmp2 = NULL;
  long is_global;
  int useScales = 0;
  int limitAdded = 0;
  char *clcopy = NULL;
  std::string prefix;
  std::string postLimit;
  std::string limitString;

  if (sddsplotCommandline2) {
    clcopy = (char*)malloc(sizeof(char) * (strlen(sddsplotCommandline2) + 1));
    strcpy(clcopy, sddsplotCommandline2);
    char *first = strtok(clcopy, " ");
    if (first && strstr(first, "sddscontour"))
      useScales = 1;
    free(clcopy);
  }

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

    auto appendToken = [&](const char *token) {
      std::string entry;
      if (token[0] != '"') {
        entry += "'";
        entry += token;
        entry += "' ";
      } else {
        entry += " ";
        entry += token;
        entry += " ";
      }
      if (limitAdded)
        postLimit += entry;
      else
        prefix += entry;
    };

    auto updateLimitString = [&]() {
      char buffer[512];
      if (useScales)
        snprintf(buffer, sizeof(buffer),
                 "-scales=%.10g,%.10g,%.10g,%.10g ",
                 xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                 xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                 ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                 ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);
      else
        snprintf(buffer, sizeof(buffer),
                 "-limit=xMin=%.10g,xMax=%.10g,yMin=%.10g,yMax=%.10g,autoscaling ",
                 xlog ? pow(10, xminLimit / xmult - xoff) : xminLimit / xmult - xoff,
                 xlog ? pow(10, xmaxLimit / xmult - xoff) : xmaxLimit / xmult - xoff,
                 ylog ? pow(10, yminLimit / ymult - yoff) : yminLimit / ymult - yoff,
                 ylog ? pow(10, ymaxLimit / ymult - yoff) : ymaxLimit / ymult - yoff);
      limitString = buffer;
    };

    auto ensureLimitString = [&]() {
      if (!limitAdded)
        limitAdded = 1;
      updateLimitString();
    };

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

          ensureLimitString();
        }
      }

      if ((strncmp(tmp, "-col", 4) == 0) || (strncmp(tmp, "-par", 4) == 0)) {
        ensureLimitString();
        if (is_global)
          is_global = 0;
        xmult = xmult_global;
        ymult = ymult_global;
        yoff = yoff_global;
        xoff = xoff_global;
        xlog = xlog_global;
        ylog = ylog_global;
        updateLimitString();
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
        ensureLimitString();
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
        ensureLimitString();
      }

      appendToken(op);
      free(tmp);
      free(op);
    }

    free(original);
    if (!limitAdded)
      ensureLimitString();
  } else {
    prefix = sddsplotCommandline2;
    userx1 = XMAX;
    usery1 = YMAX;
  }

  QTemporaryFile tempFile;
  if (tempFile.open()) {}

  std::string finalCommand;
  if (!(userx0 == 0 && userx1 == 0 && usery0 == 0 && usery1 == 0)) {
    finalCommand = prefix;
    if (limitAdded)
      finalCommand += limitString;
    finalCommand += postLimit;
  } else {
    finalCommand = prefix;
  }
  finalCommand += " -output=";
  finalCommand += tempFile.fileName().toUtf8().constData();
  int sysret = system(finalCommand.c_str());
  (void)sysret;
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
