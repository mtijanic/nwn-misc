// Released under WTFPL-2.0 license
//
// LTR manipulation tool:
//    - Generate random names from .ltr files like the game does
//    - Print .ltr file Markov chain tables in a human readable format
//    - Build a new .ltr file from a set of names
//
// About LTR files:
//  .ltr files are used by the GetRandomName() NWN function to generate names.
//  They are a simple Markov chain table of cumulative distribution function (CDF)
//  for any sequence of up to 3 characters.
//  Put simpler, for any sequence of up to 3 characters, we have the probability:
//    - That it appears at the start of the name
//    - That it appears in the middle of the name
//    - That it appears at the end of the name
//
// To compile, use any of:
//    make nwnltr
//    cc -o nwnltr nwnltr.c
//
#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "ctype.h"
#include "time.h"

#define HELP \
"NWN name generator tool\n" \
"Usage: nwnltr [OPTION] <LTRFILE>\n" \
"Options:\n" \
" -p, --print         Print Markov chain tables for <LTRFILE> in a human readable format\n" \
" -b, --build         Build Markov chain tables using words from stdin and store in <LTRFILE>\n" \
" -g, --generate=NUM  Generate NUM names from <LTRFILE> and print to stdout. NUM=100 by default\n" \
" -s, --seed=NUM      Set the RNG seed to NUM. time(NULL) by default\n"

struct cfg {
    int   build;
    int   print;
    int   generate;
    int   seed;
    char *ltrfile;
} cfg;

#define die(format, ...)                                \
    do {                                                \
        fprintf(stderr, format "\n", ##__VA_ARGS__);    \
        fflush(stderr);                                 \
        exit(~0);                                       \
    } while(0)

void parse_cmdline(int argc, char *argv[]) {
    if (argc < 3) {
        printf(HELP);
        exit(0);
    }

    for (int i = 1; i < argc - 1; i++) {
        cfg.print |= !strcmp(argv[i], "-p") || !strcmp(argv[i], "--print");
        cfg.build |= !strcmp(argv[i], "-b") || !strcmp(argv[i], "--build");

        sscanf(argv[i], "--seed=%d", &cfg.seed) || (!strcmp(argv[i], "-s") && sscanf(argv[i+1], "%d", &cfg.seed));

        if (sscanf(argv[i], "--generate=%d", &cfg.generate) != 1) {
            if (!strcmp(argv[i], "--generate"))
                cfg.generate = 100;
            else if (!strcmp(argv[i], "-g"))
                sscanf(argv[i+1], "%d", &cfg.generate) == 1 ? i++ : (cfg.generate = 100);
        }
    }

    cfg.ltrfile = argv[argc-1];
    if (!(cfg.print || cfg.build || cfg.generate)) {
        printf("Need at least one of -p, -b, -g\n" HELP);
        exit(0);
    }
}

// NOTE: Game does not support more than 28 letters.
// Files can have fewer (just alpha), but there is no point as the special ones
// can just be given a probability of 0 to achieve the same effect.
// Thus, the value 28 is hardcoded here, for ease of file IO, but can be
// overridden at compile time.
#ifndef NUM_LETTERS
#define NUM_LETTERS 28
#endif
static const char letters[] = "abcdefghijklmnopqrstuvwxyz'-";
struct ltr_header {
    char     magic[8];
    uint8_t  num_letters;
};
struct cdf {
    float start  [NUM_LETTERS];
    float middle [NUM_LETTERS];
    float end    [NUM_LETTERS];
};
struct ltrdata {
    struct cdf singles;
    struct cdf doubles[NUM_LETTERS];
    struct cdf triples[NUM_LETTERS][NUM_LETTERS];
};
struct ltrfile {
    struct ltr_header header;
    struct ltrdata data;
};

static float nrand() { return (float)rand() / RAND_MAX; }
static int idx(char letter) {
    if (letter == '\'') return 26;
    if (letter == '-')  return 27;
    if (letter >= 'a' && letter <= 'z') return letter - 'a';
    return -1;
}

void load_ltr(const char *filename, struct ltrfile *ltr) {
    FILE *f = fopen(filename, "rb");
    if (!f)
        die("Unable to open file %s", filename);

    if (fread(&ltr->header, 9, 1, f) != 1 || strncmp(ltr->header.magic, "LTR V1.0", 8))
        die("File %s has no valid LTR header", filename);

    if (ltr->header.num_letters != NUM_LETTERS)
        die("File built for %d letters, tool only supports %d.", ltr->header.num_letters, NUM_LETTERS);

    if (fread(&ltr->data, sizeof(ltr->data), 1, f) != 1)
        die("Unable to read the prob table from %s. Truncated file?", filename);

    fclose(f);
}

void build_ltr(const char *filename, struct ltrfile *ltr) {
    memset(ltr, 0, sizeof(*ltr));
    strncpy(ltr->header.magic, "LTR V1.0", 8);
    ltr->header.num_letters = NUM_LETTERS;

    char buf[256] = {0};
    int count = 0;
    int midcount = 0;
    while (scanf("%255s", buf) == 1) {
        char buf2[256] = {0};
        char *p = buf2, *q = buf2;
        for (char *r = buf; *r; r++) {
            if ((*r) == '#') // stop on # to allow comments
                break;
            *r = tolower(*r);
            if (idx(*r) == -1) {
                fprintf(stderr, "Invalid character %c (%02x) in name \"%s\". Skipping character.\n", *r, (uint8_t)*r, buf);
                fflush(stderr);
                continue;
            }
            *q++ = *r;
        }
        *q = '\0';

        if ((q - buf2) < 3) { // we need at least 3 characters in a name
            fprintf(stderr, "Name \"%s\" is too short. Skipping name.\n", buf2);
            fflush(stderr);
            continue;
        }

        q--; count++;

        ltr->data.singles.start[idx(p[0])]                       += 1.0;
        ltr->data.doubles[idx(p[0])].start[idx(p[1])]            += 1.0;
        ltr->data.triples[idx(p[0])][idx(p[1])].start[idx(p[2])] += 1.0;

        ltr->data.singles.end[idx(q[0])]                         += 1.0;
        ltr->data.doubles[idx(q[-1])].end[idx(q[0])]             += 1.0;
        ltr->data.triples[idx(q[-2])][idx(q[-1])].end[idx(q[0])] += 1.0;

        if ((q - p) == 2) continue; // No middle
        while (++p != q-2) {
            ltr->data.singles.middle[idx(p[0])]                       += 1.0;
            ltr->data.doubles[idx(p[0])].middle[idx(p[1])]            += 1.0;
            ltr->data.triples[idx(p[0])][idx(p[1])].middle[idx(p[2])] += 1.0;
            midcount++;
        }
    }

    float s = 0.0, m = 0.0, e = 0.0;
    for (int i = 0; i < ltr->header.num_letters; i++) {
        if (ltr->data.singles.start[i] > 0.0) {
            ltr->data.singles.start[i] /= (float)count;
            s = ltr->data.singles.start[i] += s;
        }
        if (ltr->data.singles.end[i] > 0.0) {
            ltr->data.singles.end[i] /= (float)count;
            e = ltr->data.singles.end[i] += e;
        }
        if (ltr->data.singles.middle[i] > 0.0) {
            ltr->data.singles.middle[i] /= (float)midcount;
            m = ltr->data.singles.middle[i] += m;
        }
    }
    s = m = e = 0.0;
    for (int i = 0; i < ltr->header.num_letters; i++) {
        for (int j = 0; j < ltr->header.num_letters; j++) {
            if (ltr->data.doubles[i].start[j] > 0.0) {
                ltr->data.doubles[i].start[j] /= (float)count;
                s = ltr->data.doubles[i].start[j] += s;
            }
            if (ltr->data.doubles[i].end[j] > 0.0) {
                ltr->data.doubles[i].end[j]    /= (float)count;
                e = ltr->data.doubles[i].end[j] += e;
            }
            if (ltr->data.doubles[i].middle[j] > 0.0) {
                ltr->data.doubles[i].middle[j] /= (float)midcount;
                m = ltr->data.doubles[i].middle[j] += m;
            }
        }
    }
    s = m = e = 0.0;
    for (int i = 0; i < ltr->header.num_letters; i++) {
        for (int j = 0; j < ltr->header.num_letters; j++) {
            for (int k = 0; k < ltr->header.num_letters; k++) {
                if (ltr->data.triples[i][j].start[k] > 0.0) {
                    ltr->data.triples[i][j].start[k] /= (float)count;
                    s = ltr->data.triples[i][j].start[k] += s;
                }
                if (ltr->data.triples[i][j].end[k] > 0.0) {
                    ltr->data.triples[i][j].end[k] /= (float)count;
                    e = ltr->data.triples[i][j].end[k] += e;
                }
                if (ltr->data.triples[i][j].middle[k] > 0.0) {
                    ltr->data.triples[i][j].middle[k] /= (float)midcount;
                    m = ltr->data.triples[i][j].middle[k] += m;
                }
            }
        }
    }

    FILE *f = fopen(filename, "wb");
    if (!f) die("Unable to create file %s", filename);
    fwrite(&ltr->header, 9, 1, f);
    fwrite(&ltr->data, sizeof(ltr->data), 1, f);
    fclose(f);
}

void print_ltr(struct ltrfile *ltr) {
    printf("Num letters: %d\n", ltr->header.num_letters);
    printf("Sequence | CDF(start)  P(start) | CDF(middle)  P(middle) | CDF(end)  P(end)\n");

    float s = 0.0, m = 0.0, e = 0.0;
    for (int i = 0; i < ltr->header.num_letters; i++) {
        struct cdf *p = &ltr->data.singles;
        printf("%c        |% .5f    % .5f  |% .5f     % .5f   |% .5f  % .5f\n", letters[i],
                p->start[i],  p->start[i]  == 0.0 ? 0.0 : p->start[i]  - s,
                p->middle[i], p->middle[i] == 0.0 ? 0.0 : p->middle[i] - m,
                p->end[i],    p->end[i]    == 0.0 ? 0.0 : p->end[i]    - e);

        if (p->start[i]  > 0.0) s = p->start[i];
        if (p->middle[i] > 0.0) m = p->middle[i];
        if (p->end[i]    > 0.0) e = p->end[i];
    }

    for (int i = 0; i < ltr->header.num_letters; i++) {
        s = m = e = 0.0;
        for (int j = 0; j < ltr->header.num_letters; j++) {
            struct cdf *p = &ltr->data.doubles[i];
            printf("%c%c       |% .5f    % .5f  |% .5f     % .5f   |% .5f  % .5f\n", letters[i], letters[j],
                    p->start[j],  p->start[j]  == 0.0 ? 0.0 : p->start[j]  - s,
                    p->middle[j], p->middle[j] == 0.0 ? 0.0 : p->middle[j] - m,
                    p->end[j],    p->end[j]    == 0.0 ? 0.0 : p->end[j]    - e);

            if (p->start[j]  > 0.0) s = p->start[j];
            if (p->middle[j] > 0.0) m = p->middle[j];
            if (p->end[j]    > 0.0) e = p->end[j];
        }
    }

    for (int i = 0; i < ltr->header.num_letters; i++) {
        for (int j = 0; j < ltr->header.num_letters; j++) {
            s = m = e = 0.0;
            for (int k = 0; k < ltr->header.num_letters; k++) {
                struct cdf *p = &ltr->data.triples[i][j];
                printf("%c%c%c      |% .5f    % .5f  |% .5f     % .5f   |% .5f  % .5f\n", letters[i], letters[j], letters[k],
                        p->start[k],  p->start[k]  == 0.0 ? 0.0 : p->start[k]  - s,
                        p->middle[k], p->middle[k] == 0.0 ? 0.0 : p->middle[k] - m,
                        p->end[k],    p->end[k]    == 0.0 ? 0.0 : p->end[k]    - e);

                if (p->start[k]  > 0.0) s = p->start[k];
                if (p->middle[k] > 0.0) m = p->middle[k];
                if (p->end[k]    > 0.0) e = p->end[k];
            }
        }
    }
}

const char *random_name(struct ltrfile *ltr) {
    static char namebuf[256];
    int attempts;
    char *p;
    float prob;
    int i;

again:
    attempts = 0;
    p = &namebuf[0];

    for (i = 0, prob = nrand(); i < ltr->header.num_letters; i++)
        if (prob < ltr->data.singles.start[i])
            break;
    // This can happen if the training set was too small
    if (i == ltr->header.num_letters)
        goto again;
    *p++ = letters[i];

    for (i = 0, prob = nrand(); i < ltr->header.num_letters; i++)
        if (prob < ltr->data.doubles[idx(p[-1])].start[i])
            break;
    if (i == ltr->header.num_letters)
        goto again;
    *p++ = letters[i];

    for (i = 0, prob = nrand(); i < ltr->header.num_letters; i++)
        if (prob < ltr->data.triples[idx(p[-2])][idx(p[-1])].start[i])
            break;
    if (i == ltr->header.num_letters)
        goto again;
    *p++ = letters[i];

    while (1) {
        // Arbitrary end threshold form the core game
        if ((rand() % 12) < (p - namebuf)) {
            for (i = 0, prob = nrand(); i < ltr->header.num_letters; i++) {
                if (prob < ltr->data.triples[idx(p[-2])][idx(p[-1])].end[i]) {
                    *p++ = letters[i]; *p = '\0';
                    namebuf[0] = toupper(namebuf[0]);
                    return namebuf;
                }
            }
        }

        for (i = 0, prob = nrand(); i < ltr->header.num_letters; i++) {
            if (prob < ltr->data.triples[idx(p[-2])][idx(p[-1])].middle[i]) {
                *p++ = letters[i];
                break;
            }
        }

        if (i == ltr->header.num_letters) {
            if (--p - namebuf < 3 || ++attempts > 100)
                goto again;
        }
    }
}

int main(int argc, char *argv[]) {
    struct ltrfile ltr;
    parse_cmdline(argc, argv);

    srand(cfg.seed ? cfg.seed : time(NULL));

    if (cfg.build)
        build_ltr(cfg.ltrfile, &ltr);
    else
        load_ltr(cfg.ltrfile, &ltr);

    if (cfg.print)
        print_ltr(&ltr);

    while (cfg.generate-- > 0)
        printf("%s\n", random_name(&ltr));

    return 0;
}
