#ifndef NN_TOKENIZE_H
#define NN_TOKENIZE_H

#include <string.h>

static char* nn_tokenize(char* text, const char* separators, char** state)
{
    char* token;

    if (text == NULL)
        text = *state;
    if (text == NULL)
        return NULL;

    text += strspn(text, separators);
    if (*text == 0) {
        *state = NULL;
        return NULL;
    }

    token = text;
    text += strcspn(text, separators);
    if (*text == 0)
        *state = NULL;
    else {
        *text = 0;
        *state = text + 1;
    }

    return token;
}

#endif
