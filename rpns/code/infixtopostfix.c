/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.8  2008/09/02 20:38:45  soliday
 * Fixed a bug with negative numbers. It was getting the negative sign
 * confused with a minus sign.
 *
 * Revision 1.7  2005/06/28 21:18:57  soliday
 * Fixed problem with strings containing numbers and numbers containing exponents.
 *
 * Revision 1.6  2003/10/21 19:50:12  soliday
 * Updated so that it does not abort an on error and returns the error code.
 *
 * Revision 1.5  2002/08/14 16:28:57  soliday
 * Added Open License
 *
 * Revision 1.4  2001/04/17 19:09:27  soliday
 * Replaced strlcpy with strncpy because of problems on Linux.
 *
 * Revision 1.3  2001/04/10 18:29:46  soliday
 * Fixed bug when no spaces are around a variable.
 *
 * Revision 1.2  2001/04/10 16:43:05  soliday
 * It now handles variables correctly.
 *
 * Revision 1.1  2001/04/09 20:37:01  soliday
 * First version of infix to postfix conversion routine.
 *
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include "rpn.h"

/* general buffer sizes */
#define IFPF_STACK_SIZE  2048
#define IFPF_TOKEN_SIZE   50

struct ifpf_oporder {
  char op[IFPF_TOKEN_SIZE];
  int order;
};

/* table of order of operations and operator functions */
struct ifpf_oporder ifpf_opo[] = {
  {"Jn",    100},
  {"Yn",    100},
  {"abs",   100},
  {"acos",  100},
  {"asin",  100},
  {"atan",  100},
  {"acosh",  100},
  {"asinh",  100},
  {"atanh",  100},
  {"atan2", 100},
  {"betai", 100},
  {"cei1",  100},
  {"cei2",  100},
  {"cos",   100},
  {"cosh",  100},
  {"dacos", 100},
  {"dasin", 100},
  {"datan", 100},
  {"dcos",  100},
  {"dsin",  100},
  {"dtan",  100},
  {"erf",   100},
  {"erfc",  100},
  {"exp",   100},
  {"gamP",  100},
  {"gamQ",  100},
  {"int",   100},
  {"ln",    100},
  {"lngam", 100},
  {"log",   100},
  {"poiSL", 100},
  {"pow",   100},
  {"sin",   100},
  {"sinh",  100},
  {"sqr",   100},
  {"sqrt",  100},
  {"tan",   100},
  {"tanh",  100},
  {"*",      90},
  {"/",      90},
  {"+",      80},
  {"-",      80},
  {">",      70},
  {"<",      70},
  {"==",     70},
  {"!",      60},
  {"&&",     60},
  {"||",     60},
  {"=",      -1},
  {"=str",   -1},
  {"?",      -1},
  {"@",      -1},
  {"[",      -1},
  {"]",      -1},
  {"cle",    -1},
  {"clos",   -1},
  {"csh",    -1},
  {"cshs",   -1},
  {"draw",   -1},
  {"execn",  -1},
  {"execs",  -1},
  {"format", -1},
  {"fprf",   -1},
  {"getformat", -1},
  {"gets",   -1},
  {"grnd",   -1},
  {"grndl",  -1},
  {"help",   -1},
  {"isinf",  -1},
  {"isnan",  -1},
  {"mal",    -1},
  {"n=",     -1},
  {"open",   -1},
  {"pop",    -1},
  {"popl",   -1},
  {"pops",   -1},
  {"puts",   -1},
  {"quit",   -1},
  {"rdn",    -1},
  {"rnd",    -1},
  {"rs",     -1},
  {"rup",    -1},
  {"scan",   -1},
  {"sleep",  -1},
  {"smem",   -1},
  {"sprf",   -1},
  {"srnd",   -1},
  {"stlv",   -1},
  {"sto",    -1},
  {"stop",   -1},
  {"sudf",   -1},
  {"sumn",   -1},
  {"swap",   -1},
  {"tsci",   -1},
  {"ttr",    -1},
  {"udf",    -1},
  {"view",   -1},
  {"vlog",   -1},
  {"vstr",   -1},
  {"xstr",   -1},
  {"(",     -10}
};

struct ifpf_stack {
  void *elem;
  size_t size, top;
};

struct ifpf_stack *ifpf_init_stack(size_t n);
void ifpf_free_stack(struct ifpf_stack *stk);
int ifpf_push(struct ifpf_stack *stk, void *datum, size_t size);
int ifpf_pop(struct ifpf_stack *stk, void *datum, size_t *size);
int ifpf_peek(struct ifpf_stack *stk, void *datum, size_t *size);
int ifpf_oporder(char *op);
char *ifpf_get_token(const char *ifix);

/* allocate and initialise a stack of size n */
struct ifpf_stack *ifpf_init_stack(size_t n) {
  struct ifpf_stack *ssp;

  ssp = malloc(sizeof *ssp);
  if (ssp) {
    ssp->elem = malloc(n);
    if (!ssp->elem) {
      free(ssp);
      return NULL;
    }

    ssp->size = n;
    ssp->top = 0;
  }
  
  return ssp;
}

/* free all memory associated with a stack */
void ifpf_free_stack(struct ifpf_stack *stk) {
  if (stk) {
    free(stk->elem);
    free(stk);
  }
}

/* store datum of given size in a stack */
int ifpf_push(struct ifpf_stack *stk, void *datum, size_t size) {
  assert(stk != NULL);
  assert(datum != NULL);
  /* if stack not full */
  if ((stk->size - stk->top) > size) {
    /* push it */
    memcpy(&(((unsigned char *)stk->elem)[stk->top]), datum, size);
    
    /* update TOS indicator */
    stk->top += size;
    
    return 0;
  }
  else
    return 1;
}

/* remove datum of given size from a stack */
int ifpf_pop(struct ifpf_stack *stk, void *datum, size_t *size) {
  char opString[IFPF_TOKEN_SIZE];
  size_t s=2;
  assert(stk != NULL);
  assert(datum != NULL);

  while (s <= stk->top) {
    if (((unsigned char *)stk->elem)[stk->top - s] == '\t')
      break;
    s++;
  }
  s--;
  *size = s;
  /* if stack not empty */
  if ((stk->top >= s) && (stk->top != 0)) {
    /* update TOS indicator */
    stk->top -= s;
    /* get datum */
    strncpy(opString, ((char *)stk->elem)+stk->top, s-1);
    opString[s-1] = '\0';
    strcpy(datum, opString);
    return 0;
  }
  else
    return 1;
}

/* view datum of given size on top of a stack */
int ifpf_peek(struct ifpf_stack *stk, void *datum, size_t *size) {
  char opString[IFPF_TOKEN_SIZE];
  size_t s=2;
  assert(stk != NULL);
  assert(datum != NULL);
  
  while (s <= stk->top) {
    if (((unsigned char *)stk->elem)[stk->top - s] == '\t')
      break;
    s++;
  }
  s--;
  *size = s;
  /* if stack not empty */
  
  if ((stk->top >= s) && (stk->top != 0)) {
    /* get datum */
    strncpy(opString, ((char *)stk->elem)+(stk->top - s), s-1);
    opString[s-1] = '\0';
    strcpy(datum, opString);
    return 0;
  }
  else
    return 1;
}

/* get the "rank" of an operator (ie., to determine order of
   operations) */
int ifpf_oporder(char *op) {
  int i;
  int j;
  char opString[IFPF_TOKEN_SIZE];

  j = strlen(op);
  if ((j > 0) && (op[j - 1] == '\t')) {
    int copy_len = j - 1;  // Exclude the trailing tab
    if (copy_len >= IFPF_TOKEN_SIZE) {
      return 0;  // Prevent buffer overflow
    }
    memcpy(opString, op, copy_len);
    opString[copy_len] = '\0';  // Ensure null termination
  } else {
    if (j >= IFPF_TOKEN_SIZE) {
      return 0;  // Prevent buffer overflow
    }
    strncpy(opString, op, IFPF_TOKEN_SIZE - 1);
    opString[IFPF_TOKEN_SIZE - 1] = '\0';  // Ensure null termination
  }

  /* search for operator */
  for (i = 0; i < (sizeof ifpf_opo / sizeof *ifpf_opo); i++) {
    if (strcmp(ifpf_opo[i].op, opString) == 0) {
      /* found - return its rank */
      return ifpf_opo[i].order;
    }
  }

  /* not found - return 0 */
  return 0;
}

/* get the next token (number or operator) from string */
char *ifpf_get_token(const char *ifix) {
  static const char *expr = NULL;
  static char token[IFPF_TOKEN_SIZE];
  int i, exponent=0;
  static int first=0;
  
  if (ifix) {
    expr = ifix;
    first = 1;
  }

  assert(expr != NULL);

  /* skip white space */
  while (isspace((unsigned char)*expr))
    expr++;

  /* operand or operator? */
  if (isdigit((unsigned char)*expr) || *expr == '.' || ((first == 1) && (*expr == '-')) || ((first == 1) && (*expr == '+'))) {
    /* operand */
    first = 0;
    for (i = 0; i < (IFPF_TOKEN_SIZE - 1) && 
	   (isdigit((unsigned char)(token[i] = *expr)) || 
	    token[i] == '.' ||
            token[i] == 'e' || token[i] == 'E' ||
            token[i] == '-' || token[i] == '+'); i++) {
      if (i != 0) {
        if ((token[i] == '-' || token[i] == '+') && (exponent == 0))
          break;
      }
      if (token[i] == 'e' || token[i] == 'E')
        exponent=1;
      else 
        exponent=0;
      expr++;
    }
    token[i] = '\0';
  } else if (*expr) {
    first = 0;
    /* operator */
    if ((*expr == '(') || (*expr == ')') ||
	(*expr == '+') || (*expr == '-') ||
	(*expr == '*') || (*expr == '/') ||
	(*expr == '>') || (*expr == '<')) {
      /* operator */
      if ((*expr == '(') || 
          (*expr == '+') || (*expr == '-') || 
          (*expr == '*') || (*expr == '/') ||
          (*expr == '>') || (*expr == '<'))
        first = 1;
      token[0] = *expr++;
      token[1] = '\t';
      token[2] = '\0';
    } else {
      for (i = 0; i < (IFPF_TOKEN_SIZE - 1) && 
	     !(isspace((unsigned char)(token[i] = *expr)) || 
	       token[i] == '.' ||
	       token[i] == ')' || token[i] == '(' ||
	       token[i] == '+' || token[i] == '-' ||
	       token[i] == '*' || token[i] == '/' ||
	       token[i] == '>' || token[i] == '<'); i++) {
	expr++;
      }
      token[i] = '\t';
      i++;
      token[i] = '\0';
    }
  } else {
    /* no more tokens */
    expr = NULL;
    return NULL;
  }
  return token;
}

/* convert infix to postfix notation */
int if2pf(char *pfix, char *ifix, size_t size) {
  struct ifpf_stack *opstk;
  char op[IFPF_TOKEN_SIZE];
  char *token;
  size_t i;
  size_t opSize = 0;
  int err;
  
  assert(pfix != NULL);
  assert(ifix != NULL);
  
  *pfix = '\0';
  
  /* initialise operator stack */
  opstk = ifpf_init_stack(IFPF_STACK_SIZE);
  if (!opstk) {
    fprintf(stderr, "Memory allocation error.\n");
    return(1);
  }
  
  /* while there are tokens in the infix string */
  i = 0;
  strcat(ifix, " ");
  token = ifpf_get_token(ifix);
  while (token) {
    /* if next token is operand */
    if (isdigit((unsigned char)*token) || 
	*token == '.') {
      /* if enough space */
      if ((size - i - 1) > (strlen(token) + 1)) {
	/* put token in postfix string */
	strcat(pfix, token);
	strcat(pfix, " ");
	i += strlen(token) + 1;
      } else {
	ifpf_free_stack(opstk);
	fprintf(stderr, "Error: input too long.\n");
	return(1);
      }
    } else if (ifpf_oporder(token) == -1) {
      /* pop any remaining operators & place in postfix string */
      while (!ifpf_peek(opstk, &op, &opSize)) {
	/* if enough space */
	if ((size - i - 1) > opSize) {
	  /* pop operator & store in postfix string */
	  ifpf_pop(opstk, &op, &opSize);
	  strcat(pfix, op);
	  strcat(pfix, " ");
	  i += strlen(op) + 1;
	} else {
	  ifpf_free_stack(opstk);
	  fprintf(stderr, "Error: input too long.\n");
	  return(1);
	}
      }
      if (token[strlen(token)-1] == '\t')
	token[strlen(token)-1] = '\0';

      /* if enough space */
      if ((size - i - 1) > (strlen(token) + 1)) {
	/* put token in postfix string */
	strcat(pfix, token);
	strcat(pfix, " ");
	i += strlen(token) + 1;
      } else {
	ifpf_free_stack(opstk);
	fprintf(stderr, "Error: input too long.\n");
	return(1);
      }
    } else if ((ifpf_oporder(token) == 0) &&
	       (*token != '(') &&
	       (*token != ')')) {
      if (token[strlen(token)-1] == '\t')
	token[strlen(token)-1] = '\0';

      /* if enough space */
      if ((size - i - 1) > (strlen(token) + 1)) {
	/* put token in postfix string */
	strcat(pfix, token);
	strcat(pfix, " ");
	i += strlen(token) + 1;
      } else {
	ifpf_free_stack(opstk);
	fprintf(stderr, "Error: input too long.\n");
	return(1);
      }
    } else {
      switch (*token) {
      default:
	/* while stack not empty & operator not greater than the
	   operator at TOS */
	while (!(ifpf_peek(opstk, &op, &opSize)) &&
	       (ifpf_oporder(token) <= ifpf_oporder(op))) {
	  /* if enough space */
	  if ((size - i - 1) > opSize) {
	    /* pop operator & store in postfix string */
	    ifpf_pop(opstk, &op, &opSize);
	    strcat(pfix, op);
	    strcat(pfix, " ");
	    i += strlen(op) + 1;
	  } else {
	    ifpf_free_stack(opstk);
	    fprintf(stderr, "Error: input too long.\n");
	    return(1);
	  }
	}
	break;
	
      case '(':
	/* immediately push operator on stack */
	if (ifpf_push(opstk, token, sizeof(char)*strlen(token))) {
	  ifpf_free_stack(opstk);
	  fprintf(stderr, "Error: stack full.\n");
	  return(1);
	}
	
	/* continue normal scanning */
	token = ifpf_get_token(NULL);
	continue;
	
      case ')':
	/* pop operators until left bracket found */
	while (!(err = ifpf_pop(opstk, &op, &opSize)) && (strcmp(op, "(") != 0)) {
	  /* if enough space */
	  if ((size - i - 1) > opSize) {
	    /* store in postfix string */
	    strcat(pfix, op);
	    strcat(pfix, " ");
	    i += strlen(op) + 1;
	  } else {
	    ifpf_free_stack(opstk);
	    fprintf(stderr, "Error: input too long.\n");
	    return(1);
	  }
	}

	if (err) {
	  ifpf_free_stack(opstk);
	  fprintf(stderr, "Error: stack empty.\n");
	  return(1);
	}
	
	/* continue normal scanning */
	token = ifpf_get_token(NULL);
	continue;
      }
      if (ifpf_push(opstk, token, sizeof(char)*strlen(token))) {
	ifpf_free_stack(opstk);
	fprintf(stderr, "Error: stack full.\n");
	return(1);
      }
    }
    
    token = ifpf_get_token(NULL);
  }
  
  /* pop any remaining operators & place in postfix string */
  while (!ifpf_peek(opstk, &op, &opSize)) {
    /* if enough space */
    if ((size - i - 1) > opSize) {
      /* pop operator & store in postfix string */
      ifpf_pop(opstk, &op, &opSize);
      strcat(pfix, op);
      strcat(pfix, " ");
      i += strlen(op) + 1;
    } else {
      ifpf_free_stack(opstk);
      fprintf(stderr, "Error: input too long.\n");
      return(1);
    }
  }
  
  ifpf_free_stack(opstk);
  return 0;
}
