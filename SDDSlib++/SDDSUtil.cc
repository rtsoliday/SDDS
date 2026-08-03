#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "SDDS3.h"

/*
 *  isBigEndianMachine returns true on big endian operating systems like Solaris
 *  and false on little endian operating systems like linux and windows.
 *
 */
int32_t SDDS_isBigEndianMachine() {
  int32_t x = 1;
  if ( *((char*)&x) )
    return false;
  return true;
}

int32_t SDDS_identifyType(char* typeName) {
  int32_t i;
  const char *SDDS_type_name_old[SDDS_NUM_TYPES] = {
    "double", "float", "long", "ulong", "short", "ushort", "string", "character"
  };
  const char *SDDS_type_name[SDDS_NUM_TYPES] = {
    "real64", "real32", "int32", "uint32", "int16", "uint16", "string", "character"
  };
  for (i=0; i<SDDS_NUM_TYPES; i++) {
    if (strcasecmp(typeName, SDDS_type_name[i]) == 0)
      return i+1;
    if (strcasecmp(typeName, SDDS_type_name_old[i]) == 0)
      return i+1;
  }
  return 0;
}

int32_t SDDS_verifyPrintfFormat(char *string, int32_t type) {
  char *percent, *s;
  int32_t len, tmp;

  s = string;
  do {
    if ((percent = strchr(s, '%'))) {
      if (*(percent+1)!='%')
        break;
      s = percent+1;
    }
  } while (percent);
  if (!percent || !*++percent)
    return(0);

  string = percent;
  switch (type) {
  case SDDS_DOUBLE:
  case SDDS_FLOAT:
    if ((len = strcspn(string, "fegEG"))==strlen(string))
      return(0);
    if (len==0)
      return(1);
    if ((tmp=strspn(string, "-+.0123456789 "))<len)
      return(0);
    break;
  case SDDS_LONG:
    if ((len = strcspn(string, "d"))==strlen(string))
      return(0);
    if (*(string+len-1)!='l')
      return(0);
    if (--len==0)
      return(1);
    if ((tmp=strspn(string, "-+.0123456789 "))<len)
      return(0);
    break;
  case SDDS_ULONG:
    if ((len = strcspn(string, "u"))==strlen(string))
      return(0);
    if (*(string+len-1)!='l')
      return(0);
    if (--len==0)
      return(1);
    if ((tmp=strspn(string, "-+.0123456789 "))<len)
      return(0);
    break;
  case SDDS_SHORT:
    if ((len = strcspn(string, "d"))==strlen(string))
      return(0);
    if (*(string+len-1)!='h')
      return(0);
    if (--len==0)
      return(1);
    if ((tmp=strspn(string, "-+.0123456789 "))<len)
      return(0);
    break;
  case SDDS_USHORT:
    if ((len = strcspn(string, "u"))==strlen(string))
      return(0);
    if (*(string+len-1)!='h')
      return(0);
    if (--len==0)
      return(1);
    if ((tmp=strspn(string, "-+.0123456789 "))<len)
      return(0);
    break;
  case SDDS_STRING:
    if ((len = strcspn(string, "s"))==strlen(string))
      return(0);
    if (len==0)
      return(1);
    if ((tmp=strspn(string, "-0123456789"))<len)
      return(0);
    break;
  case SDDS_CHARACTER:
    if ((len = strcspn(string, "c"))==strlen(string))
      return(0);
    if (len!=0)
      return(0);
    break;
  default:
    return(0);
  }
  /* no errors found--its probably okay */
  return(1);
}

int32_t SDDS_isValidType(int32_t type) {
  if ((type >= 1) && (type <= SDDS_NUM_TYPES)) {
    return 1;
  }
  return 0;
}

static uint32_t nameValidityFlags = 0;
void SDDS_setNameValidityFlags(uint32_t flags) {
  nameValidityFlags = flags;
}

int32_t SDDS_isValidName(char *name) {
  char *ptr;
  int32_t isValid = 1;
  char *validChars = (char*)"@:#+%-._$&/[]";
  char *startChars = (char*)".:";

  if (nameValidityFlags&SDDS_ALLOW_ANY_NAME) {
    return(1);
  }
  ptr = name;
  if (strlen(name)==0) {
    isValid = 0;
  } else if (!(nameValidityFlags&SDDS_ALLOW_V15_NAME)) {
    /* post V1.5 allows only alpha and startChars members as first character */
    /* V1.5 allows alpha, digits, and any validChars members */
    if (!(isalpha(*ptr) || strchr(startChars, *ptr))) {
      isValid = 0;
    }
  }
  while (isValid && *ptr) {
    if (!(isalnum(*ptr) || strchr(validChars, *ptr))) {
      isValid = 0;
    }
    ptr++;
  }
  if (!isValid) {
    return(0);
  }
  return(1);
}

int32_t SDDS_fileIsLocked(char *filename) {
#if defined(F_TEST)
  FILE *fp;
  if (!(fp = fopen(filename, "rb")))
    return 0;
  if (lockf(fileno(fp), F_TEST, 0)==-1) {
    fclose(fp);
    return 1;
  }
  fclose(fp);
  return 0;
#else
  return 0;
#endif
}

int32_t SDDS_lockFile(FILE *fp, char *filename, char *caller) {
#if defined(F_TEST)
  if (lockf(fileno(fp), F_TEST, 0)==-1) {
    fprintf(stderr, "Unable to access file %s--file is locked (%s)", filename, caller);
    return 0;
  }
  if (lockf(fileno(fp), F_TLOCK, 0)==-1) {
    fprintf(stderr, "Unable to establish lock on file %s (%s)", filename, caller);
    return 0;
  }
  return 1;
#else
  return 1;
#endif
}

void *SDDS_calloc(size_t nelem, size_t elem_size) {
  if (elem_size<=0)
    elem_size = 4;
  if (nelem<=0)
    nelem = 1;
  return(calloc(nelem, elem_size));
}

void *SDDS_malloc(size_t size) {
  if (size<=0)
    size = 4;
  return(malloc(size));
}

void SDDS_free(void *mem) {
  /* this is required so the free will be consistent with the malloc.
     On WIN32 the release (optimized) version of malloc is different
     from the debug (unoptimized) version, so debug programs freeing
     memory that was allocated by release, library routines encounter
     problems. */
    free(mem);
}

/***************************
 SDDS_printNamelistField

 C++ Arguments: char **string, int32_t strings

 Results: 0 on success
          1 on failure
 ***************************/
int32_t SDDS_freeStringArray(char **string, int32_t strings) {
  int32_t i;
  if (!string) {
    return(1);
  }
  for (i=0; i<strings; i++) {
    if (string[i]) {
      free(string[i]);
      string[i] = NULL;
    }
  }
  return(0);
}

void *SDDS_realloc(void *old_ptr, size_t new_size) {
  /* this is required because some realloc's don't behave properly when asked to return a
   * pointer to 0 memory.  They return NULL.
   */
  if (new_size<=0)
    new_size = 4;
  /* this is required because some realloc's don't behave properly when given a NULL pointer */
  if (!old_ptr)
    return(SDDS_malloc(new_size));
  else
    return(realloc(old_ptr, new_size));
}

/***********************************************************************************************
 *                        SDDS_printNamelistField                                              *
 *                                                                                             *
 * C++ Arguments: FILE *fp, char *name, char *value                                            *
 *                voidp *gzfp, char *name, char *value                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDS_printNamelistField(FILE *fp, char *name, char *value) {
  char c;
  char *s;
  int32_t hasWhitespace;
  if (!name) {
    return(1);
  }
  if (!value || !strlen(name)) {
    return(0);
  }
  if (!strlen(value)) {
    fprintf(fp, "%s=\"\", ", name);
  } else {
    fprintf(fp, "%s=", name);
    s = value;
    hasWhitespace=0;
    if (SDDS_hasWhitespace(s) || SDDS_stringIsBlank(s) || (strpbrk(s, " ,*$\t\n\b"))) {
      fputc('"', fp);
      hasWhitespace = 1;
    }
    while (s && *s) {
      c = *s++;
      if (c=='!') {
	fputs("\\!", fp);
      } else if (c=='\\') {
	fputs("\\\\", fp);
      } else if (c=='"') {
	fputs("\\\"", fp);
      } else if (c==' ') {
	fputc(' ', fp);   /* don't escape plain spaces */
      } else if (isspace(c) || !isprint(c)) {
	fprintf(fp, "\\%03o", c);
      } else {
	fputc(c, fp);
      }
    }
    if (hasWhitespace) {
      fputc('"', fp);
    }
    fputs(", ", fp);
  }
  return(0);
}

int32_t SDDS_printNamelistField(voidp *gzfp, char *name, char *value) {
  char c;
  char *s;
  int32_t hasWhitespace;
  if (!name) {
    return(1);
  }
  if (!value || !strlen(name)) {
    return(0);
  }
  if (!strlen(value)) {
    gzprintf(gzfp, "%s=\"\", ", name);
  } else {
    gzprintf(gzfp, "%s=", name);
    s = value;
    hasWhitespace=0;
    if (SDDS_hasWhitespace(s) || SDDS_stringIsBlank(s) || (strpbrk(s, " ,*$\t\n\b"))) {
      gzputc(gzfp, '"');
      hasWhitespace = 1;
    }
    while (s && *s) {
      c = *s++;
      if (c=='!') {
	gzputs(gzfp, "\\!");
      } else if (c=='\\') {
	gzputs(gzfp, "\\\\");
      } else if (c=='"') {
	gzputs(gzfp, "\\\"");
      } else if (c==' ') {
	gzputc(gzfp, ' ');   /* don't escape plain spaces */
      } else if (isspace(c) || !isprint(c)) {
	gzprintf(gzfp, "\\%03o", c);
      } else {
	gzputc(gzfp, c);
      }
    }
    if (hasWhitespace) {
      gzputc(gzfp, '"');
    }
    gzputs(gzfp, ", ");
  }
  return(0);
}

/***********************************************************************************************
 *                        SDDS_blankToNull                                                     *
 *                                                                                             *
 * C++ Arguments: char *string                                                                 *
 *                                                                                             *
 * Results: string if not blank or NULL                                                        *
 *          NULL if string is blank or NULL                                                    *
 ***********************************************************************************************/
char *SDDS_blankToNull(char *string) {
  if (!string || SDDS_stringIsBlank(string)) {
    return(NULL);
  }
  return(string);
}

/***********************************************************************************************
 *                        SDDS_stringIsBlank                                                   *
 *                                                                                             *
 * C++ Arguments: char *string                                                                 *
 *                                                                                             *
 * Results: 1 if string is blank                                                               *
 *          0 if string is NULL or not blank                                                   *
 ***********************************************************************************************/
int32_t SDDS_stringIsBlank(char *string) {
  if (!string) {
    return(1);
  }
  while (*string) {
    if (!isspace(*string++)) {
      return(0);
    }
  }
  return(1);
}

/***********************************************************************************************
 *                        SDDS_getTypeName                                                     *
 *                                                                                             *
 * C++ Arguments: int32_t type                                                                 *
 *                                                                                             *
 * Results: parameter type in string form on success                                           *
 *          NULL on failure                                                                    *
 ***********************************************************************************************/
char* SDDS_getTypeName(int32_t type) {
  const char *SDDS_type_name[SDDS_NUM_TYPES] = {
    "real64", "real32", "int32", "uint32", "int16", "uint16", "string", "character"
  };
  if ((type < 1) || (type > SDDS_NUM_TYPES)) {
    return(NULL);
  }
  return((char*)(SDDS_type_name[type - 1]));
}

/***********************************************************************************************
 *                        SDDS_getVer1TypeName                                                  *
 *                                                                                             *
 * C++ Arguments: int32_t type                                                                 *
 *                                                                                             *
 * Results: parameter type in string form on success                                           *
 *          NULL on failure                                                                    *
 ***********************************************************************************************/
char* SDDS_getVer1TypeName(int32_t type) {
  const char *SDDS_type_name[SDDS_NUM_TYPES] = {
    "double", "float", "long", "ulong", "short", "ushort", "string", "character"
  };
  if ((type < 1) || (type > SDDS_NUM_TYPES)) {
    return(NULL);
  }
  return((char*)(SDDS_type_name[type - 1]));
}

/***********************************************************************************************
 *                        SDDS_getVer2TypeName                                                  *
 *                                                                                             *
 * C++ Arguments: int32_t type                                                                 *
 *                                                                                             *
 * Results: parameter type in string form on success                                           *
 *          NULL on failure                                                                    *
 ***********************************************************************************************/
char* SDDS_getVer2TypeName(int32_t type) {
  const char *SDDS_type_name[SDDS_NUM_TYPES] = {
    "double", "float", "long", "ulong", "short", "ushort", "string", "character"
  };
  if ((type < 1) || (type > SDDS_NUM_TYPES)) {
    return(NULL);
  }
  return((char*)(SDDS_type_name[type - 1]));
}

/***********************************************************************************************
 *                        SDDS_hasWhitespace                                                   *
 *                                                                                             *
 * C++ Arguments: char *string                                                                 *
 *                                                                                             *
 * Results: 1 if string has whitespace                                                         *
 *          0 if string is NULL or does not have white space                                   *
 ***********************************************************************************************/
int32_t SDDS_hasWhitespace(char *string) {
  if (!string) {
    return(0);
  }
  while (*string) {
    if (isspace(*string)) {
      return(1);
    }
    string++;
  }
  return(0);
}

/***********************************************************************************************
 *                        SDDS_getNamelist                                                     *
 *                                                                                             *
 * C++ Arguments: char *buffer, int32_t buflen, FILE *fp                                       *
 *                char *buffer, int32_t buflen, voidp *gzfp                                    *
 *                                                                                             *
 * Results: 1 if namelist found                                                                *
 *          0 if namelist not found                                                            *
 ***********************************************************************************************/
int32_t SDDS_getNamelist(char *buffer, int32_t buflen, FILE *fp) {
  char *ptr, *flag, *buffer0;
  int32_t n;
  
  while ((flag=SDDS_fgetsSkipComments(buffer, buflen, fp, '!'))) {
    if ((ptr=strchr(buffer, '&')) && !SDDS_isQuoted(buffer, ptr, '"')) {
      break;
    }
  }
  if (!flag) {
    return(0);
  }
  n = strlen(buffer)-1;
  if (buffer[n]=='\n') {
    buffer[n] = ' ';
    if ((n-1 >= 0) && (buffer[n-1]=='\r')) {
      buffer[n-1] = ' ';
    }
  }
  
  /* check for the beginning of a namelist (an unquoted &) */
  ptr = buffer;
  while (*ptr) {
    if (*ptr=='"') {
      /* skip quoted section */
      ptr++;
      while (*ptr!='"' && *ptr) {
        ptr++;
      }
      if (*ptr) {
        ptr++;
      }
      continue;
    }
    if (*ptr=='&') {
      if (strncmp(ptr, "&end", 4)==0) {
        return(0);
      }
      break;
    }
    ptr++;
  }
  if (!*ptr) {
    return(0);
  }

  /* remove the trailing &end if there is one */
  if ((n=strlen(buffer))>=4) {
    ptr = buffer+n-4;
    while (1) {
      if (*ptr=='&' && 
          (ptr==buffer || *(ptr-1)!='\\') && 
          strncmp(ptr, "&end", 4)==0 && 
          !SDDS_isQuoted(buffer, ptr, '"')) {
        *ptr = 0;
        return(1);
      }
      if (ptr==buffer) {
        break;
      }
      ptr--;
    }
  }
  
  /* read in the remainder of the namelist */
  buffer0 = buffer;
  buflen -= strlen(buffer);
  buffer += strlen(buffer);
  while ((flag=SDDS_fgetsSkipComments(buffer, buflen, fp, '!'))) {
    n = strlen(buffer)-1;
    if (buffer[n]=='\n') {
      buffer[n] = ' ';
      if ((n-1 >= 0) && (buffer[n-1]=='\r')) {
	buffer[n-1] = ' ';
      }
    }
    if ((ptr=strstr(buffer, "&end")) && !SDDS_isQuoted(buffer0, ptr, '"')) {
      return(1);
    }
    buflen -= strlen(buffer);
    buffer += strlen(buffer);
    if (buflen==0) {
      return(0);
    }
  }
  return(0);
}

int32_t SDDS_getNamelist(char *buffer, int32_t buflen, voidp *gzfp) {
  char *ptr, *flag, *buffer0;
  int32_t n;
  
  while ((flag=SDDS_fgetsSkipComments(buffer, buflen, gzfp, '!'))) {
    if ((ptr=strchr(buffer, '&')) && !SDDS_isQuoted(buffer, ptr, '"')) {
      break;
    }
  }
  if (!flag) {
    return(0);
  }
  n = strlen(buffer)-1;
  if (buffer[n]=='\n') {
    buffer[n] = ' ';
    if ((n-1 >= 0) && (buffer[n-1]=='\r')) {
      buffer[n-1] = ' ';
    }
  }
  
  /* check for the beginning of a namelist (an unquoted &) */
  ptr = buffer;
  while (*ptr) {
    if (*ptr=='"') {
      /* skip quoted section */
      ptr++;
      while (*ptr!='"' && *ptr) {
        ptr++;
      }
      if (*ptr) {
        ptr++;
      }
      continue;
    }
    if (*ptr=='&') {
      if (strncmp(ptr, "&end", 4)==0) {
        return(0);
      }
      break;
    }
    ptr++;
  }
  if (!*ptr) {
    return(0);
  }

  /* remove the trailing &end if there is one */
  if ((n=strlen(buffer))>=4) {
    ptr = buffer+n-4;
    while (1) {
      if (*ptr=='&' && 
          (ptr==buffer || *(ptr-1)!='\\') && 
          strncmp(ptr, "&end", 4)==0 && 
          !SDDS_isQuoted(buffer, ptr, '"')) {
        *ptr = 0;
        return(1);
      }
      if (ptr==buffer) {
        break;
      }
      ptr--;
    }
  }
  
  /* read in the remainder of the namelist */
  buffer0 = buffer;
  buflen -= strlen(buffer);
  buffer += strlen(buffer);
  while ((flag=SDDS_fgetsSkipComments(buffer, buflen, gzfp, '!'))) {
    n = strlen(buffer)-1;
    if (buffer[n]=='\n') {
      buffer[n] = ' ';
      if ((n-1 >= 0) && (buffer[n-1]=='\r')) {
	buffer[n-1] = ' ';
      }
    }
    if ((ptr=strstr(buffer, "&end")) && !SDDS_isQuoted(buffer0, ptr, '"')) {
      return(1);
    }
    buflen -= strlen(buffer);
    buffer += strlen(buffer);
    if (buflen==0) {
      return(0);
    }
  }
  return(0);
}

char *SDDS_fgetsSkipComments(char *s, int32_t slen, FILE *fp,
                             char skip_char /* ignore lines that begin with this character */
                             ) {
  while (fgets(s, slen, fp)) {
    if (s[0]!=skip_char) {
      SDDS_cutOutComments(s, skip_char);
      return(s);
    } else if (s[1]=='#') {
      SDDS_parseSpecialComments(s+2);
    }
  } 
  return(NULL);
}

char *SDDS_fgetsSkipComments(char *s, int32_t slen, voidp *gzfp,
                             char skip_char /* ignore lines that begin with this character */
                             ) {
  while (gzgets(gzfp, s, slen)) {
    if (s[0]!=skip_char) {
      SDDS_cutOutComments(s, skip_char);
      return(s);
    } else if (s[1]=='#') {
      SDDS_parseSpecialComments(s+2);
    }
  } 
  return(NULL);
}

char *SDDS_fgetsSkipCommentsResize(char **s, int32_t *slen, FILE *fp,
                              char skip_char    /* ignore lines that begin with this character */
                              ) {
  int32_t spaceLeft, length, newLine;
  char *sInsert, *fgetsReturn;

  sInsert = *s;
  spaceLeft = *slen;
  newLine = 1;
  while ((fgetsReturn=fgets(sInsert, spaceLeft, fp))) {
    if (newLine && sInsert[0]=='!') {
      continue;
    }
    SDDS_cutOutComments(sInsert, skip_char);
    length = strlen(sInsert);
    if (sInsert[length-1]!='\n' && !feof(fp)) {
       /* buffer wasn't long enough to get the whole line.  Resize and add more data. */
      spaceLeft = *slen;
      *slen = *slen*2;
      *s = (char*)SDDS_realloc(*s, sizeof(**s)**slen);
      sInsert = *s + strlen(*s);
      newLine = 0;
    } else {
      break;
    }
  }
  if (!fgetsReturn) {
    return(NULL);
  }
  return(*s);
}

char *SDDS_fgetsSkipCommentsResize(char **s, int32_t *slen, voidp *gzfp,
                                   char skip_char    /* ignore lines that begin with this character */
                                   ) {
  int32_t spaceLeft, length, newLine;
  char *sInsert, *fgetsReturn;

  sInsert = *s;
  spaceLeft = *slen;
  newLine = 1;
  while ((fgetsReturn=gzgets(gzfp, sInsert, spaceLeft))) {
    if (newLine && sInsert[0]=='!') {
      continue;
    }
    SDDS_cutOutComments(sInsert, skip_char);
    length = strlen(sInsert);
    if (sInsert[length-1]!='\n' && !gzeof(gzfp)) {
      /* buffer wasn't long enough to get the whole line.  Resize and add more data. */
      spaceLeft = *slen;
      *slen = *slen*2;
      *s = (char*)SDDS_realloc(*s, sizeof(**s)**slen);
      sInsert = *s + strlen(*s);
      newLine = 0;
    } else {
      break;
    }
  }
  if (!fgetsReturn) {
    return(NULL);
  }
  return(*s);
}

void SDDS_cutOutComments(char *s, char cc) {
  int32_t length, hasNewline;
  char *s0;

  if (!cc || !s) {
    return;
  }

  hasNewline = 0;
  length = strlen(s);
  if (s[length-1]=='\n') {
    hasNewline = 1;
  }

  if (*s==cc) {
    /* check for special information */
    if (*(s+1)=='#') {
      SDDS_parseSpecialComments(s+2);
    }
    *s = 0;
    return;
  }
  s0 = s;
  while (*s) {
    if (*s=='"') {
      while (*++s && *s!='"')
        ;
      if (!*s) {
        return;
      }
      s++;
      continue;
    }
    if (*s==cc) {
      if (s!=s0 && *(s-1)=='\\') {
        strcpy(s-1, s);
      } else {
        if (hasNewline) {
          *s = '\n';
          *(s+1) = 0;
        } else {
          *s = 0;
        }
        return;
      }
    }
    s++;
  }
}

#define COMMENT_COMMANDS 3
static const char *commentCommandName[COMMENT_COMMANDS] = {
  "big-endian", "little-endian", 
  "fixed-rowcount",
};
#define SDDS_BIGENDIAN_SEEN      0x0001UL
#define SDDS_LITTLEENDIAN_SEEN   0x0002UL
#define SDDS_FIXED_ROWCOUNT_SEEN 0x0004UL
static uint32_t commentCommandFlag[COMMENT_COMMANDS] = {
  SDDS_BIGENDIAN_SEEN, SDDS_LITTLEENDIAN_SEEN,
  SDDS_FIXED_ROWCOUNT_SEEN,
};
static uint32_t commentFlags = 0;

uint32_t SDDS_getSpecialCommentsModes() {
  return commentFlags;
}

void SDDS_resetSpecialCommentsModes() {
  commentFlags = 0;
}

void SDDS_parseSpecialComments(char *s) {
  char buffer[SDDS_MAXLINE];
  int32_t size;
  int32_t i;
  size = strlen(s);

  while (SDDS_getToken(&s, &size, buffer, SDDS_MAXLINE)>0) {
    for (i=0; i<COMMENT_COMMANDS; i++) {
      if (strcmp(buffer, commentCommandName[i])==0) {
        commentFlags |= commentCommandFlag[i];
        break;
      }
    }
  }
}

int32_t SDDS_getToken(char **st, int32_t *strlength, char *buffer, int32_t buflen) {
  char *ptr0, *ptr1, *escptr, *s;
  
  s = *st;

  /* save the pointer to the head of the string */
  ptr0 = s;

  /* skip leading white-space */  
  while (isspace(*s)) {
    s++;
  }
  if (*s==0) {
    return(-1);
  }
  ptr1 = s;

  if (*s=='"') {
    /* if quoted string, skip to next quotation mark */
    ptr1 = s+1;    /* beginning of actual token */
    do {
      s++;
      escptr = NULL;
      if (*s=='\\' && *(s+1)=='\\') {
        /* skip and remember literal \ (indicated by \\ in the string) */
        escptr = s+1;
        s += 2;
      }
    } while (*s && (*s!='"' || (*(s-1)=='\\' && (s-1)!=escptr)));
    /* replace trailing quotation mark with a space */
    if (*s=='"') {
      *s = ' ';
    }
  } else {
    /* skip to first white-space following token */
    do {
      s++;
      /* imbedded quotation marks are handled here */
      if (*s=='"' && *(s-1)!='\\') {
        while (*++s && !(*s=='"' && *(s-1)!='\\'))
          ;
      }
    } while (*s && !isspace(*s));
  }

  if ((int32_t)(s-ptr1)>=buflen) {
    return(-1);
  }
  strncpy(buffer, ptr1, s-ptr1);
  buffer[s-ptr1]=0;
  
  /* update the original string to delete the token */
  *st += s-ptr0;
  *strlength -= s-ptr0;

  /* return the string length including whitespace */
  return((int32_t)(s-ptr1));
}

int32_t SDDS_isQuoted(char *string, char *position, char quotation_mark) {
  int32_t in_quoted_section;
  char *string0;

  if (*position==quotation_mark) {
    return(1);
  }

  in_quoted_section = 0;
  string0 = string;
  while (*string) {
    if (*string==quotation_mark && (string==string0 || *(string-1)!='\\')) {
      in_quoted_section = !in_quoted_section;
    } else if (string==position) {
      return(in_quoted_section);
    }
    string++;
  }
  return(0);
}

char *SDDS_prepareToParseTagValuePairs(char *s) { 
  char *ptr;
  int32_t length;
  
  /* remove the trailing &end if there is one */
  if ((length=strlen(s))>=4) {
    ptr = s+length-4;
    while (1) {
      if (*ptr=='&' && (ptr==s || *(ptr-1)!='\\') && strncmp(ptr, "&end", 4)==0 
          && !SDDS_isQuoted(s, ptr, '"')) {
        *ptr = 0;
        break;
      }
      if (ptr==s) {
        break;
      }
      ptr--;
    }
  }
  
  /* make sure there is no group name */
  while (*s==' ') {
    s++;
  }
  if (*s=='&') {
    while (*s && *s!=' ') {
      s++;
    }
  }
  return(s);
}

/******************************************
 SDDS_copyString

 C++ Arguments: char **target, char *source 

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_copyString(char **target, char *source) {
  if (!source) {
    *target = NULL;
  } else {
    if (!(*target=(char*)SDDS_malloc(sizeof(**target)*(strlen(source)+1)))) {
      return(1);
    }
    strcpy(*target, source);
  }
  return(0);
}

/* \ddd = octal value
 * ANSI escape codes (n t b r f v \ ' " a ?
 * SDDS escape codes (! )
 */
void SDDS_interpretEscapes(char *s) {
  char *ptr;
  int32_t count;
  
  ptr = s;
  while (*s) {
    if (*s!='\\')
      *ptr++ = *s++;
    else {
      s++;
      if (!*s) {
        *ptr++ = '\\';
        *ptr++ = 0;
        return;
      }
      switch (*s) {
      case 'n':
        *ptr++ = '\n';
        s++;
        break;
      case 't':
        *ptr++ = '\t';
        s++;
        break;
      case 'b':
        *ptr++ = '\b';
        s++;
        break;
      case 'r':
        *ptr++ = '\r';
        s++;
        break;
      case 'f':
        *ptr++ = '\f';
        s++;
        break;
      case 'v':
        *ptr++ = '\v';
        s++;
        break;
      case '\\':
        *ptr++ = '\\';
        s++;
        break;
      case '\'':
        *ptr++ = '\'';
        s++;
        break;
      case '"':
        *ptr++ = '\"';
        s++;
        break;
      case 'a':
        *ptr++ = '\a';
        s++;
        break;
      case '?':
        *ptr++ = '\?';
        s++;
        break;
      case '!':
        *ptr++ = '!';
        s++;
        break;
      default:
        if (*s>='0' && *s<='9') {
          *ptr = 0;
          count = 0;
          while (++count<=3 && *s>='0' && *s<='9') 
            *ptr = 8*(*ptr) + *s++ - '0';
          ptr++;
        }
        else {
          *ptr++ = '\\';
        }
        break;
      }
    }
  }
  *ptr = 0;
}

/******************************************
 SDDS_bufferedRead

 C++ Arguments: void *target, size_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile
                void *target, size_t targetSize, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_bufferedRead(void *target, size_t targetSize, FILE *fp, 
                           SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  if (!fBuffer->bufferSize) {
    /* just read into users buffer or seek if no buffer given */
    if (!target) {
      return(fseek(fp, targetSize, SEEK_CUR));
    } else {
      return(fread(target, 1, targetSize, fp)!=targetSize);
    }
  }
  if ((fBuffer->bytesLeft-=targetSize)>=0) {
    /* sufficient data is already in the buffer */
    if (target) {
      memcpy((char*)target, (char*)fBuffer->data, targetSize);
    }
    fBuffer->data += targetSize;
    return(0);
  } else {
    /* need to read additional data into buffer */
    int32_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize;  /* adds back amount subtracted above */
    /* first, use the data that is already available. this cleans out the buffer */
    if ((offset=fBuffer->bytesLeft)) {
      /* some data is available in the buffer */
      if (target) {
        memcpy((char*)target, (char*)fBuffer->data, offset);
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;
    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target) {
        return(fseek(fp, bytesNeeded, SEEK_CUR));
      } else {
        return(fread((char*)target+offset, 1, bytesNeeded, fp)!=bytesNeeded);
      }
    }
    /* fill the buffer */
    if ((fBuffer->bytesLeft = fread(fBuffer->data, 1, fBuffer->bufferSize, fp))<bytesNeeded) {
      return(1);
    }
    if (target) {
      memcpy((char*)target+offset, (char*)fBuffer->data, bytesNeeded);
    }
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return(0);
  }
}

int32_t SDDS_bufferedRead(void *target, size_t targetSize, voidp *gzfp, 
                           SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  if (!fBuffer->bufferSize) {
    ((SDDSFile*)sddsfile)->setError((char*)"You must presently have a nonzero file buffer to use zLib (reading/writing .gz files");
    return(1);
  }
  if ((fBuffer->bytesLeft-=targetSize)>=0) {
    /* sufficient data is already in the buffer */
    if (target) {
      memcpy((char*)target, (char*)fBuffer->data, targetSize);
    }
    fBuffer->data += targetSize;
    return(0);
  } else {
    /* need to read additional data into buffer */
    int32_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize;  /* adds back amount subtracted above */
    /* first, use the data that is already available. this cleans out the buffer */
    if ((offset=fBuffer->bytesLeft)) {
      /* some data is available in the buffer */
      if (target) {
        memcpy((char*)target, (char*)fBuffer->data, offset);
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;
    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target) {
        return(gzseek(gzfp, targetSize, SEEK_CUR));
      } else {
        return(gzread(gzfp, (char*)target, targetSize)!=targetSize);
      }
    }
    /* fill the buffer */
    if ((fBuffer->bytesLeft = gzread(gzfp, fBuffer->data, fBuffer->bufferSize))<bytesNeeded) {
      return(1);
    }
    if (target) {
      memcpy((char*)target+offset, (char*)fBuffer->data, bytesNeeded);
    }
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return(0);
  }
}

/******************************************
 SDDS_bufferedWrite

 C++ Arguments: void *target, size_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile
                void *target, size_t targetSize, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_bufferedWrite(void *target, size_t targetSize, FILE *fp, 
                           SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  if (!fBuffer->bufferSize) {
    return(fwrite(target, 1, targetSize, fp)!=targetSize);
  }
  if ((fBuffer->bytesLeft-=targetSize)>=0) {
    memcpy((char*)fBuffer->data, (char*)target, targetSize);
    fBuffer->data += targetSize;
    return(0);
  } else {
    int32_t lastLeft;
    /* add back what was subtracted in test above.
     * lastLeft is the number of bytes left in the buffer before doing anything 
     * and also the number of bytes from the users data that get copied into the buffer.
     */
    lastLeft = (fBuffer->bytesLeft += targetSize);  
    /* copy part of the data into the buffer and write the buffer out */
    memcpy((char*)fBuffer->data, (char*)target, fBuffer->bytesLeft);
    if (fwrite(fBuffer->buffer, 1, fBuffer->bufferSize, fp)!=fBuffer->bufferSize)
      return(1);
    if (fflush(fp)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Problem flushing file (SDDS_bufferedWrite)");
      return(1);
    }
    /* reset the data pointer and the bytesLeft value.
     * also, determine if the remaining data is too large for the buffer.
     * if so, just write it out.
     */
    fBuffer->data = fBuffer->buffer;
    if ((targetSize -= lastLeft)>(fBuffer->bytesLeft = fBuffer->bufferSize)) {
      return(fwrite((char*)target+lastLeft, 1, targetSize, fp)!=targetSize);
    }
    /* copy remaining data into the buffer.
     * could do this with a recursive call, but this is more efficient.
     */
    memcpy((char*)fBuffer->data, (char*)target+lastLeft, targetSize);
    fBuffer->data += targetSize;
    fBuffer->bytesLeft -= targetSize;
    return(0);
  }
}

int32_t SDDS_bufferedWrite(void *target, size_t targetSize, voidp *gzfp, 
                           SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  if (!fBuffer->bufferSize) {
    ((SDDSFile*)sddsfile)->setError((char*)"You must presently have a nonzero file buffer to use zLib (reading/writing .gz files");
  }
  if ((fBuffer->bytesLeft-=targetSize)>=0) {
    memcpy((char*)fBuffer->data, (char*)target, targetSize);
    fBuffer->data += targetSize;
    return(0);
  } else {
    int32_t lastLeft;
    lastLeft = (fBuffer->bytesLeft += targetSize);  
    memcpy((char*)fBuffer->data, (char*)target, fBuffer->bytesLeft);
    if (gzwrite(gzfp, fBuffer->buffer, fBuffer->bufferSize)!=fBuffer->bufferSize)
      return(1);
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
    return(SDDS_bufferedWrite((char*)target+lastLeft, targetSize-lastLeft, gzfp, fBuffer, sddsfile));
  }
}

/******************************************
 SDDS_flushBuffer

 C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile
                voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_flushBuffer(FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t writeBytes;
  if (!fBuffer->bufferSize) {
    if (fflush(fp)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Problem flushing file (SDDS_flushBuffer)");
      return(1);
    }
    return(0);
  }
  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (writeBytes<0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to flush buffer: negative byte count (SDDS_flushBuffer).");
      return(1);
    }
    if (fwrite(fBuffer->buffer, 1, writeBytes, fp)!=writeBytes) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to flush buffer: write operation failed (SDDS_flushBuffer).");
      return(1);
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  if (fflush(fp)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Problem flushing file (SDDS_flushBuffer)");
    return(1);
  }
  return(0);
}

int32_t SDDS_flushBuffer(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t writeBytes;
  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (gzwrite(gzfp, fBuffer->buffer, writeBytes)!=writeBytes) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to flush buffer: write operation failed (SDDS_flushBuffer).");
      return(1);
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  return(0);
}

/******************************************
 SDDS_writeBinaryString

 C++ Arguments: char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile
                char *string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_writeBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t length;
  static char *dummy_string = (char*)"";
  if (string == NULL) {
    string = dummy_string;
  }
  length = strlen(string);
  if (SDDS_bufferedWrite(&length, sizeof(length), fp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write string--error writing length (SDDS_writeBinaryString)");
    return(1);
  }
  if (length && SDDS_bufferedWrite(string, sizeof(*string)*length, fp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write string--error writing contents (SDDS_writeBinaryString)");
    return(1);
  }
  return(0);
}

int32_t SDDS_writeBinaryString(char *string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t length;
  static char *dummy_string = (char*)"";
  if (string == NULL) {
    string = dummy_string;
  }
  length = strlen(string);
  if (SDDS_bufferedWrite(&length, sizeof(length), gzfp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write string--error writing length (SDDS_writeBinaryString)");
    return(1);
  }
  if (length && SDDS_bufferedWrite(string, sizeof(*string)*length, gzfp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write string--error writing contents (SDDS_writeBinaryString)");
    return(1);
  }
  return(0);
}

/******************************************
 SDDS_readBinaryString

 C++ Arguments: char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile
                char *string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile

 Results: 0 on success
          1 on failure
*******************************************/
int32_t SDDS_readBinaryString(char **string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t length;
  if (SDDS_bufferedRead(&length, 4, fp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read string length (SDDS_readBinaryString)");
    return(1);
  }
  if (!(*string=(char*)SDDS_malloc(sizeof(char)*(length+1)))) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to allocate memory (SDDS_readBinaryString)");
    return(1);
  }
  if (length) {
    if (SDDS_bufferedRead(*string, sizeof(char)*length, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read string (SDDS_readBinaryString)");
      return(1);
    }
  }
  (*string)[length] = 0;
  return(0);
}

int32_t SDDS_readBinaryString(char **string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile) {
  int32_t length;
  if (SDDS_bufferedRead(&length, 4, gzfp, fBuffer, sddsfile)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read string length (SDDS_readBinaryString)");
    return(1);
  }
  if (!(*string=(char*)SDDS_malloc(sizeof(char)*(length+1)))) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to allocate memory (SDDS_readBinaryString)");
    return(1);
  }
  if (length) {
    if (SDDS_bufferedRead(*string, sizeof(char)*length, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read string (SDDS_readBinaryString)");
      return(1);
    }
  }
  (*string)[length] = 0;
  return(0);
}

/******************************************
 SDDS_registerProgramName

 C++ Arguments: const char *name

 Results: none
*******************************************/
static char *registeredProgramName = NULL;
void SDDS_registerProgramName(const char *name) {
  if (name != NULL) {
    if (SDDS_copyString(&registeredProgramName, (char*)name)) {
      registeredProgramName = NULL;
    }
  } else {
    registeredProgramName = NULL;
  }
}

char *SDDS_getProgramName() {
  return(registeredProgramName);
}

/******************************************
 SDDS_bomb

 C++ Arguments: char *message

 Please try to avoid calling this function and 
 exit your code from the main routine instead.
 I understand that this may not always be possible.

 Results: none
*******************************************/
void SDDS_bomb(char *message) {
  if (registeredProgramName)
    fprintf(stderr, "Error (%s): %s\n", registeredProgramName, message?message:"?");
  else
    fprintf(stderr, "Error: %s\n", message?message:"?");
  exit(1);
}

void SDDS_warning(char *message) {
  if (registeredProgramName)
    fprintf(stderr, "Warning (%s): %s\n", registeredProgramName, message?message:"?");
  else
    fprintf(stderr, "Warning: %s\n", message?message:"?");
}
