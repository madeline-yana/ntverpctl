#include "ntverpctl.h"

void print_error(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

int path_exists(const char *path){
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static int dir_exists(const char *path){
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static char *dup_string(const char *s){
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

char *read_file(const char *path, size_t *out_size){
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *data = malloc((size_t)size + 1);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(data);
        return NULL;
    }
    data[got] = '\0';
    *out_size = got;
    return data;
}

int write_file_atomic(const char *path, const char *data, size_t size){
    char dir[MAX_PATH_LEN + 1];
    const char *last = strrchr(path, '\\');
    const char *last2 = strrchr(path, '/');
    if (last2 != NULL && (last == NULL || last2 > last)) {
        last = last2;
    }
    if (last != NULL) {
        size_t len = (size_t)(last - path) + 1;
        if (len >= sizeof(dir)) {
            return 0;
        }
        memcpy(dir, path, len);
        dir[len] = '\0';
    } else {
        snprintf(dir, sizeof(dir), ".\\");
    }

    char temp[MAX_PATH_LEN];
    if (GetTempFileNameA(dir, "rzz", 0, temp) == 0) {
        return 0;
    }

    FILE *f = fopen(temp, "wb");
    if (f == NULL) {
        DeleteFileA(temp);
        return 0;
    }
    int ok = (fwrite(data, 1, size, f) == size);
    if (fclose(f) != 0) {
        ok = 0;
    }
    if (!ok) {
        DeleteFileA(temp);
        return 0;
    }
    if (!MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp);
        return 0;
    }
    return 1;
}

int backup_once(const char *path){
    char orig[MAX_PATH_LEN + sizeof(ORIG_SUFFIX)];
    int n = snprintf(orig, sizeof(orig), "%s" ORIG_SUFFIX, path);
    if (n < 0 || (size_t)n >= sizeof(orig)) {
        return 0;
    }
    if (path_exists(orig)) {
        return 1;
    }
    return CopyFileA(path, orig, FALSE) != 0;
}

void trim_trailing_separators(char *path){
    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        if (len == 3 && path[1] == ':') {
            break;
        }
        path[--len] = '\0';
    }
}

void windows_to_unix(const char *windows_path, char *unix_path_out, size_t max_len)
/*
Windows and *NIX applications disagree on whether to use backslashes or forward-slashes for their directory paths
We need to invoke a BASH tool here (certutil), so we need to convert between them here.
Git Bash/MSYS maps a drive to /<letter>, so D:\src becomes /d/src.
*/
{
    if (windows_path == NULL || max_len == 0) {
        return;
    }

    // Drive-letter path: D:\foo or D:/foo -> /d/foo
    if (isalpha((unsigned char)windows_path[0]) && windows_path[1] == ':' &&
        (windows_path[2] == '\\' || windows_path[2] == '/')) {
        snprintf(unix_path_out, max_len, "/%c/%s",
                 tolower((unsigned char)windows_path[0]), windows_path + 3);
    } else {
        snprintf(unix_path_out, max_len, "%s", windows_path);
    }

    for(int i = 0; unix_path_out[i]; i++){ // For every character in unix_path_out:
        if(unix_path_out[i] == '\\'){ // if that character is a backslash:
            unix_path_out[i] = '/'; // turn it into a forward slash, so Bash will be happy.
        }
    }
}

void unix_to_windows(const char *unix_path, char *windows_path_out, size_t max_len)
/*
Accept the path forms users actually set in TREE/NTVERP:
  /d/src      -> D:\src
  /mnt/d/...  -> D:\...   (WSL)
  D:/src      -> D:\src
*/
{
    if (unix_path == NULL || max_len == 0) {
        return;
    }

    if (unix_path[0] == '/' && isalpha((unsigned char)unix_path[1]) &&
        (unix_path[2] == '/' || unix_path[2] == '\0')) {
        if (unix_path[2] == '/') {
            snprintf(windows_path_out, max_len, "%c:\\%s",
                     toupper((unsigned char)unix_path[1]), unix_path + 3);
        } else {
            snprintf(windows_path_out, max_len, "%c:\\",
                     toupper((unsigned char)unix_path[1]));
        }
    } else if (strncmp(unix_path, "/mnt/", 5) == 0 &&
               isalpha((unsigned char)unix_path[5]) &&
               (unix_path[6] == '/' || unix_path[6] == '\0')) {
        if (unix_path[6] == '/') {
            snprintf(windows_path_out, max_len, "%c:\\%s",
                     toupper((unsigned char)unix_path[5]), unix_path + 7);
        } else {
            snprintf(windows_path_out, max_len, "%c:\\",
                     toupper((unsigned char)unix_path[5]));
        }
    } else {
        snprintf(windows_path_out, max_len, "%s", unix_path);
    }

    for (size_t i = 0; windows_path_out[i]; i++) {
        if (windows_path_out[i] == '/') {
            windows_path_out[i] = '\\';
        }
    }
}

int header_list_add(struct header_list *list, const char *path){
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 8;
        char **items = realloc(list->items, capacity * sizeof(*items));
        if (items == NULL) {
            return 0;
        }
        list->items = items;
        list->capacity = capacity;
    }
    char *copy = dup_string(path);
    if (copy == NULL) {
        return 0;
    }
    list->items[list->count++] = copy;
    return 1;
}

void header_list_free(struct header_list *list){
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int line_has_define_spaces(const char *line, size_t len, const char *name){
    if (len < 7 || strncmp(line, "#define", 7) != 0) {
        return 0;
    }
    size_t i = 7;
    if (i >= len || line[i] != ' ') {
        return 0;
    }
    while (i < len && line[i] == ' ') {
        i++;
    }
    size_t name_len = strlen(name);
    if (i + name_len > len || strncmp(line + i, name, name_len) != 0) {
        return 0;
    }
    i += name_len;
    return i < len && line[i] == ' ';
}

static int file_has_product_build_define(const char *path){
    size_t size = 0;
    char *data = read_file(path, &size);
    if (data == NULL) {
        return 0;
    }
    int found = 0;
    size_t pos = 0;
    while (pos < size) {
        size_t start = pos;
        while (pos < size && data[pos] != '\n') {
            pos++;
        }
        if (line_has_define_spaces(data + start, pos - start, "VER_PRODUCTBUILD")) {
            found = 1;
            break;
        }
        if (pos < size) {
            pos++;
        }
    }
    free(data);
    return found;
}

static int find_headers_recursive(const char *dir, struct header_list *out){
    size_t dir_len = strlen(dir);
    while (dir_len > 0 && (dir[dir_len - 1] == '\\' || dir[dir_len - 1] == '/')) {
        dir_len--;
    }

    char pattern[MAX_PATH_LEN + 4];
    if (snprintf(pattern, sizeof(pattern), "%.*s\\*", (int)dir_len, dir) >= (int)sizeof(pattern)) {
        return 0;
    }

    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return 1; // No files / inaccessible: yield nothing.
    }

    int ok = 1;
    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }
        char full[MAX_PATH_LEN];
        if (snprintf(full, sizeof(full), "%.*s\\%s", (int)dir_len, dir, find_data.cFileName) >= (int)sizeof(full)) {
            continue;
        }
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                if (!find_headers_recursive(full, out)) {
                    ok = 0;
                    break;
                }
            }
        } else if (_stricmp(find_data.cFileName, "ntverp.h") == 0) {
            if (file_has_product_build_define(full) && !header_list_add(out, full)) {
                ok = 0;
                break;
            }
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);
    return ok;
}

int locate_headers(struct config_app *cfg, struct header_list *out){
    char ntverp[MAX_PATH_LEN];
    DWORD ntverp_len = GetEnvironmentVariableA("NTVERP", ntverp, (DWORD)sizeof(ntverp));
    if (ntverp_len > 0 && ntverp_len < sizeof(ntverp)) {
        char windows_path[MAX_PATH_LEN];
        unix_to_windows(ntverp, windows_path, sizeof(windows_path));
        if (!path_exists(windows_path)) {
            print_error("ERROR: NTVERP does not exist: %s", ntverp);
            return 0;
        }
        return header_list_add(out, windows_path);
    }

    if (!dir_exists(cfg->tree_path)) {
        print_error("ERROR: Tree not found: %s", cfg->tree_path);
        return 0;
    }
    if (!find_headers_recursive(cfg->tree_path, out)) {
        return 0;
    }
    if (out->count == 0) {
        print_error("ERROR: No ntverp.h defining VER_PRODUCTBUILD found under %s", cfg->tree_path);
        return 0;
    }
    return 1;
}
