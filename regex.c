/*
 * 	NAME
 *    regex - janet wrappers for POSIX regex routines
 *
 * 	SYNOPSIS
 *    cfun_compile - return compiled regex pattern or nil
 *    cfun_match   - return Janet array of strings matching pattern
 *    cfun_replace - returns Janet buffer, replacing matched string
 *
 * 	DESCRIPTION
 *
 *
 * 	NOTES
 *
 *
 * 	MODIFICATION HISTORY
 * 	Mnemonic	Rel	Date	    Who
 * 	regex       1.0 20230416    MPW
 * 		Written.
 *
 */

#include <janet.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

enum {
    RE_NMATCH = 10,
    RE_ERRSIZE = 128
};


typedef struct {
    regex_t* regex;
} Pattern;

/* forward decls */
static void regex_tostring(void*, JanetBuffer*);
static int regex_gc(void*, size_t);

static const JanetAbstractType regex_type = {
  .name = "regex/pattern",
  .gc = regex_gc,
  .gcmark = NULL,
  .get = NULL,
  .put = NULL,
  .marshal = NULL,
  .unmarshal = NULL,
  .tostring = regex_tostring,
  .compare = NULL,
  .hash = NULL,
  .next = NULL,
  .call = NULL,
  .length = NULL,
  .bytes = NULL,
};

/* Does gc also need to explicitly free Pattern?
 * Other examples I've looked at don't do that.  */
static int
regex_gc(void *data, size_t len) {
  (void) len;
  Pattern* p = (Pattern *) data;
  if (p->regex) regfree(p->regex);
  return 0;
}

static void
regex_tostring(void* data, JanetBuffer* buffer) {
    char str[32];
    sprintf(str, "%lx", (uint64_t) data);
    janet_buffer_push_cstring(buffer, str);
}

static void
regex_error(int errcode, regex_t* re) {
    char err[RE_ERRSIZE];
    regerror(errcode, re, err, RE_ERRSIZE);
    janet_panic(err);
    return;
}

static Janet
cfun_compile(int32_t argc, Janet* argv) {
    regex_t* regex_buf;
    Pattern* p;
    const char* re;
    int status = 0;

    janet_fixarity(argc, 1);
    if (janet_checktype(argv[0], JANET_BUFFER)) {
        JanetBuffer* b = janet_getbuffer(argv, 0);
        re = (const char *) b->data;
    }
    else if (janet_checktype(argv[0], JANET_STRING)) {
        re = janet_getcstring(argv, 0);
    }
    else {
        janet_panic("arg 0: expected buffer or string");
    }

    p = (Pattern *) janet_abstract(&regex_type, sizeof(Pattern));
    regex_buf = (regex_t *) malloc(sizeof(regex_t));
    p->regex = regex_buf;
    status = regcomp(regex_buf, re, REG_EXTENDED);
    if (status) regex_error(status, regex_buf);
    return janet_wrap_abstract(p);
}

/* Return Janet string, converted from C string, as referenced in pm. */
Janet
janet_str(const char* string, regmatch_t* pm) {
    uint32_t len = pm->rm_eo - pm->rm_so;
    uint8_t *newbuf = janet_string_begin(len);
    memcpy(newbuf, (const uint8_t *) string + pm->rm_so, len);
    return janet_wrap_string(newbuf);
}

/* Validate adn return arguments for match and replace */
static void
validate_args(int32_t argc, Janet* argv, Pattern** p, const char** text,
              const char** replace) {
    if (!janet_checktype(argv[0], JANET_ABSTRACT)) {
        janet_panic("arg 0: regex expected");
    }

    *p = (Pattern *) janet_getabstract(argv, 0, &regex_type);
    if (janet_checktype(argv[1], JANET_BUFFER)) {
        JanetBuffer* b = janet_getbuffer(argv, 1);
        *text = (const char *) b->data;
    }
    else if (janet_checktype(argv[1], JANET_STRING)) {
        *text = janet_getcstring(argv, 1);
    }
    else {
        janet_panic("arg 1: expected buffer or string");
    }
    if (argc >= 3 && replace) {
        if (janet_checktype(argv[2], JANET_BUFFER)) {
            JanetBuffer* b = janet_getbuffer(argv, 2);
            *replace = (const char *) b->data;
        }
        else if (janet_checktype(argv[2], JANET_STRING)) {
            *replace = janet_getcstring(argv, 2);
        }
        else {
            janet_panic("arg 2: expected buffer or string");
        }
    }
    return;
}

static Janet
cfun_match(int32_t argc, Janet* argv) {
    regmatch_t pmatch[RE_NMATCH];
    Pattern* p = NULL;
    const char* string;
    int result;

    janet_fixarity(argc, 2);
    validate_args(argc, argv, &p, &string, NULL);
    result = regexec(p->regex, string, RE_NMATCH, pmatch, 0);
    if (result == 0) {
        /* match found, return contents of pmatch as Janet array of
         * strings */
        JanetArray* ja = janet_array(RE_NMATCH);
        for (regmatch_t* pm = pmatch; pm < pmatch+RE_NMATCH; pm++) {
            if (pm->rm_so > -1) {
                janet_array_push(ja, janet_str(string, pm));
            }
            else {
                break;
            }
        }
        return janet_wrap_array(ja);
    }
    else if (result != REG_NOMATCH) {
        regex_error(result, p->regex);
    }
    return janet_wrap_nil();
}

#define ESC '%'

/* Replace matched pattern in string s, with replacement string r. If
 * r contains backreferences to captures (introduced by the ESC
 * character), these are inserted into the replaced string. All text
 * is pushed into JanetBuffer* b. Returns b.
 */
static JanetBuffer*
process_rep(JanetBuffer* b, const char* s, const char* r, regmatch_t* pmatch) {
    const char* c = r;

    while (c) {
        c = strchr(r, ESC);
        if (c) {
            uint8_t nc = *(c + 1);
            if (nc >= '0' && nc <= '9') {
                janet_buffer_push_bytes(b, (const uint8_t *) r, c-r);
                /* insert capture */
                int mno = nc - '0';
                regmatch_t* pm = pmatch + mno;
                if (pm->rm_so >= 0) {
                    janet_buffer_push_bytes(b, (const uint8_t *) s + pm->rm_so,
                                            pm->rm_eo - pm->rm_so);
                }
                r = c + 2;
            }
            else if (nc == ESC) {
                janet_buffer_push_bytes(b, (const uint8_t *) r, c-r+1);
                r = c + 2;
            }
            else {
                janet_buffer_push_bytes(b, (const uint8_t *) r, c-r+1);
                r = c + 1;
            }
        }
        else {
            janet_buffer_push_cstring(b, r);
        }
    }
    return b;
}

static JanetBuffer*
replace_match(JanetBuffer* b, const char*  string, regmatch_t* pmatch,
       const char* replace) {

    if (pmatch[0].rm_so != 0) {
        janet_buffer_push_bytes(b, (const uint8_t *) string, pmatch[0].rm_so);
    }
    return process_rep(b, string, replace, pmatch);
}

static Janet
cfun_replace(int32_t argc, Janet* argv) {
    regmatch_t pmatch[RE_NMATCH];
    Pattern* p = NULL;
    const char* string, *replace;
    JanetBuffer* b = NULL;
    int result;
    const uint8_t* all;

    janet_arity(argc, 3, 4);
    validate_args(argc, argv, &p, &string, &replace);
    all = janet_optkeyword(argv, argc, 3, (const uint8_t *) NULL);
    do {
        result = regexec(p->regex, string, RE_NMATCH, pmatch, 0);
        if (result == 0) {
            if (!b) b = janet_buffer(strlen(string));
            replace_match(b, string, pmatch, replace);
            string = string + pmatch[0].rm_eo;
        }
        else if (result != REG_NOMATCH) {
            regex_error(result, p->regex);
        }
    } while (result == 0 && all);

    if (b) {
        janet_buffer_push_cstring(b, string);
        return janet_wrap_buffer(b);
    }
    return janet_wrap_nil();
}

static JanetReg
cfuns[] = {
    {"compile", cfun_compile,
     "(regex/compile RE-string)\nReturns a compiled POSIX regular expression."},
    {"match", cfun_match,
     "(regex/match RE text)\nMatches a compiled regular expression in "
     "text, a string or buffer. Returns nil if no matches found, otherwise "
     "an array of matched strings."},
    {"replace", cfun_replace,
     "(regex/replace RE text rep &opt :all)\n"
     "Replace matched regular expression in text, a string or buffer, with "
     "rep. rep may contain references to captured strings, introduced by the % "
     "character. %0 references the entire matched string. %1 through %9 reference "
     "strings captured by () constructs. If :all is specified, replaces all "
     "matched strings, else just the first is replaced. Returns nil if no "
     "replacements made, otherwise new buffer with replacements."},
    {NULL, NULL, NULL}
};

JANET_MODULE_ENTRY(JanetTable* env) {
    janet_cfuns(env, "regex", cfuns);
}
