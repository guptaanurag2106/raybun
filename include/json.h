#ifndef JSON_H
#define JSON_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

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
    struct Json
        *jsonval;  // TODO: jsonval or objval? confusing terms (json_is_obj)
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

const char *json_get_error(void);

Json *json_parse_string(const char *file_content);
Json *json_parse_file(const char *file_name);

char *json_stringify(Json *json, bool minified);

void json_dump(Json *json, FILE *f, bool minified);

void json_free(Json *json);

// Traverse, query

enum Node_Kind json_kind(const Json *node);

bool json_is(const Json *node, enum Node_Kind kind);

// TODO: if you just want a number? (either int(number) or real)
// like it can be a float (say temperature) but you can pass a number to
// autoconvert to float
bool json_is_integer(const Json *node);
bool json_is_real(const Json *node);
bool json_is_number(const Json *node);
bool json_is_string(const Json *node);
bool json_is_bool(const Json *node);
bool json_is_null(const Json *node);
bool json_is_obj(const Json *node);
bool json_is_array(const Json *node);

const char *json_key(const Json *member);

const Json *json_next(const Json *member);

const Json *json_first(const Json *obj);

const Json *json_value_find(const Json_Value *obj, const char *key);

// TODO: find by case sensitive and insensitive
const Json *json_find(const Json *obj, const char *key);

long json_integer(const Json *node);

double json_real(const Json *node);

double json_number(const Json *node);

char *json_string(const Json *node);

bool json_bool(const Json *node);

size_t json_array_size(const Json *arr);

// TODO: it returns Json_value so you have to manually do ->data.realval etc
// convert Json_Value to json?
const Json_Value *json_array_at(const Json *arr, size_t i);

#endif  // JSON_H

#ifdef JSON_IMPLEMENTATION
#include <errno.h>

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
    const char *path;

    const char *curr;
    const char *end;

    enum Token_Kind kind;

    int line_number;
    int line_offset;
} Lexer;

#define JSON_ERROR_STR_SIZE 500
static char json_error[JSON_ERROR_STR_SIZE] = "";

// TODO:some error say json:, some say json_dump:, some say just expected ...
#define json__set_error_raw(msg)              \
    do {                                      \
        memcpy(json_error, msg, strlen(msg)); \
        json_error[strlen(msg)] = '\0';       \
    } while (0)

#define json__set_error(format, args)                                    \
    do {                                                                 \
        int n = snprintf(json_error, JSON_ERROR_STR_SIZE, format, args); \
        if (n < 0 || n >= JSON_ERROR_STR_SIZE) {                         \
            fprintf(stderr, "Error message length exceeded %d bytes\n",  \
                    JSON_ERROR_STR_SIZE);                                \
            break;                                                       \
        }                                                                \
        json_error[n] = '\0';                                            \
    } while (0)

// FIX: currently all error reporting points(line_offset) to end of token not
// start
#define json__set_lerror_raw(l, str)                                        \
    do {                                                                    \
        int n = snprintf(json_error, JSON_ERROR_STR_SIZE, "%s:%d:%d: " str, \
                         l->path, l->line_number, l->line_offset);          \
        if (n < 0 || n >= JSON_ERROR_STR_SIZE) {                            \
            fprintf(stderr, "Error message length exceeded %d bytes\n",     \
                    JSON_ERROR_STR_SIZE);                                   \
        }                                                                   \
        json_error[n] = '\0';                                               \
    } while (0)

#define json__set_lerror(l, format, args)                                      \
    do {                                                                       \
        int n = snprintf(json_error, JSON_ERROR_STR_SIZE, "%s:%d:%d: " format, \
                         l->path, l->line_number, l->line_offset, args);       \
        if (n < 0 || n >= JSON_ERROR_STR_SIZE) {                               \
            fprintf(stderr, "Error message length exceeded %d bytes\n",        \
                    JSON_ERROR_STR_SIZE);                                      \
        }                                                                      \
        json_error[n] = '\0';                                                  \
    } while (0)

const char *json_get_error(void) { return json_error; }

static char *json__arena_string_to_charp(Arena *arena, String data) {
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
            const char *start = l->curr;
            size_t len = 0;
            l->kind = TOKEN_STRING;
            char c = json__lexer_get_char(l);
            while (true) {
                if (c == '\\') {
                    c = json__lexer_get_char(l);
                    len++;
                } else if (c == '"')
                    break;
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
                if (json__lexer_get_expect_bytes(l, "-", 1)) {
                    is_negative = true;
                } else if (json__lexer_get_expect_bytes(l, "+", 1)) {
                }
                const char *start = l->curr;
                chr = json__lexer_peek_char(l);

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

                if (is_real || eE_present) {
                    l->kind = TOKEN_REAL;
                    errno = 0;
                    char *end;
                    double realval = strtod(start, &end);
                    if (errno == ERANGE) {
                        json__set_lerror_raw(l,
                                             "invalid number: double overflow");
                        // FIX: for all invalid in this case, no value set, and
                        // error reported as unknown not as invalid
                        // integer/decimal overflow etc
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
                    errno = 0;
                    char *end;
                    long intval = strtol(start, &end, 10);
                    if (errno == ERANGE) {
                        json__set_lerror_raw(
                            l, "invalid number: integer overflow");
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
            if (!json__parse_object(l, &json->json_data.data.jsonval, false)) {
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
                    json__set_lerror(
                        l, "expected json value, got unknown token '%s'",
                        json__lexer_print_token(l));
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

// TODO: merge json and json_parse_file
Json *json_parse_string(const char *file_content) {
    if (file_content == NULL) {
        json__set_error_raw("json: file_content is null");
        return NULL;
    }
    size_t file_size = strlen(file_content);
    if (file_size == 0) {
        json__set_error_raw("json: file_content is empty");
        return NULL;
    }

    Arena *arena = malloc(sizeof(Arena));
    if (arena == NULL) {
        json__set_error_raw("json: out of memory");
        return NULL;
    }
    const size_t arena_size = json__max_size(sizeof(Json) * 64, file_size * 15);
    if (arena_create(arena, arena_size) < 0) {
        json__set_error_raw("json: out of memory");
        return NULL;
    }

    Lexer lexer = {.arena = arena,
                   .end = file_content + file_size,
                   .curr = file_content,
                   .path = "",
                   .line_number = 1,
                   .line_offset = 1};

    json__lexer_get_token(&lexer);
    Json *json = NULL;
    if (!json__parse_object(&lexer, &json, true) || json == NULL) {
        return NULL;
    }
    json__lexer_get_token(&lexer);
    if (lexer.kind != TOKEN_END) {
        json__set_lerror((&lexer), "expected EOF, got '%s'",
                         json__lexer_print_token(&lexer));
        return NULL;
    }

    json->input_file = "";
    json->arena = arena;
    json->is_toplevel = true;
    return json;
}

Json *json_parse_file(const char *file_name) {
    if (file_name == NULL || strlen(file_name) == 0) {
        json__set_error_raw("json: invalid file_name");
        return NULL;
    }
    FILE *f = fopen(file_name, "r");
    if (f == NULL) {
        json__set_error("json: could not open file '%s'", file_name);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        json__set_error("json: could not seek file '%s'", file_name);
        return NULL;
    }

    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size == 0) {
        json__set_error("json: file %s is empty", file_name);
        return NULL;
    }

    Arena *arena = malloc(sizeof(Arena));
    if (arena == NULL) {
        json__set_error_raw("json: out of memory");
        fclose(f);
        return NULL;
    }
    const size_t arena_size =
        json__max_size(sizeof(Json) * 64, (size_t)file_size * 15);
    if (arena_create(arena, arena_size) < 0) {
        json__set_error_raw("json: out of memory");
        fclose(f);
        return NULL;
    }

    char *data = arena_alloc_array(arena, char, (size_t)file_size + 1);
    if (data == NULL) {
        json__set_error_raw("json: out of memory");
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(data, 1, (size_t)file_size, f);
    data[bytes_read] = '\0';
    fclose(f);

    Lexer lexer = {.arena = arena,
                   .end = data + bytes_read,
                   .curr = data,
                   .path = file_name,
                   .line_number = 1,
                   .line_offset = 1};

    json__lexer_get_token(&lexer);
    Json *json = NULL;
    if (!json__parse_object(&lexer, &json, true) || json == NULL) {
        return NULL;
    }
    json__lexer_get_token(&lexer);
    if (lexer.kind != TOKEN_END) {
        json__set_lerror((&lexer), "expected EOF, got '%s'",
                         json__lexer_print_token(&lexer));
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

    if ((int)(s->capacity - s->size) <= n) {
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

#define json__writer_write(writer, ...)                           \
    do {                                                          \
        if ((writer)->f != NULL) {                                \
            fprintf((writer)->f, __VA_ARGS__);                    \
        } else {                                                  \
            json__string_write(&((writer)->string), __VA_ARGS__); \
        }                                                         \
    } while (0)

static void json__dump_impl(Json *json, Json__Writer *w, int indent_len,
                            bool minified);
static void json__dump_data(Json_Value *json_data, Json__Writer *w,
                            int indent_len, bool minified) {
    switch (json_data->data_kind) {
        case JSON_NONE:
            return;
        case JSON_OBJ:
            if (minified) {
                json__writer_write(w, "{");
            } else {
                json__writer_write(w, "{\n");
            }
            indent_len += 4;
            json__dump_impl(json_data->data.jsonval, w, indent_len, minified);
            indent_len -= 4;
            if (!minified) {
                for (int i = 0; i < indent_len; i++) {
                    json__writer_write(w, " ");
                }
            }
            json__writer_write(w, "}");
            break;
        case JSON_ARRAY:
            json__writer_write(w, "[");
            size_t len = json_data->data.arrval.size;
            for (size_t i = 0; i < len; i++) {
                json__dump_data(&json_data->data.arrval.items[i], w, indent_len,
                                minified);
                if (i != len - 1) {
                    if (minified)
                        json__writer_write(w, ",");
                    else
                        json__writer_write(w, ", ");
                }
            }
            json__writer_write(w, "]");
            break;
        case JSON_STRING:
            json__writer_write(w, "\"%.*s\"",
                               (int)json_data->data.stringval.len,
                               json_data->data.stringval.start);
            break;
        case JSON_INTEGER:
            json__writer_write(w, "%ld", json_data->data.intval);
            break;
        case JSON_REAL:
            json__writer_write(w, "%lf", json_data->data.realval);
            break;
        case JSON_BOOL:
            json__writer_write(w, "%s",
                               json_data->data.boolval ? "true" : "false");
            break;
        case JSON_NULL:
            json__writer_write(w, "null");
            break;
    }
}

static void json__dump_impl(Json *json, Json__Writer *w, int indent_len,
                            bool minified) {
    if (json == NULL) return;

    while (json != NULL) {
        if (!minified) {
            for (int i = 0; i < indent_len; i++) {
                json__writer_write(w, " ");
            }
        }

        if (!json->is_toplevel) {
            if (json->key.start == NULL || json->key.len == 0) {
                json__set_error_raw("json_dump: null key encountered");
                return;
            }
            if (minified) {
                json__writer_write(w, "\"%.*s\":", (int)json->key.len,
                                   json->key.start);
            } else {
                json__writer_write(w, "\"%.*s\": ", (int)json->key.len,
                                   json->key.start);
            }
        }

        json__dump_data(&json->json_data, w, indent_len, minified);
        if (json->next != NULL) {
            if (minified)
                json__writer_write(w, ",");
            else
                json__writer_write(w, ", ");
        }
        if (!minified) {
            json__writer_write(w, "\n");
        }

        json = json->next;
    }
}

char *json_stringify(Json *json, bool minified) {
    Json__Writer writer = {.string = {0}};
    json__dump_impl(json, &writer, 0, minified);
    if (minified) json__string_write(&writer.string, "\n");
    writer.string.items[writer.string.size] = 0;
    return writer.string.items;
}

void json_dump(Json *json, FILE *f, bool minified) {
    Json__Writer writer = {.f = f};
    json__dump_impl(json, &writer, 0, minified);
    if (minified) fprintf(f, "\n");
}

void json_free(Json *json) {
    if (json != NULL && json->is_toplevel) {
        Arena *arena = json->arena;
        arena_destroy(arena);
        free(arena);
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

const Json *json_next(const Json *member) {
    if (member == NULL) return NULL;
    return member->next;
}

const Json *json_first(const Json *obj) {
    if (obj == NULL) return NULL;
    return obj->json_data.data.jsonval;
}

const Json *json_value_find(const Json_Value *value, const char *key) {
    if (value == NULL || key == NULL || value->data_kind != JSON_OBJ)
        return NULL;
    size_t keylen = strlen(key);
    for (const Json *it = value->data.jsonval; it != NULL; it = json_next(it)) {
        if (it->key.start != NULL && keylen == it->key.len &&
            strncmp(it->key.start, key, keylen) == 0)
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

char *json_string(const Json *node) {
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
