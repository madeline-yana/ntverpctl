#include "ntverpctl.h"

struct buffer {
    char *data;
    size_t len;
    size_t cap;
};

int check_numeric(const char *str){
    if (str == NULL || *str == '\0') {
        return 0;
    }
    for (const char *p = str; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return 0;
        }
    }
    return 1;
}

int input_validation(const char *value_arg){
    return check_numeric(value_arg);
}

int valid_lab_name(const char *s){
    if (s == NULL || *s == '\0') {
        return 0;
    }
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

static int find_define_value_span(const char *line, size_t len, const char *name,
                                  size_t *value_start, size_t *value_end){
    if (len < 7 || strncmp(line, "#define", 7) != 0) {
        return 0;
    }
    size_t i = 7;
    if (i >= len || !isspace((unsigned char)line[i])) {
        return 0;
    }
    while (i < len && isspace((unsigned char)line[i])) {
        i++;
    }
    size_t name_start = i;
    while (i < len && !isspace((unsigned char)line[i])) {
        i++;
    }
    size_t name_len = i - name_start;
    if (name_len != strlen(name) || strncmp(line + name_start, name, name_len) != 0) {
        return 0;
    }
    if (i >= len || !isspace((unsigned char)line[i])) {
        return 0;
    }

    size_t end = len;
    if (end > 0 && line[end - 1] == '\r') {
        end--;
    }
    size_t content_end = end;
    while (content_end > i && isspace((unsigned char)line[content_end - 1])) {
        content_end--;
    }
    size_t token_end = content_end;
    size_t token_start = token_end;
    while (token_start > i && !isspace((unsigned char)line[token_start - 1])) {
        token_start--;
    }
    if (token_start == token_end) {
        return 0;
    }
    for (size_t k = token_start; k < token_end; k++) {
        if (!isdigit((unsigned char)line[k])) {
            return 0;
        }
    }
    *value_start = token_start;
    *value_end = token_end;
    return 1;
}

static int extract_define_value(const char *line, size_t len, const char *name,
                                char *out, size_t out_size){
    size_t start = 0;
    size_t end = 0;
    if (out_size == 0 || !find_define_value_span(line, len, name, &start, &end)) {
        return 0;
    }
    size_t value_len = end - start;
    if (value_len >= out_size) {
        value_len = out_size - 1;
    }
    memcpy(out, line + start, value_len);
    out[value_len] = '\0';
    return 1;
}

int parse_define_value(const char *data, size_t size, const char *name,
                              char *out, size_t out_size){
    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t pos = 0;
    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        if (extract_define_value(data + start, pos - start, name, out, out_size)) {
            return 1;
        }
        if (pos < size) {
            pos++;
        }
    }
    return 0;
}

static int match_lab_line(const char *line, size_t len, size_t *value_start, size_t *value_end){
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    if (i + 3 > len || strncmp(line + i, "set", 3) != 0) {
        return 0;
    }
    i += 3;
    if (i >= len || !(line[i] == ' ' || line[i] == '\t')) {
        return 0;
    }
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    const char *key = "_BuildBranch=";
    size_t key_len = strlen(key);
    if (i + key_len > len || strncmp(line + i, key, key_len) != 0) {
        return 0;
    }
    i += key_len;
    size_t end = i;
    while (end < len && !isspace((unsigned char)line[end]) && line[end] != ')') {
        end++;
    }
    *value_start = i;
    *value_end = end;
    return 1;
}

int parse_lab_value(const char *data, size_t size, char *out, size_t out_size){
    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    size_t pos = 0;
    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        size_t value_start = 0;
        size_t value_end = 0;
        if (match_lab_line(data + start, pos - start, &value_start, &value_end)) {
            size_t value_len = value_end - value_start;
            if (value_len >= out_size) {
                value_len = out_size - 1;
            }
            memcpy(out, data + start + value_start, value_len);
            out[value_len] = '\0';
            return 1;
        }
        if (pos < size) {
            pos++;
        }
    }
    return 0;
}

int count_lab_lines(const char *data, size_t size){
    int count = 0;
    size_t pos = 0;
    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        size_t value_start = 0;
        size_t value_end = 0;
        if (match_lab_line(data + start, pos - start, &value_start, &value_end)) {
            count++;
        }
        if (pos < size) {
            pos++;
        }
    }
    return count;
}

static int buffer_append(struct buffer *b, const char *data, size_t len){
    if (len == 0) {
        return 1;
    }
    if (b->len + len > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + len) {
            cap *= 2;
        }
        char *grown = realloc(b->data, cap);
        if (grown == NULL) {
            return 0;
        }
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    return 1;
}

char *transform_lines(const char *data, size_t size, line_transform_fn fn,
                             void *ctx, size_t *out_size){
    struct buffer out = {0};
    size_t pos = 0;
    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        size_t line_len = pos - start;
        char *replacement = NULL;
        size_t replacement_len = 0;
        if (fn(data + start, line_len, ctx, &replacement, &replacement_len)) {
            if (!buffer_append(&out, replacement, replacement_len)) {
                free(replacement);
                free(out.data);
                return NULL;
            }
            free(replacement);
        } else if (!buffer_append(&out, data + start, line_len)) {
            free(out.data);
            return NULL;
        }
        if (pos < size && data[pos] == '\n') {
            if (!buffer_append(&out, "\n", 1)) {
                free(out.data);
                return NULL;
            }
            pos++;
        }
    }
    if (out.data == NULL) {
        out.data = malloc(1);
        if (out.data == NULL) {
            return NULL;
        }
    }
    *out_size = out.len;
    return out.data;
}

int replace_define_line(const char *line, size_t len, void *vctx,
                               char **out, size_t *out_len){
    struct define_edit *ctx = vctx;
    size_t value_start = 0;
    size_t value_end = 0;
    if (!find_define_value_span(line, len, ctx->name, &value_start, &value_end)) {
        return 0;
    }
    size_t value_len = strlen(ctx->value);
    size_t new_len = len - (value_end - value_start) + value_len;
    char *buf = malloc(new_len ? new_len : 1);
    if (buf == NULL) {
        return 0;
    }
    memcpy(buf, line, value_start);
    memcpy(buf + value_start, ctx->value, value_len);
    memcpy(buf + value_start + value_len, line + value_end, len - value_end);
    *out = buf;
    *out_len = new_len;
    return 1;
}

int replace_build_comment_line(const char *line, size_t len, void *vctx,
                                      char **out, size_t *out_len){
    struct comment_edit *ctx = vctx;
    if (len < 2 || line[0] != '/' || line[1] != '/') {
        return 0;
    }
    size_t i = 2;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    const char *prefix = "Windows NT Build ";
    size_t prefix_len = strlen(prefix);
    if (i + prefix_len > len || strncmp(line + i, prefix, prefix_len) != 0) {
        return 0;
    }
    i += prefix_len;
    size_t number_start = i;
    while (i < len && isdigit((unsigned char)line[i])) {
        i++;
    }
    if (i == number_start || i >= len || line[i] != '.') {
        return 0;
    }
    i++;
    size_t fraction_start = i;
    while (i < len && isdigit((unsigned char)line[i])) {
        i++;
    }
    if (i == fraction_start) {
        return 0;
    }
    size_t number_end = i;

    char replacement[64];
    snprintf(replacement, sizeof(replacement), "%s.%s", ctx->build, ctx->qfe);
    size_t replacement_len = strlen(replacement);
    size_t new_len = len - (number_end - number_start) + replacement_len;
    char *buf = malloc(new_len ? new_len : 1);
    if (buf == NULL) {
        return 0;
    }
    memcpy(buf, line, number_start);
    memcpy(buf + number_start, replacement, replacement_len);
    memcpy(buf + number_start + replacement_len, line + number_end, len - number_end);
    *out = buf;
    *out_len = new_len;
    return 1;
}

int replace_lab_line(const char *line, size_t len, void *vctx,
                            char **out, size_t *out_len){
    const char *value = vctx;
    size_t value_start = 0;
    size_t value_end = 0;
    if (!match_lab_line(line, len, &value_start, &value_end)) {
        return 0;
    }
    size_t value_len = strlen(value);
    size_t new_len = len - (value_end - value_start) + value_len;
    char *buf = malloc(new_len ? new_len : 1);
    if (buf == NULL) {
        return 0;
    }
    memcpy(buf, line, value_start);
    memcpy(buf + value_start, value, value_len);
    memcpy(buf + value_start + value_len, line + value_end, len - value_end);
    *out = buf;
    *out_len = new_len;
    return 1;
}
