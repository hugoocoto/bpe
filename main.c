#include <ctype.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>

#define DEFAULT_TOKENS_SIZE 128

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

/* If lhs and rhs are equal, the token is the char with number rhs. If they're
 * not equal, the token represents the union of lhs and rhs. */
typedef struct Token {
        size_t lhs, rhs;
} Token;

// Count map : get the count of a given token index
typedef struct {
        size_t key;   // token arr index
        size_t value; // token count
} CMap;
CMap *countmap = NULL;

// Index map : get the index of a token by itself
typedef struct {
        Token key;    // token
        size_t value; // arr index
} IMap;
IMap *indexmap = NULL;

// array of tokens
Token *tokens = NULL;

// array of indexes that represents the text
size_t *toktext = NULL;

void print_tokenlit(FILE *f, Token t);


int
predict(size_t tok)
{
        size_t i;
        size_t len = arrlen(tokens);
        Token *matches = NULL;
        size_t total_prob = 0;

        for (i = 0; i < len; i++) {
                if (tokens[i].lhs == tok) {
                        arrput(matches, tokens[i]);
                        total_prob += hmget(countmap, i) + 1;
                }
        }

        if (total_prob == 0) {
                arrfree(matches);
                return 2;
        }

        size_t index = rand() % arrlen(matches);

        // TODO: chose by probability

        if (matches[index].lhs == matches[index].rhs) {
                arrfree(matches);
                return 0;
        }

        if (predict(hmget(indexmap, matches[index]))) {
                if (predict(matches[index].rhs)) {
                        print_tokenlit(stdout, matches[index]);
                        arrfree(matches);
                        return 1;
                }
        }
        arrfree(matches);
        return 0;
}

void
parse(FILE *file)
{
        size_t rn;
        char buf[1024];
        size_t i;
        size_t c;
        wchar_t wc;
        while ((wc = fgetwc(file)) != WEOF) {
                if (wc < DEFAULT_TOKENS_SIZE) {
                        i = wc;
                } else {
                        // Add char to tokens if no ascii one
                        if (!hmgetp_null(indexmap, ((Token) { wc, wc }))) {
                                size_t tokindex = arrlen(tokens);
                                arrput(tokens, ((Token) { wc, wc }));
                                hmput(indexmap, ((Token) { wc, wc }), tokindex);
                                hmput(countmap, tokindex, 0);
                                i = tokindex;
                        } else {
                                i = hmget(indexmap, ((Token) { wc, wc }));
                        }
                }
                arrput(toktext, i);
                c = hmget(countmap, i) + 1;
                hmput(countmap, i, c);
        }
}

void
print_tokenlit(FILE *f, Token t)
{
        if (t.lhs == t.rhs) {
                putwc((wchar_t) t.lhs, f);
                return;
        }
        print_tokenlit(f, tokens[t.lhs]);
        print_tokenlit(f, tokens[t.rhs]);
}


void
print_pair(size_t lhs, size_t rhs)
{
        if (lhs < 256 && isprint(lhs))
                printf("'%c' ", (char) lhs);
        else
                printf("%3zu ", lhs);

        if (rhs < 256 && isprint(rhs))
                printf("'%c' ", (char) rhs);
        else
                printf("%3zu ", rhs);
}

void
print_toktext()
{
        size_t len = arrlen(toktext);
        size_t i;
        for (i = 0; i < len; i++) {
                print_pair(tokens[toktext[i]].lhs, tokens[toktext[i]].rhs);
                printf(" -> '");
                print_tokenlit(stdout, tokens[toktext[i]]);
                puts("'");
        }
}

void
print_valid_pairs(CMap *map)
{
        size_t len = hmlen(map);
        size_t i;
        printf("Valid pairs\n");
        for (i = 0; i < len; i++) {
                if (map[i].value > 1) {
                        print_pair(tokens[map[i].key].lhs,
                                   tokens[map[i].key].rhs);
                        printf(" : %zu\n", map[i].value);
                }
        }
        printf("\n");
}

int
redux()
{
        struct {
                Token key;
                size_t value;
        } *tmpcount = NULL;
        size_t len = arrlen(toktext);
        size_t i;
        size_t c;
        hmdefault(tmpcount, 0);
        for (i = 1; i < len; i++) {
                c = hmget(tmpcount, ((Token) { toktext[i - 1], toktext[i] }));
                hmput(tmpcount, ((Token) { toktext[i - 1], toktext[i] }), c + 1);
        }

        len = hmlen(tmpcount);
        ssize_t max = -1;
        for (i = 0; i < len; i++) {
                if (tmpcount[i].value > 1 &&
                    (max < 0 || tmpcount[i].value > tmpcount[max].value)) {
                        max = i;
                }
        }

        if (max < 0) {
                // print_toktext();
                hmfree(tmpcount);
                return 0;
        }

        // print_toktext();
        printf("Join: ");
        print_pair(tmpcount[max].key.lhs, tmpcount[max].key.rhs);
        printf(" : %zu\n", tmpcount[max].value);

        // add new token
        size_t mlhs = tmpcount[max].key.lhs;
        size_t mrhs = tmpcount[max].key.rhs;
        size_t tokindex = arrlen(tokens);
        arrput(tokens, ((Token) { mlhs, mrhs }));
        hmput(indexmap, ((Token) { mlhs, mrhs }), tokindex);
        hmput(countmap, tokindex, 0);

        for (i = 1; i < arrlen(toktext); i++) {
                if (toktext[i - 1] == mlhs && toktext[i] == mrhs) {
                        countmap[toktext[i - 1]].value--;
                        countmap[toktext[i]].value--;
                        countmap[tokindex].value++;
                        toktext[i - 1] = tokindex;
                        arrdel(toktext, i);
                        i--;
                }
        }

        hmfree(tmpcount);
        return 1;
}

void
print_tokens(FILE *f)
{
        size_t len = arrlen(tokens);
        size_t i;
        size_t count;
        fprintf(f, "Tokens (%zu): \n", len);
        for (i = 0; i < len; i++) {
                count = hmget(countmap, i);
                if (count == 0) continue;
                fprintf(f, "[%zu] : %zu -> '", i, count);
                print_tokenlit(f, tokens[i]);
                fprintf(f, "'\n");
        }
}

void
fill_default_tokens()
{
        int i;
        for (i = 0; i < DEFAULT_TOKENS_SIZE; i++) {
                arrput(tokens, ((Token) { i, i }));
                hmput(countmap, i, 0);
                hmput(indexmap, ((Token) { i, i }), i);
        }
}

void
print_tokcount()
{
        size_t len = hmlen(countmap);
        size_t i;
        for (i = 0; i < len; i++) {
                Token t = tokens[countmap[i].key];
                size_t count = countmap[i].value;
                if (count > 0) {
                        print_pair(t.lhs, t.rhs);
                        printf(" : %zu\n", count);
                }
        }
}

void
dump_tokens(FILE *f)
{
}

void
load_tokens(FILE *f)
{
}

int
main(int argc, char *argv[])
{
        if (argc < 2) {
                printf("Usage: %s WORDS_FILE\n", argv[0]);
                return 1;
        }

        srand(time(NULL));
        setlocale(LC_ALL, "");

        FILE *file = fopen(argv[1], "r");
        if (!file) {
                printf("Cannot read file %s\n", argv[1]);
                return 2;
        }

        fill_default_tokens();
        parse(file);

        do {
        } while (redux());

        // print_toktext();

        FILE *fout;

        fout = fopen("tokens", "w");
        if (fout) {
                print_tokens(fout);
                fclose(fout);
        }

        fout = fopen("tokens.dump", "w");
        if (fout) {
                dump_tokens(fout);
                fclose(fout);
        }

        char c = 'e';
        printf("Predict phrases by '%c'\n", c);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);
        predict(c);
        putchar(10);

        // free as in freedom
        arrfree(toktext);
        arrfree(tokens);
        hmfree(indexmap);
        hmfree(countmap);

        fclose(file);

        return 0;
}
