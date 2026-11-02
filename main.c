#include "ntverpctl.h"

struct config_app config;

static const char *program_name(const char *path){
    const char *slash = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    if (slash2 != NULL && (slash == NULL || slash2 > slash)) {
        slash = slash2;
    }
    return slash ? slash + 1 : path;
}

static void trim_input(char *s){
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    size_t start = 0;
    while (s[start] != '\0' && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
    len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

static int env_resolution(void){
    char env_buffer[MAX_PATH_LEN];
    DWORD env_result = GetEnvironmentVariableA("TREE", env_buffer, (DWORD)sizeof(env_buffer));

    if (env_result > 0 && env_result < sizeof(env_buffer)) {
        unix_to_windows(env_buffer, config.tree_path, sizeof(config.tree_path));
    } else {
        char input[MAX_PATH_LEN];
        fprintf(stderr, "TREE is not set. Source tree root: ");
        fflush(stderr);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            print_error("ERROR: TREE is not set");
            return 0;
        }
        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            print_error("ERROR: TREE path is too long");
            return 0;
        }
        trim_input(input);
        if (input[0] == '\0') {
            print_error("ERROR: TREE is not set");
            return 0;
        }
        unix_to_windows(input, config.tree_path, sizeof(config.tree_path));
    }

    trim_trailing_separators(config.tree_path);
    windows_to_unix(config.tree_path, config.bash_tree_path, sizeof(config.bash_tree_path));
    snprintf(config.lab_file_path, sizeof(config.lab_file_path), "%s" LAB_FILE_SUFFIX, config.tree_path);
    return 1;
}

static void print_usage(const char *prog_name){
    const char *name = program_name(prog_name);
    printf("Usage: %s <command> [value]\n", name);
    printf("\n");
    printf("  show               Show the current build lab and build numbers\n");
    printf("  bump               Increase VER_PRODUCTBUILD_QFE by 1\n");
    printf("  bump-build         Increase VER_PRODUCTBUILD by 1\n");
    printf("  set-build N        Set VER_PRODUCTBUILD to N\n");
    printf("  set-qfe N          Set VER_PRODUCTBUILD_QFE to N\n");
    printf("  set-lab NAME       Set the build lab name in tools\\razzle.cmd\n");
    printf("\n");
    printf("Environment:\n");
    printf("  TREE=<path>       Source tree root\n");
    printf("  NTVERP=<path>      Use this ntverp.h instead of searching the tree\n");
    printf("\n");
    printf("Originals are saved once as <file>.orig.\n");
    printf("After changing ntverp.h, do a clean rebuild: build -cZP\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    config.command_arg = argv[1];
    config.value_arg = (argc >= 3) ? argv[2] : NULL;

    if (_stricmp(config.command_arg, "-h") == 0 ||
        _stricmp(config.command_arg, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (!env_resolution()) {
        return 1;
    }

    if (_stricmp(config.command_arg, "show") == 0) {
        return execute_show(&config);
    }
    if (_stricmp(config.command_arg, "bump") == 0 ||
        _stricmp(config.command_arg, "bump-build") == 0) {
        return execute_edit(&config);
    }
    if (_stricmp(config.command_arg, "set-build") == 0 ||
        _stricmp(config.command_arg, "set-qfe") == 0) {
        if (!input_validation(config.value_arg)) {
            print_error("ERROR: %s needs a number", config.command_arg);
            return 1;
        }
        return execute_edit(&config);
    }
    if (_stricmp(config.command_arg, "set-lab") == 0) {
        return execute_set_lab(&config);
    }

    print_usage(argv[0]);
    return 1;
}
