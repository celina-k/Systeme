# TAGSYS6

## Installation

Il faut commencer par mettre à jour la machine:
```sh
$ sudo apt update && apt upgrade
```
Les programmes `gcc` et `make` sont nécessaires pour compiler et installer
le système de tags, il faut donc les avoir installés:
```sh
$ sudo apt install gcc make 
```

Il faut ensuite se déplacer dans le répertoire du projet, dans lequel
se trouve le Makefile puis compiler:
```sh
$ make
```
Et finalement, installer le système de tag:
```sh
$ sudo make install 
```
Ceci copie les exécutables du répertoire `bin/` vers `/usr/local/bin/`,
ajoute un alias pour la commande `cp` au fichier `~/.bashrc` et crée un
fichier vide nécessaire à la configuration des tags.

Vous pouvez maintenant faire : 
```sh
$ source ~/. bashrc
```

Pour charger le nouvelle alias sans ouvrir de nouveau terminal.

## Désinstallation

Il faut se déplacer dans le répertoire du projet puis lancer la 
commande de désinstallation:
```sh
$ sudo make uninstall
```
Ceci supprimera les binaires situés dans `/usr/local/bin/`,
retirera l'alias créé dans `.bashrc` et supprimera le fichier
de configuration des tags `~/.tagsys6.json`.

## Commandes

Il y a 5 commandes pour manipuler les tags:
    
    assign-tag display-file-tag manage-tag rm-tag search-tag-file

### assign-tag 

```
Usage: ./bin/assign-tag <file> <tag> [<tag> ...]
   or: ./bin/assign-tag -h

Assign tag(s) to a file. The specified tag(s) must be present
in the user's configuration file of the tag-system. (See manage-tags)
The '-h' option prints this help message
```



### display-file-tag

```
Usage: ./bin/display-file-tag <file>
   or: ./bin/display-file-tag -q <file>
   or: ./bin/display-file-tag -h

Display the tags set by the user for the given file.
The '-q' option removes some output to only the tags
The '-h' option prints this help message
```

### manage-tag

```
Usage: ./bin/manage-tag [OPTION]

Manage the tags of the tag system:
	-r <tag>	Remove the given tag from the configuration file,
			 fails if the tag is not in the configuration file.
	-a <tag>	Create a new assignable tag <tag> and add it to the
			 configuration file, fails if the tag is already in the
			 configuration file.
	-A <tag>	Same as -a except that the tag created will not be assignable.
	-p <parent-tag> <tag>
			Create a new assignable tag <tag> and set it as a child of
			 <parent-tag>. The specified <tag> must not the name
			 of an existing tag.
	-P <parent-tag> <tag>
			Same as -p except that the tag created will not be assignable.
	-l		Print the tags available as well as their parent-children
			 relationship. '(*)' is appended to not assignable tags.
	-h		Print this help message
```

### rm-tag

```
Usage: ./bin/rm-tag <file> <tag> [<tag> ...]
   or: ./bin/rm-tag -c <file>
   or: ./bin/rm-tag -h

Remove tag(s) assigned to a file:
The '-c' option removes all the tags assigned to the file
The '-h' option prints this help message
```

### search-tag-file

```
Usage: ./bin/search-tag-file <dir> [<expression>]
   or: ./bin/search-tag-file -h

Search recursively for files matching the given expression.

<expression> is an expression consisting of tags and modifiers:
	_tag1 means that the file MUST NOT be tagged with tag1
	+tag2 means that the file MUST be tagged with tag2
A <tag_search_expr> is then a list of tags preceded by '_' or '+'
If no expression is provided, all the files tagged by the current
user and accessible from <dir> will be listed.
The '-h' option prints this help message
```
