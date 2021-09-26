#include "tag.h"
#include <assert.h>
#include <dirent.h>
#include <linux/xattr.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <errno.h>


/**
 * The parameters used to find files
 * given wanted_tags (must have) and
 * unwanted_tags (must, not have).
 */
typedef struct {
    RedimRedimStringArray wanted_tags;
    RedimRedimStringArray unwanted_tags;
} TagSearchParams;

/**
 * Frees the memory used by a RedimRedimStringArray object.
 */
static void free_redim_redim_array(RedimRedimStringArray *array)
{
    assert(array != NULL);
    RedimStringArray *str_array;
    for (int i = 0; i < array->nb_elt; i++) {
        str_array = &array->array[i];
        if (str_array)
            free(str_array->array);
    }
    free(array->array);
}

/**
 * Frees the memory used by a TagSearchParams object.
 */
static void free_tag_search_params(TagSearchParams *params)
{
    assert(params != NULL);
    free_redim_redim_array(&params->wanted_tags);
    free_redim_redim_array(&params->unwanted_tags);
}


/**
 * Appends the filename [fname] of [len] chars to the path [path].
 */
static void append_to_path(PascalBuffer *path, const char *fname, size_t len)
{
    assert(path != NULL);
    append_str_to_buffer(path, "/", 1);
    append_str_to_buffer(path, fname, len);
}

/**
 * Writes the list of extended file attributes xattr to [buffer].
 * Returns [true] if the operation succeeded, [false] otherwise.
 */
static bool get_xattr_list(const char *fname, PascalBuffer *buffer)
{
    assert(fname != NULL);
    assert(buffer != NULL);

    ssize_t buflen = listxattr(fname, NULL, 0);
    if (buflen == -1)
        return false;
    else if (buflen == 0)
        return true;

    if (buffer->str_capacity <= buflen)
        extends_buffer(buffer, buflen * 2);
    buffer->str_length = listxattr(fname, buffer->str, buffer->str_capacity);
    return buffer->str_length != -1;
}

/**
 * Returns [true] if the list of xattr in [xattr_buf] matches the
 * TagSearchParams [params], [false] otherwise.
 */
static bool valid_result(
        PascalBuffer *xattr_buf,
        const TagSearchParams *params)
{
    assert(xattr_buf != NULL);
    assert(params != NULL);

    char *key = xattr_buf->str;
    char *tag;
    size_t keylen;
    int count_wanted = 0;
    int nb_wanted = params->wanted_tags.nb_elt;
    bool is_tagged = false;
    RedimStringArray *str_array;
    bool has_tag[params->wanted_tags.nb_elt];
    memset(has_tag, false, params->wanted_tags.nb_elt);
    while (xattr_buf->str_length > 0) {
        keylen = strlen(key) + 1;

        if (keylen - 1 > XATTR_PROG_DOMAIN_LEN
                && strncmp(
                    key, XATTR_PROG_DOMAIN, XATTR_PROG_DOMAIN_LEN) == 0) {

            is_tagged = true;
            tag = key + XATTR_PROG_DOMAIN_LEN;
            for (int i = 0; i < nb_wanted && count_wanted < nb_wanted; i++) {
                str_array = &params->wanted_tags.array[i];
                for (int j = 0; j < str_array->nb_elt && !has_tag[i]; j++) {
                    if (strcmp(tag, str_array->array[j]) == 0 && !has_tag[i]) {
                        ++count_wanted;
                        has_tag[i] = true;
                    }
                }
            }
            for (int i = 0; i < params->unwanted_tags.nb_elt; i++) {
                str_array = &params->unwanted_tags.array[i];
                for (int j = 0; j < str_array->nb_elt; j++)
                    if (strcmp(tag, str_array->array[j]) == 0)
                        return false;
            }
            if (nb_wanted == count_wanted && params->unwanted_tags.nb_elt == 0)
                break;
        }

        /* Forward to next attribute key.  */
        xattr_buf->str_length -= keylen;
        key += keylen;
    }
    xattr_buf->str_length = 0;
    return count_wanted == nb_wanted && is_tagged;
}


/**
 * List the files in the directory [path] matching
 * the TagSearchParams [params]. The PascalBuffer [xattr_buf]
 * is used to store the attributes of the files and to avoid
 * allocating memory too often.
 */
static bool list_correct_files(
        const TagSearchParams *params,
        PascalBuffer *path,
        PascalBuffer *xattr_buf)
{
    assert(params != NULL);
    assert(path != NULL);
    assert(xattr_buf != NULL);

    DIR *dir = opendir(path->str);
    if (dir == NULL) {
        fprintf(stderr, "Could not open '%s' directory: %s\n",
                path->str, strerror(errno));
        return false;
    }
    bool success = true;
    struct dirent *cur;
    while ((cur = readdir(dir))) {
        if (strncmp(cur->d_name, "..", 3) != 0 && strncmp(cur->d_name, ".", 2) != 0) {
            struct stat s = {0};
            size_t len = strlen(cur->d_name);
            append_to_path(path, cur->d_name, len);

            stat(path->str, &s);
            if (S_ISDIR(s.st_mode)) {
                success = success && list_correct_files(params, path, xattr_buf);
            } else {
                if (!get_xattr_list(path->str, xattr_buf)) {
                    fprintf(stderr, "Could not get tags for '%s': %s\n",
                            path->str, strerror(errno));
                    success = false;
                } else if (valid_result(xattr_buf, params)) {
                    printf("%s\n", path->str);
                    xattr_buf->str_length = 0;
                }
            }
            path->str_length -= len + 1;
        }
    }
    closedir(dir);
    return success;
}

/**
 * Prints the name of the files, in the directory [directory], that
 * match the tag search parameters [params].
 * Returns [false] if an error occurs], [true] otherwise.
 */
static bool display_files(const char *directory, const TagSearchParams *params)
{
    assert(directory != NULL);
    assert(params != NULL);

    size_t dirname_len = strlen(directory);
    size_t path_len = (dirname_len + 2) * 2;
    char *path = malloc(path_len);
    if (path == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    int rc = snprintf(path, path_len, "%s", directory);
    if (rc < 0) {
        perror("snprintf");
        exit(EXIT_FAILURE);
    }
    PascalBuffer path_buf = {
        .str = path, .str_length = rc, .str_capacity = path_len
    };
    PascalBuffer xattr_buf = {0};
    bool success = list_correct_files(params, &path_buf, &xattr_buf);
    free(path_buf.str);
    free(xattr_buf.str);
    return success;
}


/**
 * Adds the names offf all the children tags of the TagsTree
 * root [root] to the Resizable string array [array].
 * Returns [false] if an error occurs, [true] otherwise.
 */
static bool add_all_children(const TagsTree *root, RedimStringArray *array)
{
    assert(root != NULL);
    assert(array != NULL);

    // add all children
    TagsTree children_array = {0};
    if (get_children_array(root, &children_array) != NO_ERROR) {
        return false;
    }

    bool success = true;
    TagsTree child_tag = {0};
    cJSON *child = NULL;
    TagError error;
    const char *tag_name = NULL;
    cJSON_ArrayForEach(child, children_array.json_tree) {
        child_tag.json_tree = child;
        if ((error = get_name(&child_tag, &tag_name)) != NO_ERROR) {
            print_tag_error(error);
            success = false;
            continue;
        }
        add_to_str_array(array, tag_name);
        success = success && add_all_children(&child_tag, array);
    }
    return success;
}

/**
 * Adds the children tags of the tag [tag] to the
 * Resizable string array [array].
 * Returns [false] if an error occurs, [true] otherwise.
 */
static bool add_tag_children(
        const TagsTree *root,
        const char *tag,
        RedimStringArray *array)
{
    assert(root != NULL);
    assert(tag != NULL);
    assert(array != NULL);

    TagsTree tag_tree;
    TagError tag_error;
    if (!get_tag(root, tag, &tag_tree, &tag_error) && tag_error != NO_ERROR) {
        print_tag_error(tag_error);
        return false;
    }
    return add_all_children(&tag_tree, array);
}

/**
 * Parses the [tag_search] list of [nb_members] elements
 * and writes the result to [*params].
 * If the [tag_search] list contains invalid search members,
 * an error message is printed and the function returns [false],
 * [true] is returned otherwise.
 */
bool get_tag_search_params(
        const char *tags_search[],
        int nb_members,
        TagSearchParams *params,
        const TagsTree *root)
{
    assert(tags_search != NULL);
    assert(params != NULL);

    memset(params, 0, sizeof(*params));
    const char *tag;
    for (int i = 0; i < nb_members; i++) {
        tag = tags_search[i];
        if (tag[0] != '+' && tag[0] != '_') {
            fprintf(stderr, "Invalid tag search member '%s': "
                    "char '%c' is not a valid modifier\n", tag, tag[0]);
            goto ERROR;
        }
        RedimStringArray str_array = {0};
        if (!add_tag_children(root, tag + 1, &str_array))
            goto ERROR;
        add_to_str_array(&str_array, tag + 1);
        switch (tag[0]) {
            case '_': add_to_redim_array(&params->unwanted_tags, &str_array);
                      break;
            case '+': add_to_redim_array(&params->wanted_tags, &str_array);
                      break;
            default:
                      assert(0);
        }
    }
    return true;

ERROR:
    free_tag_search_params(params);
    memset(params, 0, sizeof(*params));
    return false;
}

static void print_help(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s <dir> [<expression>]\n"
            "   or: %s -h\n\n"
            "Search recursively for files matching the given expression.\n\n"
            "<expression> is an expression consisting of tags and modifiers:\n"
            "\t_tag1 means that the file MUST NOT be tagged with tag1\n"
            "\t+tag2 means that the file MUST be tagged with tag2\n"
            "A <tag_search_expr> is then a list of tags preceded by '_' or '+'\n"
            "If no expression is provided, all the files tagged by the current\n"
            "user and accessible from <dir> will be listed.\n"
            "The '-h' option prints this help message\n\n",
            prog_name, prog_name);
}

int main(int argc, const char *argv[])
{
    if (argc < 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    } else if (strncmp(argv[1], "-h", 3) == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    const char *dirpath = argv[1];
    struct stat stats = {0};
    stat(dirpath, &stats);
    if (!S_ISDIR(stats.st_mode)) {
        fprintf(stderr, "Directory '%s' does not exist\n", dirpath);
        return EXIT_FAILURE;
    }

    load_config();

    TagsTree root;
    TagError error;
    if ((error = parse_config_file(JSON_CONFIG_FILE, &root)) != NO_ERROR) {
        print_tag_error(error);
        return EXIT_FAILURE;
    }

    TagSearchParams params = {0};
    if (!get_tag_search_params(argv + 2, argc - 2, &params, &root))
        goto FREE_RESOURCES_ON_ERROR;
    if (!display_files(argv[1], &params))
        goto FREE_RESOURCES_ON_ERROR;

    free_redim_redim_array(&params.wanted_tags);
    free_redim_redim_array(&params.unwanted_tags);
    cJSON_Delete(root.json_tree);
    return EXIT_SUCCESS;

FREE_RESOURCES_ON_ERROR:
    free_redim_redim_array(&params.wanted_tags);
    free_redim_redim_array(&params.unwanted_tags);
    cJSON_Delete(root.json_tree);
    return EXIT_FAILURE;
}
