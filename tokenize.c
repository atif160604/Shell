#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_INPUT 255

bool is_special_char(char c) {
    return (c == '<' || c == '>' || c == '|' || c == ';' || c == '(' || c == ')');
}

char** tokenize(char *input, int *token_count) {
    char **tokens = malloc(MAX_INPUT * sizeof(char*));
    int count = 0;

    char *start = input, *end = input;
    bool in_quote = false;

    while(*end) {
        if(*end == '"') {
            in_quote = !in_quote;
            if(!in_quote) {
                *end = '\0';
                tokens[count++] = strdup(start + 1);
                start = end + 1;
            }
        } else if(!in_quote && is_special_char(*end)) {
            if(start != end) {
                char special = *end;
                *end = '\0';
                tokens[count++] = strdup(start);
                *end = special;
            }
            tokens[count++] = strndup(end, 1);
            start = end + 1;
        } else if(!in_quote && (*end == ' ' || *end == '\t' || *end == '\n')) {
            if(start != end) {
                *end = '\0';
                tokens[count++] = strdup(start);
            }
            start = end + 1;
        }
        end++;
    }

    if(start != end) {
        tokens[count++] = strdup(start);
    }
    *token_count = count;
    return tokens;
}

int main(int argc, char** argv) {
    char input[MAX_INPUT];
    fgets(input, MAX_INPUT, stdin);

    int token_count;
    char **tokens = tokenize(input, &token_count);

    for(int i = 0; i < token_count; i++) {
        printf("%s\n", tokens[i]);
        free(tokens[i]);
    }

    free(tokens);
    return 0;
}

