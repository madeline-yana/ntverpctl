#ifndef RAZZLE_H
#define RAZZLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>
#include <ctype.h>

#define MAX_PATH_LEN 260 // Max path enforced by Windows
#define RAZZLE_SUFFIX "\\tools\\razzle.cmd"
#define ORIG_SUFFIX ".orig"

struct config_app {
    char tree_path[MAX_PATH_LEN];
    char bash_tree_path[MAX_PATH_LEN];
    char razzle_path[MAX_PATH_LEN + sizeof(RAZZLE_SUFFIX)];
    char *command_arg;
    char *value_arg;
};

extern struct config_app config;

struct header_list {
    char **items;
    size_t count;
    size_t capacity;
};

struct define_edit {
    const char *name;
    const char *value;
};

struct comment_edit {
    const char *build;
    const char *qfe;
};

typedef int (*line_transform_fn)(const char *line, size_t len, void *ctx,
                                 char **out, size_t *out_len);

// Diagnostics and filesystem helpers
void print_error(const char *fmt, ...);
int path_exists(const char *path);
char *read_file(const char *path, size_t *out_size);
int write_file_atomic(const char *path, const char *data, size_t size);
int backup_once(const char *path);
void trim_trailing_separators(char *path);

// Path conversion
void windows_to_unix(const char *windows_path, char *unix_path_out, size_t max_len);
void unix_to_windows(const char *unix_path, char *windows_path_out, size_t max_len);

// Validation
int check_numeric(const char *str);
int input_validation(const char *value_arg);
int valid_lab_name(const char *s);

// Parsing
int parse_define_value(const char *data, size_t size, const char *name,
                       char *out, size_t out_size);
int parse_lab_value(const char *data, size_t size, char *out, size_t out_size);
int count_lab_lines(const char *data, size_t size);

// Line transforms
char *transform_lines(const char *data, size_t size, line_transform_fn fn,
                      void *ctx, size_t *out_size);
int replace_define_line(const char *line, size_t len, void *ctx,
                        char **out, size_t *out_len);
int replace_build_comment_line(const char *line, size_t len, void *ctx,
                               char **out, size_t *out_len);
int replace_lab_line(const char *line, size_t len, void *ctx,
                     char **out, size_t *out_len);

// Header discovery
int header_list_add(struct header_list *list, const char *path);
void header_list_free(struct header_list *list);
int locate_headers(struct config_app *cfg, struct header_list *out);

// Commands
int execute_show(struct config_app *cfg);
int execute_edit(struct config_app *cfg);
int execute_set_lab(struct config_app *cfg);

#endif
