/*
How to use
#include "json.h"

In main file define the following to include implementation for both json
parsing and arena functions: #define JSON_IMPLEMENTATION #define
ARENA_IMPLEMENTATION
*/
#ifndef JSON_H
#define JSON_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_ERROR_STR_SIZE 500

#include "arena.h"

enum Node_Kind {
    JSON_NONE,
    JSON_OBJ,
    JSON_ARRAY,
    JSON_INTEGER,
    JSON_REAL,
    JSON_STRING,
    JSON_BOOL,
    JSON_NULL
};

typedef struct {
    const char *start;
    size_t len;
} String;

typedef struct Json_Value Json_Value;

typedef struct Json_Arr_Data {
    Json_Value *items;
    size_t size;
    size_t capacity;
} Json_Arr_Data;

union Json_Data {
    Json_Arr_Data arrval;
    struct Json *objval;
    long intval;
    double realval;
    String stringval;
    bool boolval;
};

struct Json_Value {
    union Json_Data data;
    enum Node_Kind data_kind;
};

typedef struct Json {
    Json_Value json_data;
    String key;
    const char *input_file;

    struct Json *next;
    Arena *arena;  // shared for lexer, just 1 arena for lexer json tree etc
    bool is_toplevel;
} Json;

typedef struct {
    const char *source;
    int line;
    int column;
    size_t byte_offset;
    const char *subsystem;  // "parse" or "dump"
    char message[JSON_ERROR_STR_SIZE];
} Json_Error;

// TODO: merge json and json_parse_file
// TODO: possibly add versions json_parse_string_ref(references user memory, but
// would still need to alloc more for say escaped string as cannot modify
// user-data
// or add json_parse_string_insitu, free to modify user-data

// Parses a null-terminated JSON string. The returned Json owns its parsed data;
// the input may be freed after this call. Free with json_free().
Json *json_parse_string(const char *file_content, Json_Error *err);
// Parses a file (if exists). The returned Json owns its parsed data;
// Free with json_free().
Json *json_parse_file(const char *file_name, Json_Error *err);

// TODO:add some form of minifier (here or utils?)
// FIX:should this escape strings as well?
char *json_stringify(Json *json, bool minified, Json_Error *err);

void json_dump(Json *json, FILE *f, bool minified, Json_Error *err);

void json_free(Json *json);

// Traverse, query
enum Node_Kind json_kind(const Json *node);

bool json_is(const Json *node, enum Node_Kind kind);

bool json_is_integer(const Json *node);
bool json_is_real(const Json *node);
bool json_is_number(const Json *node);
bool json_is_string(const Json *node);
bool json_is_bool(const Json *node);
bool json_is_null(const Json *node);
bool json_is_obj(const Json *node);
bool json_is_array(const Json *node);

const char *json_key(const Json *member);

const Json *json_first(const Json *obj);

const Json *json_next(const Json *member);

const Json *json_value_find(const Json_Value *obj, const char *key);

// TODO: find by case sensitive and insensitive
// TODO: key can be escaped as well
// {"a\"b": 1}
// if you search: json_find(obj, "a\"b")
const Json *json_find(const Json *obj, const char *key);

long json_integer(const Json *node);

double json_real(const Json *node);

double json_number(const Json *node);

String json_string_view(const Json *node);

char *json_cstring(const Json *node);

bool json_bool(const Json *node);

size_t json_array_size(const Json *arr);

// TODO: it returns Json_value so you have to manually do ->data.realval etc
// convert Json_Value to json?
const Json_Value *json_array_at(const Json *arr, size_t i);

#endif  // JSON_H

#ifdef JSON_IMPLEMENTATION
#include <errno.h>
#include <limits.h>
#include <math.h>

enum Token_Kind {
    TOKEN_OPENCURLY,
    TOKEN_CLOSECURLY,
    TOKEN_OPENSQUARE,
    TOKEN_CLOSESQUARE,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_INTEGER,
    TOKEN_REAL,
    TOKEN_STRING,
    TOKEN_BOOL,
    TOKEN_NULL,
    TOKEN_END,
    TOKEN_UNKNOWN
};

typedef struct Lexer {
    Json_Value json_data;
    Arena *arena;
    const char *start;
    const char *path;
    Json_Error *err;

    const char *curr;
    const char *end;

    enum Token_Kind kind;

    int line_number;
    int line_offset;
} Lexer;

static inline void json__clear_error(Json_Error *err, const char *source,
                                     const char *subsystem) {
    if (err == NULL) return;
    err->source = source != NULL ? source : "";
    err->line = 0;
    err->column = 0;
    err->byte_offset = 0;
    err->subsystem = subsystem != NULL ? subsystem : "";
    err->message[0] = '\0';
}

static inline void json__set_error_impl(Json_Error *err, const char *source,
                                        int line, int column,
                                        size_t byte_offset,
                                        const char *subsystem,
                                        const char *format, ...) {
    if (err == NULL) return;
    err->source = source != NULL ? source : "";
    err->line = line;
    err->column = column;
    err->byte_offset = byte_offset;
    err->subsystem = subsystem != NULL ? subsystem : "";

    va_list args;
    va_start(args, format);
    vsnprintf(err->message, JSON_ERROR_STR_SIZE, format, args);
    va_end(args);
    err->message[JSON_ERROR_STR_SIZE - 1] = '\0';
}

#define json__set_error_raw(err, msg) \
    json__set_error_impl((err), "", 0, 0, 0, "", "%s", (msg))

#define json__set_error(err, format, ...) \
    json__set_error_impl((err), "", 0, 0, 0, "", (format), __VA_ARGS__)

// FIX: currently all error reporting points(line_offset) to end of token not
// start
#define json__set_lerror_raw(l, str)                                         \
    json__set_error_impl((l)->err, (l)->path, (l)->line_number,              \
                         (l)->line_offset, (size_t)((l)->curr - (l)->start), \
                         "parse", "%s", (str))

#define json__set_lerror(l, format, ...)                                     \
    json__set_error_impl((l)->err, (l)->path, (l)->line_number,              \
                         (l)->line_offset, (size_t)((l)->curr - (l)->start), \
                         "parse", (format), __VA_ARGS__)

// TODO: should it malloc and not arena_alloc?
// static char *json__arena_string_to_charp(Arena *arena, String data) {
//    if (data.start == NULL) return NULL;
//    // char *buf = arena_alloc_array(arena, char, data.len + 1);
//    char *buf = malloc(sizeof(char) * (data.len + 1));
//    if (buf == NULL) return NULL;
//    memcpy(buf, data.start, data.len);
//    buf[data.len] = '\0';
//    return buf;
//}

static char *json__arena_string_to_charp(Arena *arena, String data) {
    if (data.start == NULL) return NULL;
    char *buf = arena_alloc_array(arena, char, data.len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, data.start, data.len);
    buf[data.len] = '\0';
    return buf;
}

static inline char json__lexer_peek_char(Lexer *l) {  // peeks next char
    if (l->curr >= (l->end)) {
        return '\x03';
    }
    return *(l->curr + 1);
}

static char json__lexer_get_char(
    Lexer *l) {  // returns current char and move next
    if (l->curr >= (l->end)) {
        return '\x03';
    }
    char curr = *l->curr;
    if (curr == '\r') {
        if (json__lexer_peek_char(l) == '\n') {
            l->curr++;
        }
        curr = '\n';
        l->line_number++;
        l->line_offset = 1;
    } else if (curr == '\n') {
        l->line_number++;
        l->line_offset = 1;
    } else {
        l->line_offset++;
    }
    l->curr++;

    return curr;
}

// just compare if next char match input
static bool json__lexer_expect_bytes(Lexer *l, const char *input,
                                     size_t strlen) {
    size_t len = 0;
    while (len < strlen) {
        if ((l->curr + len) >= (l->end) || *(l->curr + len) != input[len]) {
            return false;
        }
        len++;
    }
    return true;
}

// compare if next char match input, if so increment curr
static bool json__lexer_get_expect_bytes(Lexer *l, const char *input,
                                         size_t strlen) {
    if (json__lexer_expect_bytes(l, input, strlen)) {
        l->curr += strlen;
        l->line_offset += strlen;
        return true;
    }
    return false;
}

// TODO:maybe should return bool? to express end of parsing and report error
static void json__lexer_get_token(Lexer *l) {
    char chr = *l->curr;
    switch (chr) {
        case '{':
            l->kind = TOKEN_OPENCURLY;
            json__lexer_get_char(l);
            break;
        case '}':
            l->kind = TOKEN_CLOSECURLY;
            json__lexer_get_char(l);
            break;
        case '[':
            l->kind = TOKEN_OPENSQUARE;
            json__lexer_get_char(l);
            break;
        case ']':
            l->kind = TOKEN_CLOSESQUARE;
            json__lexer_get_char(l);
            break;
        case ',':
            l->kind = TOKEN_COMMA;
            json__lexer_get_char(l);
            break;
        case ':':
            l->kind = TOKEN_COLON;
            json__lexer_get_char(l);
            break;
        case '"': {  // get string
            json__lexer_get_char(l);
            char *start = l->curr;
            size_t len = 0;
            l->kind = TOKEN_STRING;
            char c = json__lexer_get_char(l);
            while (true) {
                // TODO: fix \uXXXX
                if (c == '\\') {
                    c = json__lexer_get_char(l);
                    switch (c) {
                        case '"': {
                            start[len] = '"';
                        } break;
                        case '\\': {
                            start[len] = '\\';
                        } break;
                        case '/': {
                            start[len] = '/';
                        } break;
                        case 'b': {
                            start[len] = '\b';
                        } break;
                        case 'f': {
                            start[len] = '\f';
                        } break;
                        case 'n': {
                            start[len] = '\n';
                        } break;
                        case 'r': {
                            start[len] = '\r';
                        } break;
                        case 't': {
                            start[len] = '\t';
                        } break;
                        default: {
                            l->kind = TOKEN_UNKNOWN;
                            json__set_lerror_raw(
                                l, "invalid string: unknown escape sequence");
                            return;
                        }
                    }
                    len++;
                    c = json__lexer_get_char(l);
                    continue;
                } else if (c == '\n' || c == '\r') {
                    l->kind = TOKEN_UNKNOWN;
                    json__set_lerror_raw(
                        l, "invalid string: strings cannot contain newline");
                    return;
                } else if (c == '"') {
                    break;
                }  
                start[len] = c;
                c = json__lexer_get_char(l);
                len++;
            }
            l->json_data.data_kind = JSON_STRING;
            l->json_data.data.stringval = (String){.start = start, .len = len};
        } break;
        case ' ':
        case '\t':
            while ((*l->curr) == ' ' || (*l->curr) == '\t') {
                // won't overflow as string is null terminated
                l->curr++;
                l->line_offset++;
            }
            l->curr--;
            l->line_offset--;
            json__lexer_get_char(l);
            json__lexer_get_token(l);
            break;
        case '\r':
        case '\n':
            json__lexer_get_char(l);
            json__lexer_get_token(l);
            break;
        case '\x03':
            l->kind = TOKEN_END;
            break;
        default: {
            if ((chr >= '0' && chr <= '9') ||
                json__lexer_expect_bytes(l, "-", 1) ||
                json__lexer_expect_bytes(l, "+", 1)) {
                bool is_real = false;
                bool is_negative = false;
                bool eE_present = false;
                const char *orig_start = l->curr;
                const char *start = l->curr;
                chr = json__lexer_peek_char(l);

                // TODO: i am iterating twice over the number (this and
                // strtod/l)
                while (true) {
                    if (chr >= '0' && chr <= '9') {
                        json__lexer_get_char(l);
                    } else if (chr == 'e' || chr == 'E') {
                        if (eE_present) break;
                        eE_present = true;
                        json__lexer_get_char(l);
                        if (json__lexer_peek_char(l) == '-' ||
                            json__lexer_peek_char(l) == '+') {
                            json__lexer_get_char(l);
                        }
                    } else if (chr == '.') {
                        if (is_real) {
                            break;
                        }
                        is_real = true;
                        json__lexer_get_char(l);
                    } else {
                        break;
                    }
                    chr = json__lexer_peek_char(l);
                }
                json__lexer_get_char(l);

                errno = 0;
                if (is_real || eE_present) {  // TODO: is 8.9e10 real or
                                              // integer?
                    l->kind = TOKEN_REAL;
                    char *end;
                    double realval = strtod(start, &end);
                    if (errno == ERANGE) {
                        if (realval == HUGE_VAL || realval == -HUGE_VAL) {
                            json__set_lerror_raw(
                                l, "invalid number: double overflow");
                        } else {
                            json__set_lerror_raw(
                                l, "invalid number: double underflow");
                        }
                        l->kind = TOKEN_UNKNOWN;
                        l->json_data.data.stringval =
                            (String){.start = orig_start, .len = 1};
                        return;
                    }
                    //"123." is valid strtod but not in json
                    if (*(end - 1) == '.') {
                        json__set_lerror_raw(
                            l, "invalid number: decimal cannot end with '.'");
                        l->kind = TOKEN_UNKNOWN;
                        l->json_data.data.stringval =
                            (String){.start = orig_start, .len = 1};
                        return;
                    }
                    l->json_data.data_kind = JSON_REAL;
                    l->json_data.data.realval =
                        realval * (is_negative ? -1 : 1);
                } else {
                    l->kind = TOKEN_INTEGER;
                    char *end;
                    long intval = strtol(start, &end, 10);
                    if (errno == ERANGE) {
                        if (intval == LONG_MIN) {
                            json__set_lerror_raw(
                                l, "invalid number: integer underflow");
                        } else {
                            json__set_lerror_raw(
                                l, "invalid number: integer overflow");
                        }
                        l->kind = TOKEN_UNKNOWN;
                        l->json_data.data.stringval =
                            (String){.start = orig_start, .len = 1};
                        return;
                    }
                    if (*start == '0' && intval != 0) {
                        json__set_lerror_raw(l, "invalid number: leading zero");
                        l->kind = TOKEN_UNKNOWN;
                        l->json_data.data.stringval =
                            (String){.start = orig_start, .len = 1};
                        return;
                    }
                    l->json_data.data_kind = JSON_INTEGER;
                    l->json_data.data.intval = intval * (is_negative ? -1 : 1);
                }
            } else if (json__lexer_expect_bytes(l, "null", 4)) {
                json__lexer_get_expect_bytes(l, "null", 4);
                l->kind = TOKEN_NULL;
                l->json_data.data_kind = JSON_NULL;
            } else if (json__lexer_expect_bytes(l, "true", 4)) {
                json__lexer_get_expect_bytes(l, "true", 4);
                l->kind = TOKEN_BOOL;
                l->json_data.data_kind = JSON_BOOL;
                l->json_data.data.boolval = true;
            } else if (json__lexer_expect_bytes(l, "false", 5)) {
                json__lexer_get_expect_bytes(l, "false", 5);
                l->kind = TOKEN_BOOL;
                l->json_data.data_kind = JSON_BOOL;
                l->json_data.data.boolval = false;
            } else if (l->curr >= (l->end)) {
                l->kind = TOKEN_END;
            } else {
                json__set_lerror(l, "unknown token: %c", *l->curr);
                l->kind = TOKEN_UNKNOWN;
                l->json_data.data.stringval =
                    (String){.start = l->curr, .len = 1};
            }
        }
    }
}

// TODO: print the '' here, as { will be '{' but <EOF> shouldn't be '<EOF>' and
// string should be '"str"'
static char *json__lexer_print_token(Lexer *l) {
    switch (l->kind) {
        case TOKEN_OPENCURLY:
            return "{";
        case TOKEN_CLOSECURLY:
            return "}";
        case TOKEN_OPENSQUARE:
            return "[";
        case TOKEN_CLOSESQUARE:
            return "]";
        case TOKEN_COMMA:
            return ",";
        case TOKEN_COLON:
            return ":";
        case TOKEN_INTEGER:
            return arena_sprintf(l->arena, "%ld", l->json_data.data.intval);
        case TOKEN_REAL:
            return arena_sprintf(l->arena, "%lf", l->json_data.data.realval);
        case TOKEN_STRING:
            return json__arena_string_to_charp(l->arena,
                                               l->json_data.data.stringval);
        case TOKEN_BOOL:
            if (l->json_data.data.boolval) {
                return "true";
            }
            return "false";
        case TOKEN_NULL:
            return "null";
        case TOKEN_END:
            return "<EOF>";
        case TOKEN_UNKNOWN:
            return json__arena_string_to_charp(l->arena,
                                               l->json_data.data.stringval);
        default:
            assert(false && "unreachable token.kind");
            return "";
    }
}

static bool json__parse_object(Lexer *l, Json **res, bool toplevel);
static bool json__put_data(Json *json, Lexer *l);

static bool json__parse_array(Lexer *l, Json_Arr_Data *arr) {
    ArenaCheckpoint cp = arena_get_checkpoint(l->arena);
    *arr = (Json_Arr_Data){0};
    while (true) {
        json__lexer_get_token(l);
        if (l->kind == TOKEN_CLOSESQUARE) {
            return true;
        }

        Json json = {0};
        if (!json__put_data(&json, l)) {
            json__set_lerror(l, "expected json value, got '%s'",
                             json__lexer_print_token(l));
            arena_rewind(l->arena, cp);
            return false;
        }
        arena_vec_push(l->arena, arr, json.json_data);

        json__lexer_get_token(l);
        if (l->kind == TOKEN_CLOSESQUARE) {
            return true;
        }
        if (l->kind != TOKEN_COMMA) {
            json__set_lerror(l, "expected ',' or ']', got '%s'",
                             json__lexer_print_token(l));
            arena_rewind(l->arena, cp);
            return false;
        }
    }
}

static bool json__put_data(Json *json, Lexer *l) {
    switch (l->kind) {
        case TOKEN_STRING:
        case TOKEN_INTEGER:
        case TOKEN_REAL:
        case TOKEN_BOOL:
        case TOKEN_NULL:
            json->json_data = l->json_data;
            return true;
        case TOKEN_OPENCURLY:
            json->json_data.data_kind = JSON_OBJ;
            if (!json__parse_object(l, &json->json_data.data.objval, false)) {
                return false;
            }
            assert(l->kind == TOKEN_CLOSECURLY);

            return true;
        case TOKEN_OPENSQUARE:
            json->json_data.data_kind = JSON_ARRAY;
            if (!json__parse_array(l, &json->json_data.data.arrval)) {
                return false;
            }
            return true;
        default:
            return false;
    }
}

static bool json__parse_object(Lexer *l, Json **res, bool toplevel) {
    ArenaCheckpoint cp = arena_get_checkpoint(l->arena);
    Json *json = arena_alloc_struct_zeroed(l->arena, Json);
    if (json == NULL) {
        json__set_lerror_raw(l, "out of memory");
        goto fail;
    }
    json->arena = l->arena;
    if (toplevel) {
        if (!json__put_data(json, l)) {
            goto fail;
        }
        *res = json;
        return true;
    }

    json__lexer_get_token(l);
    switch (l->kind) {
        case TOKEN_STRING:
            json->key = l->json_data.data.stringval;
            json__lexer_get_token(l);
            if (l->kind != TOKEN_COLON) {
                json__set_lerror(l, "expected ':', got '%s'",
                                 json__lexer_print_token(l));
                goto fail;
            }
            json__lexer_get_token(l);
            switch (l->kind) {
                case TOKEN_STRING:
                case TOKEN_INTEGER:
                case TOKEN_REAL:
                case TOKEN_BOOL:
                case TOKEN_NULL:
                case TOKEN_OPENCURLY:
                case TOKEN_OPENSQUARE:
                    if (!json__put_data(json, l)) {
                        goto fail;
                    }
                    break;
                case TOKEN_UNKNOWN:
                    // json__set_lerror(
                    //     l, "expected json value, got unknown token '%s'",
                    //     json__lexer_print_token(l));
                    goto fail;
                default:
                    json__set_lerror(l, "expected json value, got '%s'",
                                     json__lexer_print_token(l));
                    goto fail;
            }
            json__lexer_get_token(l);
            if (l->kind == TOKEN_CLOSECURLY) goto end;
            if (l->kind != TOKEN_COMMA) {  // TODO: allows trailing commas
                json__set_lerror(l, "expected ',' or '}', got '%s'",
                                 json__lexer_print_token(l));
                goto fail;
            }
            break;
        case TOKEN_CLOSECURLY:
            return true;
        default:
            json__set_lerror(l, "expected '}', got '%s'",
                             json__lexer_print_token(l));
            goto fail;
    }
    if (!json__parse_object(l, &json->next, false)) {
        goto fail;
    }
end:
    if (l->kind != TOKEN_CLOSECURLY) {
        json__set_lerror(l, "expected '}', got '%s'",
                         json__lexer_print_token(l));
        goto fail;
    }
    *res = json;
    return true;

fail:
    *res = NULL;
    arena_rewind(l->arena, cp);
    return false;
}

static inline size_t json__max_size(size_t a, size_t b) {
    return a > b ? a : b;
}

Json *json_parse_string(const char *file_content, Json_Error *err) {
    json__clear_error(err, "", "parse");
    if (file_content == NULL) {
        json__set_error_impl(err, "", 0, 0, 0, "parse", "%s",
                             "file_content is null");
        return NULL;
    }
    // TODO: an extra iteration
    size_t file_size = strlen(file_content);
    if (file_size == 0) {
        json__set_error_raw(err, "json: file_content is empty");
        return NULL;
    }

    Arena *arena = malloc(sizeof(Arena));
    if (arena == NULL) {
        json__set_error_raw(err, "json: out of memory");
        return NULL;
    }
    const size_t arena_size = json__max_size(sizeof(Json) * 64, file_size * 15);
    if (arena_create(arena, arena_size) < 0) {
        json__set_error_raw(err, "json: out of memory");
        return NULL;
    }

    char *data = arena_alloc_array(arena, char, (size_t)file_size + 1);
    if (data == NULL) {
        json__set_error_raw(err, "json: out of memory");
        arena_destroy(arena);
        free(arena);
        return NULL;
    }
    memcpy(data, file_content, sizeof(char) * (file_size + 1));

    Lexer lexer = {.arena = arena,
                   .start = data,
                   .end = data + file_size,
                   .curr = data,
                   .path = "",
                   .err = err,
                   .line_number = 1,
                   .line_offset = 1};

    json__lexer_get_token(&lexer);
    Json *json = NULL;
    if (!json__parse_object(&lexer, &json, true) || json == NULL) {
        arena_destroy(arena);
        free(arena);
        return NULL;
    }
    json__lexer_get_token(&lexer);
    if (lexer.kind != TOKEN_END) {
        json__set_lerror((&lexer), "expected EOF, got '%s'",
                         json__lexer_print_token(&lexer));
        arena_destroy(arena);
        free(arena);
        return NULL;
    }

    json->input_file = "";
    json->arena = arena;
    json->is_toplevel = true;
    return json;
}

Json *json_parse_file(const char *file_name, Json_Error *err) {
    json__clear_error(err, file_name, "parse");
    if (file_name == NULL || strlen(file_name) == 0) {
        json__set_error_impl(err, file_name, 0, 0, 0, "parse", "%s",
                             "invalid file_name");
        return NULL;
    }

    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        json__set_error(err, "json: could not open file '%s'", file_name);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        json__set_error(err, "json: could not seek file '%s'", file_name);
        return NULL;
    }

    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size == 0) {
        json__set_error(err, "json: file %s is empty", file_name);
        fclose(f);
        return NULL;
    }

    Arena *arena = malloc(sizeof(Arena));
    if (arena == NULL) {
        json__set_error_raw(err, "json: out of memory");
        fclose(f);
        return NULL;
    }
    const size_t arena_size =
        json__max_size(sizeof(Json) * 64, (size_t)file_size * 15);
    if (arena_create(arena, arena_size) < 0) {
        json__set_error_raw(err, "json: out of memory");
        fclose(f);
        return NULL;
    }

    char *data = arena_alloc_array(arena, char, (size_t)file_size + 1);
    if (data == NULL) {
        json__set_error_raw(err, "json: out of memory");
        arena_destroy(arena);
        free(arena);
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(data, 1, (size_t)file_size, f);
    data[bytes_read] = '\0';
    fclose(f);

    Lexer lexer = {.arena = arena,
                   .start = data,
                   .end = data + bytes_read,
                   .curr = data,
                   .path = file_name,
                   .err = err,
                   .line_number = 1,
                   .line_offset = 1};

    json__lexer_get_token(&lexer);
    Json *json = NULL;
    if (!json__parse_object(&lexer, &json, true) || json == NULL) {
        arena_destroy(arena);
        free(arena);
        return NULL;
    }
    json__lexer_get_token(&lexer);
    if (lexer.kind != TOKEN_END) {
        json__set_lerror((&lexer), "expected EOF, got '%s'",
                         json__lexer_print_token(&lexer));
        arena_destroy(arena);
        free(arena);
        return NULL;
    }

    json->input_file = file_name;
    json->arena = arena;
    json->is_toplevel = true;
    return json;
}

typedef struct {
    char *items;
    size_t size;
    size_t capacity;
} Json__String;

void json__string_write(Json__String *s, const char *format, ...) {
    va_list args, args2;
    va_start(args, format);

    va_copy(args2, args);
    int n = vsnprintf(NULL, 0, format, args2);
    va_end(args2);
    if (n < 0) {
        va_end(args);
        return;
    }

    while ((int)(s->capacity - s->size) <= (n + 1)) {
        if (s->capacity == 0) s->capacity = 1;
        s->capacity *= 2;
        s->items = realloc(s->items, s->capacity * sizeof(*s->items));
    }

    vsnprintf(&s->items[s->size], (size_t)n + 1, format, args);
    va_end(args);

    s->size += (size_t)n;
}

typedef struct {
    FILE *f;
    Json__String string;
} Json__Writer;

#define json__writer_write(err, writer, ...)                                  \
    do {                                                                      \
        if ((writer)->f != NULL) {                                            \
            int n = fprintf((writer)->f, __VA_ARGS__);                        \
            if (n < 0) {                                                      \
                json__set_error_impl((err), (err) ? (err)->source : "", 0, 0, \
                                     0, "dump", "%s", "unable to write");     \
            }                                                                 \
        } else {                                                              \
            json__string_write(&((writer)->string), __VA_ARGS__);             \
        }                                                                     \
    } while (0)

static void json__dump_impl(Json *json, Json__Writer *w, int indent_len,
                            bool minified, Json_Error *err);
static void json__dump_data(Json_Value *json_data, Json__Writer *w,
                            int indent_len, bool minified, Json_Error *err) {
    switch (json_data->data_kind) {
        case JSON_NONE:
            return;
        case JSON_OBJ:
            if (minified) {
                json__writer_write(err, w, "{");
            } else {
                json__writer_write(err, w, "{\n");
            }
            indent_len += 4;
            json__dump_impl(json_data->data.objval, w, indent_len, minified,
                            err);
            indent_len -= 4;
            if (!minified) {
                for (int i = 0; i < indent_len; i++) {
                    json__writer_write(err, w, " ");
                }
            }
            json__writer_write(err, w, "}");
            break;
        case JSON_ARRAY:
            json__writer_write(err, w, "[");
            size_t len = json_data->data.arrval.size;
            for (size_t i = 0; i < len; i++) {
                json__dump_data(&json_data->data.arrval.items[i], w, indent_len,
                                minified, err);
                if (i != len - 1) {
                    if (minified)
                        json__writer_write(err, w, ",");
                    else
                        json__writer_write(err, w, ", ");
                }
            }
            json__writer_write(err, w, "]");
            break;
        case JSON_STRING:
            json__writer_write(err, w, "\"%.*s\"",
                               (int)json_data->data.stringval.len,
                               json_data->data.stringval.start);
            break;
        case JSON_INTEGER:
            json__writer_write(err, w, "%ld", json_data->data.intval);
            break;
        case JSON_REAL:
            json__writer_write(err, w, "%lf", json_data->data.realval);
            break;
        case JSON_BOOL:
            json__writer_write(err, w, "%s",
                               json_data->data.boolval ? "true" : "false");
            break;
        case JSON_NULL:
            json__writer_write(err, w, "null");
            break;
    }
}

static void json__dump_impl(Json *json, Json__Writer *w, int indent_len,
                            bool minified, Json_Error *err) {
    if (json == NULL) return;

    while (json != NULL) {
        if (!minified) {
            for (int i = 0; i < indent_len; i++) {
                json__writer_write(err, w, " ");
            }
        }

        if (!json->is_toplevel) {
            if (json->key.start == NULL || json->key.len == 0) {
                json__set_error_impl(err, err ? err->source : "", 0, 0, 0,
                                     "dump", "%s", "null key encountered");
                return;
            }
            if (minified) {
                json__writer_write(err, w, "\"%.*s\":", (int)json->key.len,
                                   json->key.start);
            } else {
                json__writer_write(err, w, "\"%.*s\": ", (int)json->key.len,
                                   json->key.start);
            }
        }

        json__dump_data(&json->json_data, w, indent_len, minified, err);
        if (json->next != NULL) {
            if (minified)
                json__writer_write(err, w, ",");
            else
                json__writer_write(err, w, ", ");
        }
        if (!minified) {
            json__writer_write(err, w, "\n");
        }

        json = json->next;
    }
}

char *json_stringify(Json *json, bool minified, Json_Error *err) {
    Json__Writer writer = {.string = {0}};
    if (json == NULL) {
        json__set_error_impl(err, "", 0, 0, 0, "dump", "%s", "json is null");
        return NULL;
    }
    json__clear_error(err, json->input_file, "dump");
    json__dump_impl(json, &writer, 0, minified, err);
    if (writer.string.items != NULL) {
        writer.string.items[writer.string.size] = 0;
    }
    return writer.string.items;
}

void json_dump(Json *json, FILE *f, bool minified, Json_Error *err) {
    Json__Writer writer = {.f = f};
    if (json == NULL) {
        json__set_error_impl(err, "", 0, 0, 0, "dump", "%s", "json is null");
        return;
    }
    json__clear_error(err, json->input_file, "dump");
    json__dump_impl(json, &writer, 0, minified, err);
}

void json_free(Json *json) {
    if (json != NULL && json->is_toplevel) {
        Arena *arena = json->arena;
        arena_destroy(arena);
        free(arena);
        json = NULL;
    }
}

enum Node_Kind json_kind(const Json *node) {
    if (node == NULL) return JSON_NONE;
    return node->json_data.data_kind;
}

bool json_is(const Json *node, enum Node_Kind kind) {
    return json_kind(node) == kind;
}

bool json_is_integer(const Json *node) { return json_is(node, JSON_INTEGER); }
bool json_is_real(const Json *node) { return json_is(node, JSON_REAL); }
bool json_is_number(const Json *node) {
    return json_is_integer(node) || json_is_real(node);
}
bool json_is_string(const Json *node) { return json_is(node, JSON_STRING); }
bool json_is_bool(const Json *node) { return json_is(node, JSON_BOOL); }
bool json_is_null(const Json *node) { return json_is(node, JSON_NULL); }
bool json_is_obj(const Json *node) { return json_is(node, JSON_OBJ); }
bool json_is_array(const Json *node) { return json_is(node, JSON_ARRAY); }

const char *json_key(const Json *member) {
    if (member == NULL) return NULL;
    return json__arena_string_to_charp(member->arena, member->key);
}

const Json *json_first(const Json *obj) {
    if (json_kind(obj) != JSON_OBJ) return NULL;
    return obj->json_data.data.objval;
}

const Json *json_next(const Json *member) {
    // cannot do this as non-obj also have next members
    // if (json_kind(member) != JSON_OBJ) return NULL;
    if (member == NULL) return NULL;
    return member->next;
}

const Json *json_value_find(const Json_Value *value, const char *key) {
    if (value == NULL || key == NULL || value->data_kind != JSON_OBJ)
        return NULL;
    size_t keylen = strlen(key);
    for (const Json *it = value->data.objval; it != NULL; it = json_next(it)) {
        if (keylen == it->key.len && strncmp(it->key.start, key, keylen) == 0)
            return it;
    }
    return NULL;
}

const Json *json_find(const Json *obj, const char *key) {
    if (obj == NULL) return NULL;
    return json_value_find(&obj->json_data, key);
}

long json_integer(const Json *node) {
    if (node == NULL) return 0;
    return node->json_data.data.intval;
}

double json_real(const Json *node) {
    if (node == NULL) return 0.0;
    return node->json_data.data.realval;
}

double json_number(const Json *node) {
    if (node == NULL) return 0.0;
    if (json_is_integer(node)) return (double)json_integer(node);
    return json_real(node);
}

String json_string_view(const Json *node) {
    if (node == NULL) return (String){0};
    return node->json_data.data.stringval;
}

char *json_cstring(const Json *node) {
    if (node == NULL) return NULL;
    return json__arena_string_to_charp(node->arena,
                                       node->json_data.data.stringval);
}

bool json_bool(const Json *node) {
    if (node == NULL) return false;
    return node->json_data.data.boolval;
}

size_t json_array_size(const Json *arr) {
    if (arr == NULL) return 0;
    return arr->json_data.data.arrval.size;
}

const Json_Value *json_array_at(const Json *arr, size_t i) {
    if (arr == NULL) return NULL;
    if (i >= arr->json_data.data.arrval.size) return NULL;
    return &arr->json_data.data.arrval.items[i];
}
#endif
