#include "tag.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/xattr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

/**
 * Displays the tags of the domain XATTR_PROG_DOMAIN
 * for the file named [name] which file descriptor [fd] is given.
 * If [quiet] is [true], no output except the tag names will be
 * will be generated.
 */
static bool display_tags(int fd, char const *name, bool is_quiet)
{
    ssize_t buflen = flistxattr(fd, NULL, 0);
    if (buflen == -1)
        return false;
    else if (buflen == 0)
        return true;

    char *buf = malloc(buflen);
    if (buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    buflen = flistxattr(fd, buf, buflen);
    if (buflen == -1) {
        free(buf);
        return false;
    }

    /*
     * Loop over the list of zero terminated strings with the
     * attribute keys. Use the remaining buffer length to determine
     * the end of the list.
     */
    char *key = buf;
    char *tagname;
    size_t keylen;
    bool has_tag_printed = false;
    while (buflen > 0) {
        keylen = strlen(key) + 1; // +1 for the null byte

        if (keylen - 1 > XATTR_PROG_DOMAIN_LEN
                && strncmp(key, XATTR_PROG_DOMAIN, XATTR_PROG_DOMAIN_LEN) == 0) {
            if (!has_tag_printed && !is_quiet) {
                printf("# file '%s' has tags:\n", name);
                has_tag_printed = true;
            }
            tagname = key + XATTR_PROG_DOMAIN_LEN;
            printf("%s\n", tagname);
        }

        /* Forward to next attribute key.  */
        buflen -= keylen;
        key += keylen;
    }
    free(buf);
    return true;
}

static void print_help(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s <file>\n"
            "   or: %s -q <file>\n"
            "   or: %s -h\n\n"
            "Display the tags set by the user for the given file.\n"
            "The '-q' option removes some output to only the tags\n"
            "The '-h' option prints this help message\n\n",
            prog_name, prog_name, prog_name);
}

int main(int argc, char const *argv[])
{
    bool is_quiet = false;
    if (argc != 2 && argc != 3) {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }
    if(strcmp("-h", argv[1]) == 0) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    } else if(strcmp("-q", argv[1]) == 0) {
        is_quiet = true;
        if(argc!=3 || strcmp("-h", argv[2]) == 0) {
            print_help(argv[0]);
            return EXIT_FAILURE;
        }
    }
    int fd = open(argv[argc-1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Could not open target file '%s': %s\n",
                argv[argc-1], strerror(errno));
        return EXIT_FAILURE;
    }
    load_config();
    if (!display_tags(fd, argv[argc-1],is_quiet)) {
        fprintf(stderr, "Could not list tags: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
