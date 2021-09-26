PREFIX ?= /usr/local
CC = gcc -std=c11
CFLAGS = $(DEBUG) -pedantic -Wall -Werror

BUILDDIR = bin/
SRCDIR = src/

USER_NAME=$(shell logname)
USER_GROUP=$(shell id -rgn $(USER_NAME))
USER_HOME=$(shell cat /etc/passwd | grep $(USER_NAME) | cut -d':' -f6)
SKELETON_FILE = skeleton.json
TAGSYS6_FILE = $(USER_HOME)/.tagsys6.json
BASHRC_FILE = $(USER_HOME)/.bashrc
CP_ALIAS_LINE = alias cp='cp --preserve=xattr' \#aabf5ef21968838599788394e014dbe4e3259e0c

extsrc = $(wildcard lib/*.c)
testsrc= $(wildcard tests/*.c)
binsrc = $(wildcard bin/*)

tagsrc = $(SRCDIR)tag.c
assign-tagsrc = $(SRCDIR)assign-tag.c
display-file-tagsrc = $(SRCDIR)display-file-tag.c
manage-tagsrc = $(SRCDIR)manage-tag.c
rm-tagsrc = $(SRCDIR)rm-tag.c
search-tag-filesrc = $(SRCDIR)search-tag-file.c

extobj = $(extsrc:.c=.o)
testobj = $(testsrc:.c=.o)
tagobj = $(tagsrc:.c=.o)
assign-tagobj = $(assign-tagsrc:.c=.o)
display-file-tagobj = $(display-file-tagsrc:.c=.o)
manage-tagobj = $(manage-tagsrc:.c=.o)
rm-tagobj = $(rm-tagsrc:.c=.o)
search-tag-fileobj = $(search-tag-filesrc:.c=.o)

COREOBJ = $(tagobj) $(extobj)

OBJECTS = $(COREOBJ) $(testobj) $(assign-tagobj) $(display-file-tagobj) \
		  $(manage-tagobj) $(rm-tagobj) $(search-tag-fileobj)

.PHONY: all clean install uninstall

BINARIES = assign-tag display-file-tag manage-tag rm-tag search-tag-file

all: directories $(addprefix  $(BUILDDIR),  $(BINARIES))

$(BUILDDIR)assign-tag: $(assign-tagobj) $(COREOBJ)
	$(CC) -o $@ $^ 

$(BUILDDIR)display-file-tag: $(display-file-tagobj) $(COREOBJ)
	$(CC) -o $@ $^

$(BUILDDIR)manage-tag: $(manage-tagobj) $(COREOBJ)
	$(CC) -o $@ $^ 

$(BUILDDIR)rm-tag: $(rm-tagobj) $(COREOBJ)
	$(CC) -o $@ $^

$(BUILDDIR)search-tag-file: $(search-tag-fileobj) $(COREOBJ)
	$(CC) -o $@ $^

directories: 
	@mkdir -p $(BUILDDIR)

install:
	install $(binsrc) $(PREFIX)/bin
	$(file >> $(BASHRC_FILE),$(CP_ALIAS_LINE))
	cp $(SKELETON_FILE) $(TAGSYS6_FILE)
	chown $(USER_NAME):$(USER_GROUP) $(TAGSYS6_FILE)

uninstall:
	$(RM) $(PREFIX)/bin/assign-tag
	$(RM) $(PREFIX)/bin/display-file-tag
	$(RM) $(PREFIX)/bin/manage-tag
	$(RM) $(PREFIX)/bin/rm-tag
	$(RM) $(PREFIX)/bin/search-tag-file
	(grep -q "^$(CP_ALIAS_LINE)$$" '$(BASHRC_FILE)' && sed -in "/$(CP_ALIAS_LINE)/d" '$(BASHRC_FILE)')
	$(RM) $(TAGSYS6_FILE)
	

clean:
	$(RM) $(OBJECTS)
