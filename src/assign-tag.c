#include "../lib/cJSON.h"
#include "tag.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/xattr.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

/**
 *  Take a file descriptor [fd], and a tag name
 *  [tag] and assign the tag to the file referred
 *  by fd
 */
static bool set_tag(int fd, const char* tag)
{
    size_t attr_name_len = strlen(XATTR_PROG_DOMAIN) + strlen(tag);
    char attr_name[attr_name_len + 1];

    snprintf(attr_name, attr_name_len + 1, "%s%s", XATTR_PROG_DOMAIN, tag);
    if (fsetxattr(fd, attr_name, "", 0, XATTR_CREATE) == 0)
        return true;
    if (errno == EEXIST)
        fprintf(stderr, "This file is already tagged with '%s'\n", tag);
    else
        fprintf(stderr, "Could not tag this file: %s\n", strerror(errno));
    return false;
}

/**
 * Takes a filetag [tag] and searches its definition
 * in our [tag_tree], if it exists returns true
 * else returns false
 */
static bool check_is_assignable(const char* tag, TagsTree* tag_tree)
{
    TagsTree res_tag;
    TagError tag_error;
    bool assignable = false;
    if (get_tag(tag_tree, tag, &res_tag, &tag_error) && tag_error == NO_ERROR) {
        if ((tag_error = is_assignable(&res_tag, &assignable)) == NO_ERROR
                && assignable)
            return true;
        fprintf(stderr, "Tag '%s' is not assignable\n", tag);
    }
    if (tag_error == NO_ERROR) {
        fprintf(stderr, "Unknown tag '%s'.\n", tag);
    } else {
        print_tag_error(tag_error);
    }
    return false;
}

static void print_help(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s <file> <tag> [<tag> ...]\n"
            "   or: %s -h\n\n"
            "Assign tag(s) to a file. The specified tag(s) must be present\n"
            "in the user's configuration file of the tag-system. (See manage-tags)\n"
            "The '-h' option prints this help message\n\n",
            prog_name, prog_name);
}

/**
 * ./assign-tag tag filename
 */
int main(int argc, char const *argv[])
{
    if(argc < 2) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    int optlen = strlen(argv[1]);
    if (optlen == strlen("-h") && strncmp(argv[1], "-h", optlen) == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    } else if (argc < 3) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Could not open target file '%s': %s\n",
                filename, strerror(errno));
        return EXIT_FAILURE;
    }
    load_config();

    TagsTree tree;
    TagError rc = parse_config_file(JSON_CONFIG_FILE, &tree);
    if (rc != NO_ERROR) {
        print_tag_error(rc);
        goto FREE_RESOURCES_ON_ERROR;
    }

    int first_tag_pos = 2;
    const char *tag = NULL;
    for (int i = first_tag_pos; i < argc; i++) {
        tag = argv[i];
        if (!check_is_assignable(tag, &tree) || !set_tag(fd, tag))
            goto FREE_RESOURCES_ON_ERROR;
    }

    free_tags_tree(&tree);
    return EXIT_SUCCESS;

FREE_RESOURCES_ON_ERROR:
    free_tags_tree(&tree);
    return EXIT_FAILURE;
}
