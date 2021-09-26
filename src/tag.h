#ifndef TAG_H
#define TAG_H

#include "../lib/cJSON.h"
#include <stdbool.h>

#define XATTR_PROG_DOMAIN xattr_namespace()
#define XATTR_PROG_DOMAIN_LEN xattr_namespace_len()
#define JSON_CONFIG_FILE config_file()

#define NAME_ATTRIBUTE "name"
#define ASSIGNABLE_ATTRIBUTE "assignable"
#define CHILDREN_ATTRIBUTE "children"


/**
 * Represents the errors that migth occur while
 *
 */
typedef enum {
    NO_ERROR = 0,
    LOAD_CONFIG_ERROR = 1,
    PARSE_CONFIG_ERROR = 2,
    INCOMPLETE_WRITE_CONFIG_ERROR = 3,
    INVALID_CONFIG_FILE = 4
} TagError;

/**
 * A resizable string buffer.
 */
typedef struct {
    char *str;
    size_t str_length;
    size_t str_capacity;
} PascalBuffer;

/**
 * A resizable array to store strings.
 */
typedef struct {
    const char **array;
    size_t nb_elt;
    size_t capacity;
} RedimStringArray;

/**
 * A resizable array of String arrays.
 */
typedef struct {
    RedimStringArray *array;
    size_t nb_elt;
    size_t capacity;
} RedimRedimStringArray;

/**
 * A simple wrapper arround cJSON objects.
 */
typedef struct {
    cJSON *json_tree;
} TagsTree;

/**
 * Loads the config of the tagsystem.
 * This function must be called first
 * in order to use either config_file()
 * or xattr_namespace().
 */
const void load_config();

/**
 * Returns the path to the config file
 * containing the tags.
 */
const char * config_file();

/**
 * Returns the length of the string representing
 * the path to the config file. Useful
 * to avoid recomputing the length all the time.
 */
const size_t config_file_len();

/**
 * Returns the string representing the
 * xattr namespace for the current user.
 */
const char * xattr_namespace();

/**
 * Same as config_file_len() but for
 * the xattr_namespace() function.
 */
const size_t xattr_namespace_len();

/**
 * Adds the string [str] to the RedimStringArray array [array].
 * If [array] does not have enough memory to hold str, it is
 * extended.
 */
void add_to_str_array(RedimStringArray *array, const char *str);

void free_str_array(RedimStringArray *str_array);

void add_to_redim_array(
        RedimRedimStringArray *redim_array,
        RedimStringArray *str_array);

void free_redim_array(RedimRedimStringArray *redim_array);

/**
 * The [str] member of [buffer] will be reallocated to be able to
 * hold [new_size] bytes. The [str_capacity] member of [buffer] will
 * be updated accordingly.
 */
void extends_buffer(PascalBuffer *buffer, size_t new_size);

void append_str_to_buffer(PascalBuffer *pb, const char *str, size_t len);

/**
 */
void print_tag_error(TagError error);

/**
 * Returns an int code on error.
 */
TagError parse_config_file(const char *filename, TagsTree *res);

/**
 * Writes the config file.
 */
TagError write_config_file(const char *filename, TagsTree *src);

/**
 * Frees the memory used by the TagsTree [tree]
 */
void free_tags_tree(TagsTree *tree);

/**
 * Returns an int code on error.
 */
bool get_tag(
        const TagsTree *tree,
        const char *tag,
        TagsTree *res,
        TagError *tag_error);

/**
 * Returns the index of tag in his
 * parent's array
 */
int get_parent_array(
        const char *tag,
        const TagsTree *root,
        TagsTree *parent_array,
        TagError *error);

/**
 * Returns an int code on error.
 */
TagError is_assignable(const TagsTree *tag, bool *res);

TagError get_children_array(const TagsTree *tag, TagsTree *children_array);

/**
 * Writes the name of the tag [tag] to [name].
 * Returns a TagError that can be used to determine whether or
 * not an error occured and to print a message to explain the error.
 */
TagError get_name(const TagsTree *tag, const char **name);

#endif
