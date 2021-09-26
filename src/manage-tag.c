#include "../lib/cJSON.h"
#include "tag.h"
#include <linux/xattr.h>
#include <sys/xattr.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#define INDENT_CHILD    "|     "
#define INDENT_NO_CHILD "      "
#define BEFORE_TAG_STR  "\\---- "
#define NOT_ASSIGNABLE_HINT "(*)"

/**
 * Creates a new_tag with the name [tag].
 */
static cJSON * new_tag(const char *tag, bool assignable)
{
    assert(tag != NULL);
    cJSON *tag_obj = NULL;
    cJSON *name_obj = NULL;
    cJSON *assignable_obj = NULL;
    cJSON *children_obj = NULL;

    tag_obj = cJSON_CreateObject();
    if (tag_obj == NULL) {
        perror("cJSON_CreateObject");
        exit(EXIT_FAILURE);
    }

    name_obj = cJSON_CreateString(tag);
    if (name_obj == NULL) {
        perror("cJSON_CreateString");
        exit(EXIT_FAILURE);
    }

    cJSON_AddItemToObject(tag_obj, NAME_ATTRIBUTE, name_obj);

    assignable_obj = (assignable) ? cJSON_CreateTrue() : cJSON_CreateFalse();
    if (assignable_obj == NULL) {
        perror("cJSON_CreateTrue");
        exit(EXIT_FAILURE);
    }

    cJSON_AddItemToObject(tag_obj, ASSIGNABLE_ATTRIBUTE, assignable_obj);

    children_obj = cJSON_CreateArray();
    if (children_obj == NULL) {
        perror("cJSON_CreateArray");
        exit(EXIT_FAILURE);
    }
    cJSON_AddItemToObject(tag_obj, CHILDREN_ATTRIBUTE, children_obj);

    return tag_obj;
}

/**
 * Adds the tag named [tag_name] to the parent tag [parent_obj].
 */
bool add_to_parent(
        const char *tag_name,
        TagsTree *parent_obj,
        TagsTree *root,
        TagError *error,
        bool assignable)
{
    assert(tag_name != NULL);
    assert(parent_obj != NULL);
    assert(root != NULL);

    cJSON *tag_obj = new_tag(tag_name, assignable);
    if (tag_obj == NULL) {
        perror("new_tag");
        exit(EXIT_FAILURE);
    }

    TagsTree children;
    if ((*error = get_children_array(parent_obj, &children)) != NO_ERROR)
        return false;

    cJSON_AddItemToArray(children.json_tree, tag_obj);
    write_config_file(JSON_CONFIG_FILE, root);
    return true;
}

/**
 * Adds the tag named [tag] to the TagsTree [tag_tree].
 */
bool add_tag(const char *tag, TagsTree *tag_tree, bool assignable)
{
    assert(tag != NULL);
    assert(tag_tree != NULL);

    cJSON *tag_obj = new_tag(tag, assignable);
    if (tag_obj == NULL) {
        perror("new_tag");
        exit(EXIT_FAILURE);
    }
    cJSON *root_array = tag_tree->json_tree;
    cJSON_AddItemToArray(root_array, tag_obj);

    write_config_file(JSON_CONFIG_FILE, tag_tree);
    return true;
}

/**
 * Remove the tag named [tag] from the TagsTree [root].
 */
bool remove_tag(const char *tag, TagsTree *root)
{
    assert(tag != NULL);
    assert(root != NULL);

    TagError error;
    TagsTree parent_array = {0};
    int index_in_array = get_parent_array(tag, root, &parent_array, &error);
    if (index_in_array < 0) {
        if (error != NO_ERROR)
            print_tag_error(error);
        else
            fprintf(stderr, "Error: tag '%s' not found in parent array! "
                    "This should be reported as a bug\n", tag);
        return false;
    }

    TagsTree tag_object = {
        .json_tree = cJSON_DetachItemFromArray(
                parent_array.json_tree, index_in_array)
    };
    TagsTree children_array = {0};
    if (get_children_array(&tag_object, &children_array) != NO_ERROR) {
        fprintf(stderr, "'%s' tag does not have an array of children\n", tag);
        return false;
    }

    // Reattach all the children to the parent array
    cJSON *child = NULL;
    cJSON *children = children_array.json_tree;
    for (int count = cJSON_GetArraySize(children); count > 0; count--) {
        child = cJSON_DetachItemFromArray(children, 0);
        cJSON_AddItemToArray(parent_array.json_tree, child);
    }

    cJSON_Delete(tag_object.json_tree);
    write_config_file(JSON_CONFIG_FILE, root);
    return true;
}


/**
 * List the tags available in the TagTree [root] of name [root_name].
 */
static bool list_available_tags(
        const TagsTree *root,
        const char *root_name,
        PascalBuffer *print_level)
{
    assert(root != NULL);
    assert(print_level != NULL);

    bool no_error = true;
    if (!cJSON_IsArray(root->json_tree)) {
        fprintf(stderr, "Invalid config file: %s\n", JSON_CONFIG_FILE);
        exit(EXIT_FAILURE);
    }

    TagsTree child_tag = {0};
    TagsTree children_array = {0};
    TagError error;
    cJSON *child = NULL;
    const char *tag_name = NULL;
    // keep track of the current indentation level
    size_t initial_level = print_level->str_length;
    bool assignable = false;
    cJSON_ArrayForEach(child, root->json_tree) {

        child_tag.json_tree = child;
        if (get_name(&child_tag, &tag_name) != NO_ERROR) {
            fprintf(stderr, "Could not retrieve tag name of one of %s%s\n",
                    (root_name) ? root_name:"the top level tag",
                    (root_name) ? "'s child":"");
            continue;
        }

        assignable = false;
        if ((error = is_assignable(&child_tag, &assignable)) != NO_ERROR)
           print_tag_error(error);
        printf("%s"BEFORE_TAG_STR"%s%s\n",
                (print_level->str) ? print_level->str : "",
                tag_name,
                (assignable) ? "" : NOT_ASSIGNABLE_HINT);
        if (get_children_array(&child_tag, &children_array) != NO_ERROR) {
           fprintf(stderr, "'%s' tag does not have an array of children\n",
                   tag_name);
           continue;
        }
        const char *indent =
            (child->next != NULL) ? INDENT_CHILD : INDENT_NO_CHILD;
        append_str_to_buffer(print_level, indent, strlen(indent));

        no_error = no_error
            && list_available_tags(&children_array, tag_name, print_level);

        print_level->str_length = initial_level; // reset indentation level
        print_level->str[print_level->str_length] = '\0';
    }
    return no_error;
}

/**
 * Takes a filetag [tag] and searches its definition
 * in our [tag_tree], if it exists returns true
 * else returns false
 */
static bool get_tag_checked(
        const char *tag,
        const TagsTree *root,
        TagsTree *tag_obj)
{
    assert(tag != NULL);
    assert(root != NULL);
    assert(tag_obj != NULL);
    TagError tag_error;
    if (get_tag(root, tag, tag_obj, &tag_error) && tag_error == NO_ERROR)
        return true;
    else if (tag_error != NO_ERROR)
        print_tag_error(tag_error);
    return false;
}

static void print_help(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s [OPTION]\n\n"
            "Manage the tags of the tag system:\n"
            "\t-r <tag>\tRemove the given tag from the configuration file,\n"
            "\t\t\t fails if the tag is not in the configuration file.\n"
            "\t-a <tag>\tCreate a new assignable tag <tag> and add it to the\n"
            "\t\t\t configuration file, fails if the tag is already in the\n"
            "\t\t\t configuration file.\n"
            "\t-A <tag>\tSame as -a except that the tag created will not be assignable.\n"
            "\t-p <parent-tag> <tag>\n"
            "\t\t\tCreate a new assignable tag <tag> and set it as a child of\n"
            "\t\t\t <parent-tag>. The specified <tag> must not the name\n"
            "\t\t\t of an existing tag.\n"
            "\t-P <parent-tag> <tag>\n"
            "\t\t\tSame as -p except that the tag created will not be assignable.\n"
            "\t-l\t\tPrint the tags available as well as their parent-children\n"
            "\t\t\t relationship. '"NOT_ASSIGNABLE_HINT"' is appended to not assignable tags.\n"
            "\t-h\t\tPrint this help message\n\n",
            prog_name);
}

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    } else if (strlen(argv[1]) == strlen("-h")
            && strncmp(argv[1], "-h", 2) == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }

    load_config();

    TagsTree root;
    TagError error;
    if ((error = parse_config_file(JSON_CONFIG_FILE, &root)) != NO_ERROR) {
        print_tag_error(error);
        return EXIT_FAILURE;
    }

    const char *tag, *parent_tag;
    TagsTree tag_obj;
    TagsTree parent_obj;
    PascalBuffer buffer = {0};
    bool assignable = true;
    switch(argv[1][1]) {
        case 'r':
            if (argc != 3) {
                print_help(argv[0]);
                goto FREE_RESOURCES_ON_ERROR;
            }
            tag = argv[2];
            if (!get_tag_checked(tag, &root, &tag_obj)) {
                fprintf(stderr, "Cannot remove inexistent tag '%s'\n", tag);
                goto FREE_RESOURCES_ON_ERROR;
            }
            remove_tag(tag, &root);
            break;

        case 'A':
            assignable = false;
        case 'a':
            if (argc != 3) {
                print_help(argv[0]);
                goto FREE_RESOURCES_ON_ERROR;
            }
            tag = argv[2];
            if (get_tag_checked(tag, &root, &tag_obj)) {
                fprintf(stderr, "Tag '%s' already exists\n", tag);
                goto FREE_RESOURCES_ON_ERROR;
            }
            add_tag(tag, &root, assignable);
            break;

        case 'P':
            assignable = false;
        case 'p':
            if (argc != 4) {
                print_help(argv[0]);
                goto FREE_RESOURCES_ON_ERROR;
            }
            parent_tag = argv[2];
            tag = argv[3];
            if (!get_tag_checked(parent_tag, &root, &parent_obj)) {
                fprintf(stderr, "Parent tag '%s' does not exist\n",
                        parent_tag);
                goto FREE_RESOURCES_ON_ERROR;
            } else if (get_tag_checked(tag, &root, &tag_obj)) {
                fprintf(stderr, "Tag '%s' already exists\n", tag);
                goto FREE_RESOURCES_ON_ERROR;
            }
            if (!add_to_parent(tag, &parent_obj, &root, &error, assignable)) {
                print_tag_error(error);
                goto FREE_RESOURCES_ON_ERROR;
            }
            break;

        case 'l':
            if (!list_available_tags(&root, NULL, &buffer))
                goto FREE_RESOURCES_ON_ERROR;
            break;

        default:
            fprintf(stderr, "Unkown option '%s'\n", argv[1]);
            print_help(argv[0]);
            goto FREE_RESOURCES_ON_ERROR;
    }

    free_tags_tree(&root);
    free(buffer.str);
    return EXIT_SUCCESS;

FREE_RESOURCES_ON_ERROR:
    free(buffer.str);
    free_tags_tree(&root);
    return EXIT_FAILURE;
}
