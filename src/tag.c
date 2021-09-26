#include "tag.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define XATTR_ROOT_NAMESPACE "user.tagsys6."
#define CONFIG_FILE_LOCATION ".tagsys6.json"

static const char *parsing_error = NULL;
static char user_config_file[1000] = {0};
static bool user_config_file_set = false;
static size_t user_config_file_len = 0;
static char xattr_user_namespace[200] = {0};
static bool xattr_user_namespace_set = false;
static size_t xattr_user_namespace_len = 0;

const void load_config()
{
    config_file();
    xattr_namespace();
}

const char * config_file()
{
    if (!user_config_file_set) {
        char *home_dir = getenv("HOME");
        if (home_dir != NULL) {
            snprintf(user_config_file, 1000, "%s",
                    home_dir);
        } else {
            struct passwd *pw = getpwuid(getuid());
            if (pw == NULL) {
                fprintf(stderr,
                        "Could not determine home directory: %s\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
            home_dir = pw->pw_dir;
        }
        user_config_file_set = true;
        user_config_file_len = snprintf(
                user_config_file, 1000, "%s/"CONFIG_FILE_LOCATION, home_dir);
    }
    return user_config_file;
}

const size_t config_file_len()
{
    return user_config_file_len;
}

const char * xattr_namespace()
{
    if (!xattr_user_namespace_set) {
        xattr_user_namespace_set = true;
        uid_t uid = getuid();
        xattr_user_namespace_len = snprintf(
                xattr_user_namespace, 200, "%s%d.", XATTR_ROOT_NAMESPACE, uid);
    }
    return xattr_user_namespace;
}

const size_t xattr_namespace_len()
{
    return xattr_user_namespace_len;
}


void print_tag_error(TagError error)
{
    switch(error) {
        case LOAD_CONFIG_ERROR:
            fprintf(stderr, "Error loading config file '%s': %s\n",
                    JSON_CONFIG_FILE, strerror(errno));
            break;
        case PARSE_CONFIG_ERROR:
            fprintf(stderr, "Error parsing config file '%s', before: %s\n",
                    JSON_CONFIG_FILE, parsing_error);
            break;
        case INCOMPLETE_WRITE_CONFIG_ERROR:
            fprintf(stderr, "Error writing config file '%s': incomplete "
                    "write\n",
                    JSON_CONFIG_FILE);
            break;
        case INVALID_CONFIG_FILE:
            fprintf(stderr, "Error, invalid config file '%s'\n",
                    JSON_CONFIG_FILE);
            break;
        case NO_ERROR:
            break;
        default:
            fprintf(stderr, "An unknown error occured (%d)\n", error);
            break;
    }
}


void add_to_str_array(RedimStringArray *array, const char *str)
{
    assert(array != NULL);
    assert(str != NULL);

    if (array->nb_elt + 1 >= array->capacity) {
        int new_capacity = ((array->capacity + 1) * 2) * sizeof(char*);
        assert(new_capacity > 0);
        void *rc = realloc(array->array, new_capacity);
        if (rc == NULL) {
            free(array->array);
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        array->capacity = new_capacity;
        array->array = rc;
    }
    array->array[array->nb_elt++] = str;
}

void add_to_redim_array(
        RedimRedimStringArray *redim_array,
        RedimStringArray *str_array)
{
    assert(redim_array != NULL);
    assert(str_array != NULL);

    if (redim_array->nb_elt + 1 >= redim_array->capacity) {
        int new_capacity =
            ((redim_array->capacity + 1) * 2) * sizeof(*str_array);
        assert(new_capacity > 0);
        void *rc = realloc(redim_array->array, new_capacity);
        if (rc == NULL) {
            free(redim_array->array);
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        redim_array->capacity = new_capacity;
        redim_array->array = rc;
    }
    redim_array->array[redim_array->nb_elt++] = *str_array;
}

void free_redim_array(RedimRedimStringArray *redim_array) {
    if (!redim_array)
        return;
    free(redim_array->array);
}

void extends_buffer(PascalBuffer *buffer, size_t new_size)
{
    assert(buffer != NULL);
    void *rc = realloc(buffer->str, new_size);
    if (rc == NULL) {
        free(buffer->str);
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    buffer->str = rc;
    buffer->str_capacity = new_size;
}

void append_str_to_buffer(PascalBuffer *pb, const char *str, size_t len) {
    assert(pb != NULL);
    if (pb->str_length + len + 1 >= pb->str_capacity)
        extends_buffer(pb, 2 * (pb->str_length + len + 1));
    memmove(pb->str + pb->str_length, str, len);
    pb->str_length += len;
    pb->str[pb->str_length] = '\0';
}

bool add_subtree(TagsTree *root, RedimStringArray *arr)
{
    assert(root != NULL);

    bool no_error = true;
    if (!cJSON_IsArray(root->json_tree)) {
        fprintf(stderr, "Invalid config file: %s\n", JSON_CONFIG_FILE);
        exit(EXIT_FAILURE);
    }

    TagsTree child_tag = {0};
    TagsTree children_array = {0};
    cJSON *child = NULL;
    const char *tag_name = NULL;
    cJSON_ArrayForEach(child, root->json_tree) {
        child_tag.json_tree = child;
        if (get_name(&child_tag, &tag_name) != NO_ERROR)
            continue;
        printf("%s\n", tag_name);
        add_to_str_array(arr, tag_name);
        if (get_children_array(&child_tag, &children_array) != NO_ERROR)
            continue;
        no_error =
            no_error && add_subtree(&children_array, arr);
    }
    return no_error;
}

TagError parse_config_file(const char *filename, TagsTree *res)
{
    assert(filename != NULL);
    assert(res != NULL);

    int rc;
    FILE *f = fopen(filename, "r");
    if (f == NULL)
        return LOAD_CONFIG_ERROR;
    if ((rc = fseek(f, 0L, SEEK_END)) == -1)
        goto FILE_ERROR;
    long nbbytes;
    if ((nbbytes = ftell(f)) == -1)
        goto FILE_ERROR;
    if ((rc = fseek(f, 0L, SEEK_SET)) == -1)
        goto FILE_ERROR;


    char *buffer = malloc(nbbytes * sizeof(char));
    if (buffer == NULL) {
        fclose(f);
        perror("malloc");
        exit(EXIT_FAILURE); // Do not try to recover from a malloc error
    }
    long readbytes = fread(buffer, sizeof(char), nbbytes, f);
    if (readbytes < nbbytes && ferror(f) == 0) {
        free(buffer);
        goto FILE_ERROR;
    }
    rc = fclose(f);
    if (rc == EOF) {
        free(buffer);
        return LOAD_CONFIG_ERROR;
    }
    assert(readbytes == nbbytes);

    res->json_tree = cJSON_ParseWithLength(buffer, readbytes);
    free(buffer);
    if (res->json_tree == NULL) {
        parsing_error = cJSON_GetErrorPtr();
        return PARSE_CONFIG_ERROR;
    }

    return NO_ERROR;

FILE_ERROR:
    fclose(f);
    return LOAD_CONFIG_ERROR;
}

TagError write_config_file(const char *filename, TagsTree *src)
{
    assert(filename != NULL);
    assert(src != NULL);

    FILE *f = fopen(filename, "w");
    if (f == NULL)
        return LOAD_CONFIG_ERROR;
    char *str = cJSON_Print(src->json_tree);
    cJSON_Minify(str);
    size_t len = strlen(str);
    size_t rc = fwrite(str, 1, len,  f);
    fclose(f);
    free(str);
    if (rc != len)
        return INCOMPLETE_WRITE_CONFIG_ERROR;
    return NO_ERROR;
}

void free_tags_tree(TagsTree *tree)
{
    if (tree != NULL)
        cJSON_Delete(tree->json_tree);
}

int get_parent_array(
        const char *tag,
        const TagsTree *root,
        TagsTree *parent_array,
        TagError *error)
{
    assert(tag != NULL);
    assert(root != NULL);
    assert(parent_array != NULL);
    assert(error != NULL);

    *error = NO_ERROR;

    cJSON *tag_array = root->json_tree;
    if (!cJSON_IsArray(tag_array)) {
        *error = INVALID_CONFIG_FILE;
        return -1;
    }

    cJSON *tag_object = NULL;
    int rc = 0;
    TagsTree sub_tree;
    int i = 0;
    cJSON_ArrayForEach(tag_object, tag_array) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(
                tag_object, NAME_ATTRIBUTE);

        if (!cJSON_IsString(name) || name->valuestring == NULL) {
            *error = INVALID_CONFIG_FILE;
            return -1;
        }

        size_t tag_len = strlen(tag);
        if (tag_len == strlen(name->valuestring)
                && memcmp(tag, name->valuestring, tag_len) == 0) {
            parent_array->json_tree = tag_array;
            return i;
        }

        cJSON *children = cJSON_GetObjectItemCaseSensitive(
                tag_object, CHILDREN_ATTRIBUTE);
        sub_tree.json_tree = children;
        if (children != NULL) {
            rc = get_parent_array(
                    tag, &sub_tree, parent_array, error);
            if (rc >= 0 || *error != NO_ERROR)
                return rc;
        }
        i++;
    }
    return -1; // Not Found
}

/**
 * Returns an int code on error.
 * Config file structure is:
 * [
 *  {
 *   name: ...,
 *   assignable: ...,
 *   children: [
 *      {
 *          name: ...,
 *          assignable: ...,
 *          children: []
 *      }
 *   ]
 *  },
 *  {
 *   name: ...,
 *   assignable: ...,
 *   children: []
 *  },
 * ]
 */
bool get_tag(
        const TagsTree *root,
        const char *tag,
        TagsTree *res,
        TagError *tag_error)
{
    assert(root != NULL);
    assert(tag != NULL);
    assert(res != NULL);
    assert(tag_error != NULL);

    *tag_error = NO_ERROR;
    cJSON *tag_array = root->json_tree;
    if (!cJSON_IsArray(tag_array)) {
        *tag_error = INVALID_CONFIG_FILE;
        return false;
    }

    cJSON *tag_object = NULL;
    bool found = false;
    TagsTree sub_tree;
    cJSON_ArrayForEach(tag_object, tag_array) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(
                tag_object, NAME_ATTRIBUTE);

        if (!cJSON_IsString(name) || name->valuestring == NULL) {
            *tag_error = INVALID_CONFIG_FILE;
            return false;
        }

        size_t tag_len = strlen(tag);
        if (tag_len == strlen(name->valuestring)
                && memcmp(tag, name->valuestring, tag_len) == 0) {
            res->json_tree = tag_object;
            return true;
        }

        cJSON *children = cJSON_GetObjectItemCaseSensitive(
                tag_object, CHILDREN_ATTRIBUTE);
        sub_tree.json_tree = children;
        if (children != NULL) {
            found = get_tag(&sub_tree, tag, res, tag_error);
            if (found || *tag_error != NO_ERROR)
                return found;
        }
    }
    return false;
}

TagError is_assignable(const TagsTree *tag, bool *res)
{
    assert(tag != NULL);

    const cJSON *assignable = cJSON_GetObjectItemCaseSensitive(
            tag->json_tree, ASSIGNABLE_ATTRIBUTE);
    if (!cJSON_IsBool(assignable))
        return INVALID_CONFIG_FILE;

    *res = cJSON_IsTrue(assignable);
    return NO_ERROR;
}

TagError get_children_array(const TagsTree *tag, TagsTree *children_array)
{
    assert(tag != NULL);
    assert(children_array != NULL);

    cJSON *children = cJSON_GetObjectItemCaseSensitive(
            tag->json_tree, CHILDREN_ATTRIBUTE);
    if (!cJSON_IsArray(children))
        return INVALID_CONFIG_FILE;
    children_array->json_tree = children;
    return NO_ERROR;
}

TagError get_name(const TagsTree *tag, const char **name)
{
    assert(tag != NULL);

    const cJSON *name_obj = cJSON_GetObjectItemCaseSensitive(
            tag->json_tree, NAME_ATTRIBUTE);
    if (!cJSON_IsString(name_obj) || name_obj->valuestring == NULL)
        return INVALID_CONFIG_FILE;

    *name = name_obj->valuestring;
    return NO_ERROR;
}
