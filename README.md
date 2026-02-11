**Minimalist HTTP/1.0 Server in C (bichttpd)**
Un serveur Web léger et performant écrit en C, capable de traiter des requêtes HTTP/1.0 en utilisant une architecture multi-processus. Ce projet a été développé pour comprendre les mécanismes bas niveau des protocoles réseau et de la communication client-serveur.

**Fonctionnalités**
Architecture Multi-processus : Utilisation de fork() pour gérer plusieurs connexions clients simultanément (chaque requête est traitée par un processus enfant).

Support HTTP/1.0 : Gestion des méthodes GET, HEAD et POST.

Gestion des Erreurs : Codes de retour standards implémentés (200 OK, 400 Bad Request, 403 Forbidden, 404 Not Found).

Sécurité intégrée : Protection contre les attaques de type "Directory Traversal" (empêche l'accès aux fichiers en dehors du dossier ./www via ..).

Configuration flexible : Support des arguments en ligne de commande (port, mode debug) et lecture via un fichier de configuration externe.

Mode Debug : Affichage détaillé des sockets, des adresses IP clients et des requêtes brutes pour le monitoring.

**Stack technique**
Langage : C (Standards POSIX/Linux)

Réseau : Sockets TCP/IP (AF_INET, SOCK_STREAM)

Système : Gestion des processus (fork, waitpid), manipulation de fichiers.

 **Tests et Environnement**
Ce serveur a été conçu pour fonctionner dans un environnement Linux.

Note technique : Pour valider le bon fonctionnement du serveur dans des conditions réelles, les tests ont été effectués sur une machine distante via une connexion SSH. Les échanges de paquets et la validité des headers HTTP ont été confirmés par des requêtes distantes.

**Utilisation**
Compilation
Bash
gcc -o bichttpd main.c
Lancement
Bash
./bichttpd -p 8080 -d on
-p : Port d'écoute (par défaut 42110 et peut être spécifié via l'option -p dans le terminal).

-d : Mode debug (on ou off).

-c : Chemin vers un fichier de configuration (ex: port=8080).
