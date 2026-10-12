# Laboratoire 2

Parallélisation d'un calcul sériel

_Écrit par Gabriel-Andrew Pollo-Guilbert, idée originale de Francis Giraldeau, mis à jour par
Félix Gagnon (2026)_

Les [systèmes hétérogènes](https://fr.wikipedia.org/wiki/Calcul_h%C3%A9t%C3%A9rog%C3%A8ne) sont des
systèmes informatiques utilisant plusieurs types de processeur ou de pièces de matériel
spécialisé. Il est souvent idéal d'effectuer certains calculs sur des processeurs spécialisés afin
d'augmenter la performance ou l'efficacité du système. Par exemple, le décodage d'une vidéo est
souvent effectué dans une carte graphique afin de ne pas affecter le reste du système.

Dans ce laboratoire, vous devez convertir un calcul sériel afin de l'exécuter sur plusieurs coeurs
avec [OpenMP](https://fr.wikipedia.org/wiki/OpenMP) et aussi sur un périphérique spécialisé, comme
une carte graphique, avec [OpenCL](https://fr.wikipedia.org/wiki/OpenCL).

## Code

- `source/main.c`
  - Contient le point d'entrée du programme qui traite les arguments en ligne de commande et démarre
    le calcul.
- `source/color.c` `source/kernel/helpers.cl` `include/color.h` `include/pixel.h`
  - Contiennent des structures et des fonctions pour manipuler les couleurs et pixels.
- `source/headless.c` `include/headless.h`
  - Contiennent l'interface non-graphique permettant d'effectuer le calcul.
- `source/viewer.c` `include/viewer.h`
  - Contiennent l'interface graphique (OpenGL, via GLFW) pour montrer le dessin.
- `include/log.h`
  - Contient des macros pour formatter et afficher des logs dans la sortie d'erreur.
- `source/sinoscope.c` `include/sinoscope.h`
  - Contiennent la structure et certaines fonctions pour le calcul.
- `source/sinoscope-serial.c`
  - Contient l'implémentation sérielle de référence du calcul (ne pas modifier).
- `source/sinoscope-openmp.c` (**À COMPLÉTER**)
  - Contient l'implémentation demandée avec OpenMP.
- `source/sinoscope-opencl.c` `source/kernel/sinoscope.cl` (**À COMPLÉTER**)
  - Contiennent l'implémentation demandée avec OpenCL.
- `source/opencl.c` `include/opencl.h`
  - Contiennent des fonctions utilitaires pour OpenCL (choix du périphérique, chargement du
    kernel, affichage du journal de compilation).
- `source/image.c` `include/image.h`
  - Contiennent la sauvegarde d'une image PNG (option `--save`).
- `source/platform.c` `include/platform.h` `source/stubs-*.c`
  - Contiennent la couche de portabilité (Linux/macOS/Windows) et les implémentations de
    remplacement utilisées par `sinoscope-nocl` et `sinoscope-nomp`.
- `source/matcher/`
  - Contient le vérificateur de consignes (cible `check-source`, voir ci-dessous).

Seuls les fichiers marqués **À COMPLÉTER** doivent être modifiés.

## Calcul

Le calcul effectué par le programme est simple. Celui-ci calcule la valeur d'une fonction pour tous
les points d'une matrice 2D. Le calcul de chacun des points individuels est trivialement
parallélisable. Lisez le fichier `source/sinoscope-serial.c` pour plus d'information.

## Spécifications

Les spécifications ci-dessous sont celles de votre équipe.

<!-- SPEC -->

De plus, les contraintes suivantes sont imposées :

- La compilation ne doit pas lancer d'avertissements.
- Le programme ne doit pas avoir de fuite de mémoire durant son exécution autre que celles
  provenant d'OpenMP et d'OpenCL.

### Environnement de développement

Toutes les dépendances (en-têtes et chargeur OpenCL, GLFW, stb) sont téléchargées et compilées par
CMake lors de la première configuration : il n'y a rien à installer à part les outils de
compilation et un pilote OpenCL.

- **Linux** : installez Clang, CMake et Ninja avec le gestionnaire de paquets de votre
  distribution, ainsi que
  - un pilote OpenCL pour votre carte graphique ou, à défaut, [PoCL](https://portablecl.org/)
    (paquet `pocl-opencl-icd`) qui utilise le processeur ;
  - le support d'OpenMP de Clang (`libomp-dev` sous Debian/Ubuntu) ;
  - pour l'interface graphique : `libgl-dev libx11-dev libxrandr-dev libxinerama-dev
    libxcursor-dev libxi-dev`. Sans ces paquets, l'interface graphique n'est pas compilée et
    vous devez utiliser `--headless`.
- **Windows** : installez [LLVM/Clang](https://github.com/llvm/llvm-project/releases),
  [CMake](https://cmake.org/download/) et [Ninja](https://ninja-build.org/) (ou
  `winget install LLVM.LLVM Kitware.CMake Ninja-build.Ninja`), ainsi que le pilote de votre
  carte graphique (il fournit OpenCL). Le projet se compile avec Clang et les outils de
  Visual Studio (Build Tools) ou MinGW ; utilisez une invite de commande « x64 Native Tools » ou
  initialisez l'environnement avec `vcvars64.bat` avant de configurer le projet.
- **macOS** : OpenCL est fourni par le système (avec une prise en charge limitée) mais le Clang
  d'Apple ne supporte pas OpenMP. Installez `brew install llvm cmake ninja` et configurez avec
  `-DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang`.
- **Dev Container** : sous n'importe quel système, ouvrez le dépôt dans
  [VS Code](https://code.visualstudio.com/) avec l'extension
  [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).
  L'image contient tous les outils ainsi que PoCL (OpenCL sur CPU). Il n'y a pas de fenêtre
  graphique dans le conteneur : utilisez `--headless`, `--save` et les cibles ci-dessous.

### Compilation

Pour compiler l'application, il est recommandé de créer un dossier `build/` à la racine du projet
afin de bien séparer les fichiers générés. On configure d'abord le projet avec `cmake` en
utilisant le générateur [Ninja](https://ninja-build.org/). La première configuration télécharge
et compile les dépendances, ce qui peut prendre quelques minutes.

```
$ cmake -B build -G Ninja
$ cmake --build build
```

Il n'est pas nécessaire de re-exécuter la première commande pour recompiler, seulement la
seconde. Vous obtenez trois exécutables :

| Exécutable       | Contenu                                                      |
| ---------------- | ------------------------------------------------------------ |
| `sinoscope`      | tout (sériel, OpenMP, OpenCL, interface graphique)           |
| `sinoscope-nocl` | sans OpenCL : fonctionne même si votre code OpenCL est absent |
| `sinoscope-nomp` | sans OpenMP : fonctionne même si votre code OpenMP est absent |

Exécutez `./build/sinoscope --help` pour voir les options du programme. Le fichier
`build/compile_commands.json` est généré pour votre éditeur (clangd, etc.).

### Exécution

Il y a 3 modes d'exécution : avec interface graphique, sans interface graphique (`--headless`)
ou vérification (`--check`). Les deux premiers modes sont interactifs, les contrôles suivants
sont disponibles :

- `[1]` utiliser l'algorithme sériel
- `[2]` utiliser l'algorithme basé sur OpenMP
- `[3]` utiliser l'algorithme basé sur OpenCL
- `[q]` quitter l'application

De plus, l'interface graphique offre aussi ces contrôles :

- `[-]` diminuer le polynôme de Taylor du calcul
- `[+]` augmenter le polynôme de Taylor du calcul
- `[espace]` activer/désactiver l'affichage

Le programme choisit le périphérique OpenCL à utiliser avec `--opencl-platform N` et
`--opencl-device N`.

### Commandes

Les commandes se lancent avec `ninja -C build <cible>`.

- `run-serial`, `run-openmp`, `run-opencl`
  - Exécutent respectivement le calcul sériel, OpenMP et OpenCL et affichent le temps écoulé.
    `run-serial` et `run-openmp` utilisent `sinoscope-nocl` et `run-opencl` utilise
    `sinoscope-nomp` : chacune fonctionne même si l'autre partie de votre travail n'est pas
    encore terminée.
- `run-all`
  - Compare les temps des trois implémentations (avec l'accélération par rapport au sériel),
    puis vérifie que les résultats d'OpenMP et d'OpenCL sont corrects. Les paramètres se
    changent avec `-DBENCH_ITERATIONS=...`, `-DBENCH_WIDTH=...` et `-DBENCH_HEIGHT=...` à la
    configuration.
- `test` (ou `ctest --test-dir build`)
  - Vérifie les résultats de l'implémentation OpenMP (`check-openmp`), de l'implémentation
    OpenCL (`check-opencl`) et le respect des consignes (`check-source`, si disponible).
- `check-source`
  - Vérifie que votre code respecte les spécifications de votre équipe (construit seulement si les
    librairies de développement de Clang sont installées, `libclang-dev` et `llvm-dev` sous
    Debian/Ubuntu).
- `format`
  - Utilise `clang-format` pour formatter le code source.

### Détection d'erreurs mémoire (AddressSanitizer)

Configurez le projet avec `-DENABLE_ASAN=ON`, dans un dossier séparé :

```
$ cmake -B build-asan -G Ninja -DENABLE_ASAN=ON
$ cmake --build build-asan
$ ctest --test-dir build-asan
```

## Remise

Une fois votre travail commité, générez directement l'archive de remise avec `git archive`. Seuls
`source/sinoscope-openmp.c`, `source/sinoscope-opencl.c` et `source/kernel/sinoscope.cl` sont à
remettre :

```
$ git archive --format=zip --output=matricule1_matricule2.zip HEAD -- source/sinoscope-openmp.c source/sinoscope-opencl.c source/kernel/sinoscope.cl
```
