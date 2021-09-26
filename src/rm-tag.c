#include "../lib/cJSON.h"
#include "tag.h"
#include <fcntl.h>
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

bool remove_tag(int fd, const char *tag)
{
    size_t tag_len = strlen(tag) + XATTR_PROG_DOMAIN_LEN;
    char attr_name[tag_len + 1];
    if (snprintf(attr_name, tag_len + 1, "%s%s", XATTR_PROG_DOMAIN, tag) < 0)
        return false;

    if (fremovexattr(fd, attr_name) != 0)
        return false;
    return true;
}

static void print_rm_tag_error(const char *tag, int error_num)
{
    fprintf(stderr, "Error removing tag '%s': %s\n", tag, strerror(error_num));
}

bool clear_tags(int fd)
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
        int errno_save = errno;
        free(buf);
        errno = errno_save;
        return false;
    }

    char *key = buf;
    const char *tag;
    size_t keylen;

    while (buflen > 0) {
        keylen = strlen(key) + 1; // +1 for null byte

        if (keylen - 1 > XATTR_PROG_DOMAIN_LEN
                && strncmp(key, XATTR_PROG_DOMAIN, XATTR_PROG_DOMAIN_LEN) == 0) {
            if (fremovexattr(fd, key) != 0) {
                tag = key + XATTR_PROG_DOMAIN_LEN;
                print_rm_tag_error(tag, errno);
                errno = 0;
            }
        }

        buflen -= keylen;
        key += keylen;
    }
    free(buf);
    return true;
}

static void print_help(const char *prog_name)
{
    fprintf(stderr,
            "Usage: %s <file> <tag> [<tag> ...]\n"
            "   or: %s -c <file>\n"
            "   or: %s -h\n\n"
            "Remove tag(s) assigned to a file:\n"
            "The '-c' option removes all the tags assigned to the file\n"
            "The '-h' option prints this help message\n\n",
            prog_name,prog_name,prog_name);
}

int main(int argc, char const *argv[])
{
    if(argc < 2) {
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    bool clear_tags_opt = false;
    const char *filename = NULL;
    int first_tag_pos = -1;
    int optlen = strlen(argv[1]);
    if (optlen == strlen("-c") && strncmp(argv[1], "-c", optlen) == 0) {
        if (argc != 3) {
            print_help(argv[0]);
            exit(EXIT_FAILURE);
        }
        clear_tags_opt = true;
        filename = argv[2];
    } else if (optlen == strlen("-h") && strncmp(argv[1], "-h", optlen) == 0) {
        print_help(argv[0]);
        exit(EXIT_SUCCESS);
    } else if (argc < 3) {
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    } else {
        filename = argv[1];
        first_tag_pos = 2;
    }

    int fd = open(filename, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "Could not open target file '%s': %s\n",
                filename, strerror(errno));
        exit(EXIT_FAILURE);
    }

    load_config();

    if (clear_tags_opt) {
        if (!clear_tags(fd)) {
            fprintf(stderr, "Could not clear tags: %s\n", strerror(errno));
            goto FREE_RESSOURCES_ON_ERROR;
        }
    } else {
        bool error_occured = false;
        for (int i = first_tag_pos; i < argc; i++) {
            if (!remove_tag(fd, argv[i])) {
                error_occured = true;
                print_rm_tag_error(argv[i], errno);
                errno = 0;
            }
        }
        if (error_occured)
            goto FREE_RESSOURCES_ON_ERROR;
    }

    close(fd);
    exit(EXIT_SUCCESS);

FREE_RESSOURCES_ON_ERROR:
    close(fd);
    exit(EXIT_FAILURE);
}
