#include "ntverpctl.h"

static void show_header(const char *path){
    char build[32] = "";
    char qfe[32] = "";
    size_t size = 0;
    char *data = read_file(path, &size);
    if (data != NULL) {
        parse_define_value(data, size, "VER_PRODUCTBUILD", build, sizeof(build));
        parse_define_value(data, size, "VER_PRODUCTBUILD_QFE", qfe, sizeof(qfe));
        free(data);
    }
    printf("%s\n", path);
    printf("    VER_PRODUCTBUILD     = %s\n", build);
    printf("    VER_PRODUCTBUILD_QFE = %s\n", qfe);
}

static int choose_headers(const struct header_list *headers, struct header_list *chosen){
    if (headers->count == 1) {
        return header_list_add(chosen, headers->items[0]);
    }

    printf("Several ntverp.h files define the version:\n");
    for (size_t i = 0; i < headers->count; i++) {
        printf("  %d) %s\n", (int)(i + 1), headers->items[i]);
    }
    printf("Edit which? (number, or 'a' for all): ");
    fflush(stdout);

    char answer[64];
    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        print_error("ERROR: Invalid choice");
        return 0;
    }
    size_t len = strlen(answer);
    while (len > 0 && (answer[len - 1] == '\n' || answer[len - 1] == '\r')) {
        answer[--len] = '\0';
    }

    if (_stricmp(answer, "a") == 0) {
        for (size_t i = 0; i < headers->count; i++) {
            if (!header_list_add(chosen, headers->items[i])) {
                return 0;
            }
        }
        return 1;
    }

    if (!check_numeric(answer)) {
        print_error("ERROR: Invalid choice");
        return 0;
    }
    unsigned long choice = strtoul(answer, NULL, 10);
    if (choice < 1 || choice > headers->count) {
        print_error("ERROR: Invalid choice");
        return 0;
    }
    return header_list_add(chosen, headers->items[choice - 1]);
}

static int edit_header(struct config_app *cfg, const char *path){
    size_t size = 0;
    char *data = read_file(path, &size);
    if (data == NULL) {
        print_error("ERROR: Failed to read %s", path);
        return 0;
    }

    char build[32] = "";
    char qfe[32] = "";
    parse_define_value(data, size, "VER_PRODUCTBUILD", build, sizeof(build));
    parse_define_value(data, size, "VER_PRODUCTBUILD_QFE", qfe, sizeof(qfe));

    const char *name = NULL;
    char value[32];
    if (_stricmp(cfg->command_arg, "bump") == 0) {
        if (qfe[0] == '\0') {
            print_error("ERROR: No numeric VER_PRODUCTBUILD_QFE in %s", path);
            free(data);
            return 0;
        }
        snprintf(value, sizeof(value), "%lu", strtoul(qfe, NULL, 10) + 1);
        name = "VER_PRODUCTBUILD_QFE";
    } else if (_stricmp(cfg->command_arg, "bump-build") == 0) {
        if (build[0] == '\0') {
            print_error("ERROR: No numeric VER_PRODUCTBUILD in %s", path);
            free(data);
            return 0;
        }
        snprintf(value, sizeof(value), "%lu", strtoul(build, NULL, 10) + 1);
        name = "VER_PRODUCTBUILD";
    } else if (_stricmp(cfg->command_arg, "set-build") == 0) {
        name = "VER_PRODUCTBUILD";
        snprintf(value, sizeof(value), "%s", cfg->value_arg ? cfg->value_arg : "");
    } else if (_stricmp(cfg->command_arg, "set-qfe") == 0) {
        name = "VER_PRODUCTBUILD_QFE";
        snprintf(value, sizeof(value), "%s", cfg->value_arg ? cfg->value_arg : "");
    } else {
        free(data);
        return 0;
    }

    if (!backup_once(path)) {
        print_error("ERROR: Failed to back up %s", path);
        free(data);
        return 0;
    }

    struct define_edit define_ctx = {name, value};
    size_t new_size = 0;
    char *new_data = transform_lines(data, size, replace_define_line, &define_ctx, &new_size);
    free(data);
    if (new_data == NULL) {
        print_error("ERROR: Failed to update %s in %s", name, path);
        return 0;
    }
    if (!write_file_atomic(path, new_data, new_size)) {
        print_error("ERROR: Failed to update %s in %s", name, path);
        free(new_data);
        return 0;
    }
    free(new_data);

    size_t comment_size = 0;
    char *comment_data = read_file(path, &comment_size);
    if (comment_data == NULL) {
        print_error("ERROR: Failed to read %s", path);
        return 0;
    }
    char new_build[32] = "";
    char new_qfe[32] = "";
    parse_define_value(comment_data, comment_size, "VER_PRODUCTBUILD", new_build, sizeof(new_build));
    parse_define_value(comment_data, comment_size, "VER_PRODUCTBUILD_QFE", new_qfe, sizeof(new_qfe));
    if (new_build[0] != '\0' && new_qfe[0] != '\0') {
        struct comment_edit comment_ctx = {new_build, new_qfe};
        size_t updated_size = 0;
        char *updated = transform_lines(comment_data, comment_size, replace_build_comment_line,
                                        &comment_ctx, &updated_size);
        if (updated == NULL || !write_file_atomic(path, updated, updated_size)) {
            free(updated);
            free(comment_data);
            print_error("ERROR: Failed to update build comment in %s", path);
            return 0;
        }
        free(updated);
    }
    free(comment_data);

    show_header(path);
    return 1;
}

int execute_show(struct config_app *cfg){
    if (path_exists(cfg->lab_file_path)) {
        size_t size = 0;
        char *data = read_file(cfg->lab_file_path, &size);
        if (data != NULL) {
            char lab[256];
            if (parse_lab_value(data, size, lab, sizeof(lab))) {
                printf("Build lab (razzle.cmd): %s\n", lab);
            }
            free(data);
        }
    }

    struct header_list headers = {0};
    if (!locate_headers(cfg, &headers)) {
        header_list_free(&headers);
        return 1;
    }
    for (size_t i = 0; i < headers.count; i++) {
        show_header(headers.items[i]);
    }
    header_list_free(&headers);
    return 0;
}

int execute_edit(struct config_app *cfg){
    struct header_list headers = {0};
    if (!locate_headers(cfg, &headers)) {
        header_list_free(&headers);
        return 1;
    }

    struct header_list chosen = {0};
    if (!choose_headers(&headers, &chosen)) {
        header_list_free(&chosen);
        header_list_free(&headers);
        return 1;
    }

    int result = 0;
    for (size_t i = 0; i < chosen.count; i++) {
        if (!edit_header(cfg, chosen.items[i])) {
            result = 1;
            break;
        }
    }
    header_list_free(&chosen);
    header_list_free(&headers);
    return result;
}

int execute_set_lab(struct config_app *cfg){
    if (cfg->value_arg == NULL) {
        print_error("ERROR: set-lab needs a name");
        return 1;
    }
    if (!path_exists(cfg->lab_file_path)) {
        print_error("ERROR: Not found: %s", cfg->lab_file_path);
        return 1;
    }
    if (!valid_lab_name(cfg->value_arg)) {
        print_error("ERROR: Lab name may only contain letters, digits, _ . -");
        return 1;
    }

    size_t size = 0;
    char *data = read_file(cfg->lab_file_path, &size);
    if (data == NULL) {
        print_error("ERROR: Failed to read %s", cfg->lab_file_path);
        return 1;
    }
    int lab_lines = count_lab_lines(data, size);
    if (lab_lines != 1) {
        print_error("ERROR: Expected exactly one '_BuildBranch=' line in razzle.cmd, found %d", lab_lines);
        free(data);
        return 1;
    }
    if (!backup_once(cfg->lab_file_path)) {
        print_error("ERROR: Failed to back up %s", cfg->lab_file_path);
        free(data);
        return 1;
    }

    size_t new_size = 0;
    char *new_data = transform_lines(data, size, replace_lab_line, cfg->value_arg, &new_size);
    free(data);
    if (new_data == NULL || !write_file_atomic(cfg->lab_file_path, new_data, new_size)) {
        free(new_data);
        print_error("ERROR: Failed to update %s", cfg->lab_file_path);
        return 1;
    }
    free(new_data);

    printf("Build lab is now: %s\n", cfg->value_arg);
    return 0;
}
