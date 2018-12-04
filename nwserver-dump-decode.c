// Released under WTFPL-2.0 license
//
// nwserver stacktrace decoding tool:
//    - Decode a nwserver (linux or windows) stack trace to human readable symbols
//
// To compile, use any of:
//    make nwserver-dump-decode
//    cc -o nwserver-dump-decode nwserver-dump-decode.c
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define HELP \
"NWN nwserver stacktrace decoding tool\n" \
"Usage: nwserver-dump-decode [OPTIONS]\n" \
"Options:\n" \
" -h, --help          Print this help command\n" \
" -d, --dumpfile      Path to the dump file to decode. Defaults to stdin if not specified\n" \
" -f, --funcfile      Path to the functions.hpp file. Will attempt to auto detect if not specified\n" \
" -r, --repeat-input  Will print all non-decoded input lines over to output.\n" \
" -a, --autodetect    Try to automatically detect the <FUNCTIONS_FILE>\n" \
"\n" \
"Example usages:\n" \
"  Decode a crash dump with autodetcting the offsets:\n" \
"    nwserver-dump-decode -r -a < nwserver-crash-1543867203.log\n" \
"  Decode a crash dump with manually specifying the offsets:\n" \
"    nwserver-dump-decode -r -d nwserver-crash-1543867203.log -f ~/nwnx/NWNXLib/API/FunctionsLinux.hpp\n"


#define die(format, ...)                                \
    do {                                                \
        fprintf(stderr, format "\n", ##__VA_ARGS__);    \
        exit(~0);                                       \
    } while(0)

struct args {
    int   repeat;
    int   autodetect;
    char *dumpfile;
    char *funcfile;
} args;

void parse_cmdline(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf(HELP);
            exit(0);
        }

        args.repeat |= !strcmp(argv[i], "-r") || !strcmp(argv[i], "--repeat-input");
        args.autodetect |= !strcmp(argv[i], "-a") || !strcmp(argv[i], "--autodetect");

        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--dumpfile")) {
            if (i == argc-1)
                die("Bad argument - Need file name with -d / --dumpfile");
            args.dumpfile = argv[++i];
        }
        if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--funcfile")) {
            if (i == argc-1)
                die("Bad argument - Need file name with -f / --funcfile");
            args.funcfile = argv[++i];
        }
    }

    if (args.autodetect && args.funcfile)
        die("Bad arguments: --autodetect and --funcfile are mutually exclusive");
    else if (!args.funcfile)
        args.autodetect = 1;
}

#define MAX_FUNCTIONS 10000
struct function {
    char name[256];
    uint32_t offset;
} *functions;
uint32_t fcount;

int cmp(const void *a, const void *b) {
    const struct function *f1 = a, *f2 = b;
    return (int64_t)f1->offset - (int64_t)f2->offset;
}

void load_functions(const char *infile) {
    FILE *f = fopen(infile, "r");
    if (!f)
        die("Input file '%s' not found", infile);

    functions = malloc(MAX_FUNCTIONS * sizeof(*functions));
    if (!functions)
         die("Out of memory");

    char buf[1024];
    while (fgets(buf, 1024, f))
        fcount += (sscanf(buf, "constexpr%*[ \t]uintptr_t%*[ \t]%s%*[ \t]=%*[ \t]%x;", functions[fcount].name, &functions[fcount].offset) == 2);

    fclose(f);

    qsort(functions, fcount, sizeof(*functions), cmp);
}

uint32_t lookup(uint32_t offset) {
    for (uint32_t i = 1; i < fcount; i++) {
        if (functions[i].offset > offset) {
            return i-1;
        }
    }
    return ~0;
}
char *decode(uint32_t offset) {
    static char out[1024];
    uint32_t idx = lookup(offset);
    if (idx != ~0) {
        sprintf(out, "%s+0x%x", functions[idx].name, offset - functions[idx].offset);
        return out;
    }
    return NULL;
}

char *try_parse(char *buf) {
    static char out[1024];
    char tmp[1024] = "";
    uint32_t offset = 0;

    if (sscanf(buf, "%X", &offset)) {
        return decode(offset);
    } else if (sscanf(buf, "./nwserver-linux(+0x%x)%[^\n]", &offset, tmp)) {
        char *dec = decode(offset);
        if (dec) {
            sprintf(out, "./nwserver-linux(%s)%s", dec, tmp);
            return out;
        }
    }
    return NULL;
}

#define starts_with(str1, str2) (!strncmp(str1, str2, strlen(str2)))

char *detect_functions_file(int build, int os) {
    static char out[1024];
    static const char *paths[] = {
        "extra/offsets",
        "../extra/offsets",
        "../../extra/offsets",
        "offsets",
        "."
    };
    static const char *filenames[] = { "FunctionsLinux", "FunctionsWindows" };
    for (uint32_t i = 0; i < (sizeof(paths)/sizeof(paths[0])); i++) {
        sprintf(out, "%s/%s-%4d.hpp", paths[i], filenames[os], build);
        FILE *f = fopen(out, "r");
        if (f) {
            fclose(f);
            return out;
        }
    }
    // Try to detect nwnx and use the current one..
    static const char *nwnxpaths[] = {
        "~/nwnx",
        "~/unified",
        "~/nwnx/unified",
        "~/nwn/nwnx",
        "~/nwn/unified",
        "~/nwn/nwnx/unified",
        "../nwnx",
        "../unified",
        "../nwnx/unified",
        "../nwn/nwnx",
        "../nwn/unified",
        "../nwn/nwnx/unified",
        "../../nwnx",
        "../../unified",
        "../../nwnx/unified",
        "../../nwn/nwnx",
        "../../nwn/unified",
        "../../nwn/nwnx/unified",
        "."
    };
    for (uint32_t i = 0; i < (sizeof(nwnxpaths)/sizeof(nwnxpaths[0])); i++) {
        sprintf(out, "%s/NWNXLib/API/%s.hpp", nwnxpaths[i], filenames[os]);
        FILE *f = fopen(out, "r");
        if (f) {
            char buf[1024];
            int nwnxbuild = 0;
            while (fgets(buf, 1024, f)) {
                if (sscanf(buf, "NWNX_EXPECT_VERSION(%d);", &nwnxbuild))
                    break;
            }
            fclose(f);
            if (nwnxbuild != build)
                die("Autodetect found NWNX at build %d, but need build %d", nwnxbuild, build);

            return out;
        }
    }
    die("Autodetect of functions file failed");
}

int main(int argc, char *argv[])
{
    parse_cmdline(argc, argv);
    if (!args.autodetect)
        load_functions(args.funcfile);

    FILE *in = args.dumpfile ? fopen(args.dumpfile, "r") : stdin;
    if (!in)
        die("Unable to open input file '%s'", args.dumpfile);

    char buf[1024];
    int skip = 0;
    int windows = 1;
    int build;

    while (fgets(buf, 1024, in)) {
        if (starts_with(buf, "=== ")) {
            skip = !starts_with(buf, "=== Backtrace");
        }

        if (args.autodetect) {
            sscanf(buf, "g_sBuildNumber = %d", &build);
            if (starts_with(buf, "&GenericCrashHandler")) {
                if (starts_with(buf, "&GenericCrashHandler = 0x"))
                    windows = 0;
                load_functions(detect_functions_file(build, windows));
            }
        }

        char *parse = try_parse(buf);
        if (parse && !skip) {
            printf("%s\n", parse);
        } else if (args.repeat) {
            printf("%s", buf);
        }
    }

    return 0;
}

