# Architecture
### 1. Architecture logicielle

Afin d'avoir des commandes simples d'utilisation, nous avons choisi de produire
un executable différent par fonctionnalité. L'installation de notre système de 
gestion de tags produit donc les commandes suivantes:

| Command | Fonction |
| ------ | ------ |
| `assign-tag` | Assigner un ou plusieurs tags à un fichier |
| `display-file-tag` | Afficher les tags d'un fichier donné |
| `manage-tag` | Gérer l'arboresence des tags |
| `rm-tag` | Supprimer le(s) tag(s) d'un fichier |
| `search-tag-file` | Rechercher des fichers en fonction d'une conjonction de tags |

### 2. Implémentation et hiérarchie des tags

* Implémentation des tags  
  
Nous avons choisi de stocker les tags dans un fichier JSON (.tagsys6.json) situé dans le 
répertoire personnel de l'utilisateur. Notre système étant multi-utilisateur,
cela nous permet d'avoir facilement accès à l'arborescence des tags à utiliser.
Nous avons également décidé d'avoir des tags 'assignable' et 'non assignable'
et de laisser la possibilité à l'utilisateur de choisir entre l'un ou l'autre
à la création du tag.
Nous avons choisi de l'implémenter de cette facon car pour nous, certains tag 
n'ont pas forcément lieu de d'être assignable.
Par exemple, `couleur` correspond plus à une référence à un ensemble de tag 
qu'à un tag en lui même. Il pourrait ainsi être père d'un tag `rouge` ou `bleu` 
qui eux seraient assignables.

* Format de notre fichier de configuration

Nos tags sont enregistrés au format json pour faciliter la représentation de 
la relation parent-enfant des tags ainsi que l'utilisation d'attributs supplémentaires. 
Le fichier de configuration est 'minifié' (les espaces et retours à la ligne inutiles 
sont retirés) pour minimiser sa place sur le disque.
Dans le fichier de configuration, chaque tag possède 3 attributs:   
-> assignable : définit si le tag est assignable ou pas  
-> name : le nom du tag  
-> children : les enfants du tag  
  
Il faut savoir que si un tag parent d'un autre est supprimé, ces fils resteront 
présent dans le fichier de configuration mais remonteront au parent suivant.

### 3. La sécurité

Nous avons fait le choix d'utiliser `xattr` pour marquer les fichiers avec un 
ou plusieurs tags.
Nous utilisons l'API `xattr`, qui défini les opérations élémentaires sur 
les attributs étendus. Les attributs étendus étant implémenté par défaut sur 
un certain nombre de distributions linux incluant `Ubuntu 18.04`, nous évitons 
ainsi certains problèmes de sécurité que nous aurions pu rencontrer si nous 
avions implémenté nous-même cette partie.

### 4. Outils utilisés

| Outil | Description |
| ------ | ------ |
| `cJSON` | bibliothèque minimale pour la manipulation d'objets JSON en C |
| `XATTR` | API pour manipuler les attributs étendus |

Nous avons utilisé [cJSON](https://github.com/DaveGamble/cJSON) pour manipuler
les fichiers JSON car la bibliothèque est uniquement composée, d'un fichier 
header et d'un fichier source, et s'inclut simplement au projet.

Nous avons minimisé nos dépendances pour essayer d'être le moins lourd 
possible dans le système de l'utilsateur

### 5. Choix d'implémentation

Nous avons permis à notre programme d'être multi-utilisateur. Ainsi, 
tout utilisateur souhaitant utiliser le système de tag a uniquement besoin
de copier le fichier skeleton.json sous le nom .tagsys6.json dans son répertoire
personnel (seul l'utilisateur installant le système de tag en possède un par 
défaut). 
Les tags écrits dans les attributs étendus sont concaténés à 
l'identifiant de l'application ainsi qu'à l'UID de l'utilisateur courant. 
Nous pouvons ainsi facilement identifier les fichiers taggés par un utilisateur.
De plus, comme nous utilisons l'UID et non le nom d'utilisateur, le changement
de nom d'un utilisateur n'affecte pas notre système de tag.

### 6. Installation/Désinstallation du système de tag

Pour installer et désinstaller le système de tags, nous utilisons respectivement
les cibles `install` et `uninstall`. 

La cible `install` se charge d'abord de copier les binaires dans le répertoire 
`/usr/local/bin/` qui est sensé regrouper les programmes installés sans le 
gestionnaire de paquets. Ensuite, elle ajoute un alias `alias cp='cp --preserve=xattr'`
au fichier `~/.bashrc` de l'utilisateur qui permet l'utilisation de `cp` de 
façon transparente. Nous ajoutons un identiant suffisament long (40 caractères hexa) 
à la fin de la ligne ce qui nous évite de supprimer par inadvertance des commandes
similaires que l'utilisateur aurait ajouté. Pour finir, nous copions un fichier
JSON squelette `skeleton.json` dans le répertoire personnel de l'utilisateur et
le renommons en `.tagsys6.json`.

La cible `uninstall` fait l'opération inverse: elle supprime tout les binaires
copiés, supprime les lignes ajoutées au `.bashrc` en utilisant l'identifiant ajouté
et supprime le fichier `.tagsys6.json`.

### 7. Différentes améliorations possible

Actuellement, la recherche des fichiers par tag n'est pas très efficace
et peut être amélioré. 
Nous pourrions aussi fournir un fichier d'auto-complétion pour les options
de nos commandes. (Grâce à l'auto-completion fournie par bash/zsh).
