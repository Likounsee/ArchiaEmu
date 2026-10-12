# ArchiaEmu — roadmap du framework global

Cette feuille de route priorise le **socle commun de l'émulateur**, pas le développement d'une console en particulier. Les cœurs de console servent à vérifier que les contrats communs sont réellement utilisables, mais ne doivent pas dicter l'architecture ni monopoliser les prochaines étapes.

Les pourcentages sont des estimations d'avancement fonctionnel, fondées sur le code présent et les tests vérifiés. Ils ne représentent ni le temps passé ni une promesse de compatibilité. Une phase n'est terminée que lorsque ses critères d'acceptation sont démontrés par des tests reproductibles.

## Priorités et état actuel

| Priorité | Domaine | État estimé | Résultat attendu |
| --- | --- | ---: | --- |
| **P0** | Fiabilité du runtime commun | 83 % | Contrats cohérents, cycle de vie sûr, erreurs récupérables, tests de régression |
| **P0** | Chargement et validation du contenu | 77 % | Pipeline commun qui identifie, valide et transmet des contenus sans faire confiance aux fichiers |
| **P0** | Contrats de modules et capacités | 85 % | Modules isolés, capacités explicites, découverte et création prévisibles |
| **P1** | Orchestration multi-sessions | 25 % | Hôte capable de gérer plusieurs sessions indépendantes sans état partagé accidentel |
| **P1** | Entrées, sorties et horloge hôte | 25 % | Contrats génériques d'entrée, vidéo, audio, cadence et arrêt propre |
| **P1** | Sauvegardes et restauration | 20 % | Stockage persistant robuste et formats de sauvegarde validés/versionnés |
| **P1** | Frontend minimal du framework | 0 % | Parcourir le contenu, sélectionner un module, lancer/arrêter et afficher les erreurs |
| **P2** | Sécurité et distribution des modules | 45 % | Frontières de confiance, provenance, installation atomique et récupération |
| **P2** | Diagnostics, tests et compatibilité | 35 % | Rapports exploitables, tests de conformité communs et matrice CI |
| **P3** | Performance et stabilité | 10 % | Benchmarks reproductibles et optimisation guidée par les mesures |
| **P4** | Extension du catalogue de systèmes | En attente | Ajouter des cœurs seulement après stabilisation des interfaces partagées |
| **P5** | Préparation de versions | 10 % | Paquets reproductibles, politique de support et critères de publication |

> **Point de départ réel :** le workspace Rust, le contrat de cœur, le registre, une partie du cycle de vie de session, les capacités, l'état d'entrée partagé, les fonctions de sauvegarde/restauration déléguées au cœur, la gestion de modules et le CI existent déjà. Il n'existe pas encore d'application complète permettant de charger un jeu et de l'émuler avec audio et vidéo de bout en bout. La réussite du CI vérifie les contrôles configurés, pas la compatibilité d'une console.

## Vérification transversale P0 — persistance du state store

- [x] Supprimer les fichiers temporaires lorsqu'une suppression de sauvegarde ou un renommage primaire-vers-backup échoue; un test de régression force l'échec de suppression de la sauvegarde et vérifie la préservation du primaire ainsi que l'absence de fichier temporaire orphelin.
- [x] Tester la récupération depuis une sauvegarde valide lorsque le fichier primaire dépasse la limite de 4 Mio; le test vérifie que l'état de secours est retourné et que le fichier primaire rejeté n'est pas supprimé silencieusement.
- [x] Le décodeur d'état indexe les identifiants sélectionnés et installés avec des ensembles de hachage pour détecter les doublons sans rescanner les enregistrements déjà lus, tout en conservant l'ordre d'encodage et les erreurs de format dédiées.
- [ ] Tester les autres pannes d'E/S du remplacement (échec du renommage temporaire, restauration de secours et synchronisation du répertoire) sur les plateformes CI prises en charge; ne pas considérer les échecs de synchronisation comme validés tant qu'ils ne sont pas observables et testés.

## P0 — rendre le runtime commun fiable

### P0.3 — Chargement et validation du contenu
- [x] Le détecteur de formats normalise les réponses `NeedMoreData` incohérentes des probes : une probe ne peut plus demander « plus de données » tout en indiquant une longueur déjà couverte par le préfixe. Un test vérifie les longueurs nulles, trop courtes et égales au préfixe.


### P0.1 — Cycle de vie de session
- [x] Contrat de cœur commun et erreurs explicites.
- [x] États de session et contrôles d'opérations selon l'état.
- [x] Capacités déclarées et refus des opérations non annoncées.
- [x] État d'entrée commun avec suppression des transitions dupliquées et relâchement explicite; la libération globale vérifie aussi la capacité d'entrée avant d'appeler le cœur.
- [x] API générique de sauvegarde/restauration d'état avec vérification de capacité.
- [x] Formaliser les transitions autorisées dans une table de référence dans `docs/SESSION_LIFECYCLE.md`; les tests vérifient les transitions invalides principales, pause/reprise, réinitialisation, budget nul et arrêt sur `Halted`. La couverture exhaustive de chaque combinaison reste à compléter dans les tâches ci-dessous.
- [ ] Garantir un comportement documenté après chaque erreur du cœur : état de session, entrées maintenues, événements et possibilité de reprise. Les gardes d'entrée sont aussi testées avant chargement et après Halted pour la pression et la libération globale. Le runner borné a aussi un test de reprise après erreur transitoire du cœur. Les tests couvrent le refus d'une pression, d'un relâchement individuel, d'une libération globale partielle (les commandes déjà libérées restent libérées), d'une réinitialisation et d'un rechargement invalide depuis les états en pause, arrêté et maintenant Halted; ils ne couvrent pas encore toutes les catégories d'erreur. Un test supplémentaire vérifie désormais qu'un échec de réinitialisation conserve exactement l'état Running, Paused ou Halted.
- [ ] Ajouter des tests de séquences longues : charger, démarrer, mettre en pause, reprendre, réinitialiser, arrêter et recharger. Un premier test couvre maintenant charger, pause/reprise répétées, exécution bornée, réinitialisation depuis l'état en pause et rechargement; la sémantique d'arrêt reste à définir.
- [ ] Clarifier la sémantique d'arrêt et de réinitialisation pour chaque état, sans hypothèses spécifiques à une console.

**Critère de sortie :** chaque opération publique possède des préconditions, des effets et une politique d'erreur testés; aucune erreur ne laisse la session dans un état non documenté.

### P0.2 — Contrats et isolation des modules
- [x] Identifiants système stables, registre et vérifications de cohérence des descripteurs.
- [x] Déclarations de capacités par module.
- [x] Refuser le chargement de contenu quand un cœur ne déclare pas la capacité `content_loading`; test de régression sur les API directe et bornée.
- [x] Planification de dépendances et détection des dépendances manquantes/cycles; le manifeste rejette aussi les dépendances directes dupliquées avec une erreur de régression. Les parcours du manifeste et du catalogue de modules utilisent un DFS itératif à pile explicite, et le plan d'installation construit aussi son ordre sans récursion. La validation du catalogue indexe désormais les ID et dépendances avec des tables de hachage au lieu de rescanner toute la liste pour chaque entrée. Des tests couvrent une chaîne de 2 048 modules avec ordre d'installation dépendances-d'abord et un cycle profond de 1 024 modules dans chaque validateur.
- [x] Renforcer la validation structurelle des URL d'artefacts HTTPS : autorités et hôtes vides/malformés, identifiants utilisateur, antislashs, labels invalides, ports non numériques/hors plage et IPv6 entre crochets; des tests couvrent aussi les hôtes/ports valides. Ce contrôle dédié ne remplace pas un parseur d'URL complet.
- [x] Définir la version du contrat API et une règle de compatibilité : même version majeure et version mineure requise inférieure ou égale à celle de l'hôte. Le runtime refuse la création de session pour un cœur incompatible. Une matrice de tests couvre aussi la règle de compatibilité majeur/mineur. Le registre applique maintenant aussi cette vérification à toutes les voies de création, y compris le pipeline de lancement par détection; tests sur création directe et création depuis une détection.
- [ ] Définir les métadonnées minimales du module : ID, version, version de contrat, capacités, provenance et exigences.
- [ ] Tester deux modules factices dans le même processus et vérifier l'absence de partage involontaire d'état.
- [ ] Définir les limites de confiance : un module tiers n'est pas sûr simplement parce que son manifeste est valide.
- [ ] Documenter la stratégie future pour les modules natifs et les éventuels modules hors processus.

**Critère de sortie :** l'hôte peut découvrir, vérifier et instancier un module compatible sans dépendre de son implémentation interne.

### P0.3 — Chargement et validation du contenu
- [x] Introduire un premier modèle générique de contenu possédé, avec limite de taille explicite et erreurs communes (les empreintes et la détection de format restent à faire).
- [x] Rejeter les contenus vides et ceux dépassant la limite explicitement fournie, avant appel au cœur. L'API de session `load_content_bounded` valide une tranche empruntée avant d'allouer une copie et laisse la session vide en cas de rejet.
- [x] Lire un flux avec une limite stricte (au plus limite + 1 octet pour détecter le dépassement), sans exposer un contenu partiellement lu en cas d’erreur ; les tests vérifient aussi que la source n'est pas consommée au-delà de cet octet, y compris avec une limite de zéro.
- [x] Ajouter un helper dédié aux chemins/fichiers qui réutilise la même lecture bornée.
- [x] Traiter les ROM Game Boy ROM-only tronquées : vérifier la taille déclarée avant de remplacer le contenu déjà chargé, avec test de régression.
- [x] Rejeter un logo Nintendo incorrect et un checksum d'en-tête invalide sans remplacer la cartouche précédemment chargée ; les tests vérifient que l'ancienne cartouche reste exécutable.
- [ ] Étendre les validateurs structurels et les tests de troncature aux autres formats ; le cœur Game Boy expose maintenant une sonde branchée sur le contrat générique qui vérifie le logo et la longueur minimale, sans prétendre valider le checksum ou la ROM entière.
- [x] Introduire un contrat de détection générique séparé de la validation structurelle et de l'acceptation par le cœur; les sondes examinent les octets, peuvent demander un préfixe plus long et peuvent signaler plusieurs candidats sans les déclarer valides.
- [x] La sonde Game Boy est utilisable via le détecteur générique après l'application des limites communes; la détection reste informative et ne remplace pas la validation par le cœur.
- [x] La détection générique reçoit uniquement un préfixe d'octets et des sondes explicites; elle ne consulte ni extension ni chemin et ne valide pas elle-même le contenu.
- [x] Relier détection, sélection d'un cœur enregistré et chargement borné dans un pipeline de lancement unique; les tests couvrent lancement réussi, limite dépassée, détection ambiguë, préfixe incomplet et refus du contenu par le cœur.
- [ ] Étendre les tests aux tailles limites et aux fichiers tronqués; la détection couvre maintenant les signatures partielles, les octets arbitraires et les candidats ambigus. La lecture de fichier teste aussi le dépassement exact de la limite d'un octet. Le chargement borné vérifie aussi qu'un dépassement ne modifie ni l'état en pause avec entrée maintenue, ni l'état Halted.
- [x] Ajouter au registre un point d'entrée qui crée le cœur correspondant à un résultat de détection unique, en refusant l'absence de candidat, les résultats ambigus, les IDs invalides et les sondes encore indécises; le chargement par le cœur reste obligatoire pour valider réellement le contenu.
- [x] Ajouter un pipeline de lancement générique qui borne et possède le contenu, exécute les sondes sur ces octets, refuse les détections absentes/ambiguës/incomplètes, crée le cœur du registre et retourne une session Running uniquement après acceptation du contenu par le cœur. Les tests couvrent le lancement réussi, la limite de taille et l'ambiguïté.
- [x] Tester les erreurs de validation du cœur et les sondes incomplètes dans le pipeline de lancement.
- [x] Tester le refus d'un identifiant de cœur détecté mais non enregistré dans le pipeline de lancement.
- [ ] Définir l'intégration du pipeline dans une application hôte.
- [ ] Ne pas exiger que le framework comprenne lui-même les formats propriétaires de chaque système.

**Critère de sortie :** le framework rejette proprement les contenus invalides et fournit au module un contenu borné avec des métadonnées fiables.

## P1 — construire le chemin complet de lancement

### P1.1 — Orchestrateur et multi-sessions
- [ ] Créer une couche hôte qui relie catalogue, registre, chargement de contenu et sessions.
- [ ] Distinguer identifiant de session, système sélectionné et identité du contenu.
- [ ] Permettre la création et la fermeture de plusieurs sessions indépendantes.
- [ ] Définir les limites de ressources et la politique de concurrence avant tout parallélisme.
- [ ] Ajouter des tests d'isolation, d'échec de lancement et de fermeture répétée.

### P1.2 — Entrées, vidéo, audio et cadence
- [ ] Stabiliser des contrats d'entrée génériques, y compris la perte de focus et la libération de toutes les commandes.
- [ ] Définir des formats de trames vidéo avec dimensions, stride, format de pixel et durée de vie du tampon explicites.
- [ ] Définir des blocs audio avec fréquence, canaux, format d'échantillon et règles de propriété explicites.
- [ ] Séparer horloge/cadence d'émulation et horloge de présentation hôte.
- [ ] Gérer pause, reprise, arrêt, retard de l'hôte et files pleines sans blocage indéfini.
- [ ] Tester tailles invalides, buffers vides, cadence irrégulière et back-pressure. Les tests couvrent maintenant le dépassement de taille vidéo en arithmétique et le tampon audio vide à zéro frame.
- [ ] Conserver les interfaces sans dépendance obligatoire à une bibliothèque graphique ou audio unique.

### P1.3 — Sauvegardes et restauration
- [ ] Distinguer sauvegarde persistante du jeu, configuration et save state de l'émulateur.
- [ ] Définir un conteneur de save state versionné, avec ID/version du cœur, version de format, taille et intégrité.
- [ ] Valider les métadonnées et limites avant d'appeler le cœur.
- [ ] Écrire de façon atomique et prévoir récupération après interruption.
- [ ] Documenter que chaque cœur reste responsable de la validation de son état interne avant mutation.
- [ ] Tester état corrompu, version inconnue, fichier trop grand, restauration échouée et reprise après crash.

### P1.4 — Première application hôte minimale
- [ ] Ajouter une interface simple pour sélectionner un contenu et un module, puis lancer/mettre en pause/arrêter.
- [ ] Afficher les erreurs, capacités indisponibles et informations de diagnostic sans planter.
- [ ] Ne pas créer de menus ou d'améliorations propres à une console avant que le chemin de lancement générique fonctionne.
- [ ] Séparer les réglages d'interface des réglages de matériel émulé.

**Critère de sortie P1 :** un test d'intégration peut sélectionner un module, valider un contenu de test, démarrer une session, échanger des entrées et des sorties simulées, sauvegarder/restaurer, puis fermer proprement le tout.

## P2 — sécurité, confiance et diagnostic

- [x] Appliquer aussi à l'écriture la limite de 4 Mio du stockage d'état; tests de rejet avant écriture et de fichier persistant trop volumineux.
- [x] Préserver la sauvegarde valide lorsqu'il ne reste plus de fichier primaire, jusqu'à la création réussie du nouveau fichier; test de régression.

- [ ] Compléter le traitement des manifestes et la validation des métadonnées avant toute installation.
- [ ] Utiliser une implémentation cryptographique maintenue et auditée; ne pas inventer de cryptographie.
- [ ] Vérifier la signature et la provenance avant de faire confiance au contenu d'un catalogue ou d'un paquet.
- [ ] Rendre les mises à jour de catalogue résistantes au retour vers une ancienne révision, avec persistance durable.
- [ ] Installer dans une zone temporaire, vérifier les fichiers, puis activer atomiquement; prévoir rollback.
- [ ] Définir les permissions, les chemins autorisés et la politique de liens symboliques.
- [ ] Produire des diagnostics structurés avec contexte et causes, sans exposer les données sensibles.
- [ ] Ajouter tests de fuzzing ciblés pour les parseurs et formats exposés à des fichiers non fiables.
- [ ] Vérifier dépendances, licences, provenance et contenu des paquets dans la CI.

**Critère de sortie :** les opérations sur les contenus et modules non fiables sont bornées, traçables et récupérables; les contrôles de sécurité sont démontrés par des tests.

## P3 — qualité et performance mesurées

- [ ] Ajouter des benchmarks de référence pour le chargement, les changements d'état, le transfert de trames et les files audio.
- [ ] Mesurer latence d'entrée, temps de lancement, mémoire et stabilité sur des charges définies.
- [ ] Ajouter des tests de longue durée et des tests de non-régression.
- [ ] Définir des budgets de mémoire et de file pour éviter une croissance non bornée.
- [ ] Optimiser uniquement après profilage, en conservant des résultats reproductibles.

## P4 — ajouter des systèmes sans casser le framework

- [ ] Garder chaque cœur dans un crate indépendant qui dépend du contrat commun, pas du frontend.
- [ ] Créer une suite de conformité réutilisable pour tous les cœurs.
- [ ] Utiliser un cœur factice de test pour valider le framework sans dépendre d'une console réelle.
- [ ] Garder Game Boy comme cible de validation, pas comme centre de la roadmap actuelle.
- [ ] Ne déclarer aucun système pris en charge sur la seule base de la compilation : exiger des tests de comportement et des preuves reproductibles.
- [ ] Ajouter d'autres systèmes lorsque les interfaces, l'I/O et le pipeline de lancement sont assez stables.

## P5 — interface utilisateur et publication

- [ ] Ajouter bibliothèque de contenus, paramètres et rapports de diagnostic dans l'application hôte.
- [ ] Pour les systèmes ayant une interface logicielle, évaluer séparément la faisabilité d'une exécution légale, d'une recréation fidèle et de thèmes modernisés.
- [ ] Garder les améliorations visuelles optionnelles, réversibles et séparées du comportement matériel.
- [ ] Vérifier les licences, les notices, les ressources fournies par l'utilisateur et les restrictions de redistribution.
- [ ] Construire des paquets reproductibles par plateforme, avec manifeste, empreintes et procédure de mise à jour/retour arrière.
- [ ] Documenter les systèmes réellement pris en charge, les limites connues et la procédure de rapport de bugs.

## Règles de priorité

1. **Le framework global passe avant l'avancement d'un cœur spécifique.**
2. Corriger d'abord les régressions du build/CI et les erreurs de contrat.
3. Favoriser les fonctionnalités génériques réutilisables et les tests de conformité.
4. N'ajouter aucune dépendance lourde sans besoin concret et revue de licence/sécurité.
5. Ne jamais confondre code présent, test réussi, compatibilité théorique et émulation jouable.
6. Tous les changements du projet restent sur la branche `ArchiaEmu`; ne pas modifier `main`.

## Vérification CI

Commandes de référence depuis `ArchiaEmu/` :

```sh
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked
```

La CI du commit `75299439a4b171879499de392f30d9483fe19b58` a réussi sur Linux, Windows et macOS. Cela confirme les contrôles configurés sur ce commit, mais ne constitue pas une preuve d'émulation complète.
