# ArchiaEmu — roadmap du framework global

Cette feuille de route priorise le **socle commun de l'émulateur**, pas le développement d'une console en particulier. Les cœurs de console servent à vérifier que les contrats communs sont réellement utilisables, mais ne doivent pas dicter l'architecture ni monopoliser les prochaines étapes.

Les pourcentages sont des estimations d'avancement fonctionnel, fondées sur le code présent et les tests vérifiés. Ils ne représentent ni le temps passé ni une promesse de compatibilité. Une phase n'est terminée que lorsque ses critères d'acceptation sont démontrés par des tests reproductibles.

## Priorités et état actuel

| Priorité | Domaine | État estimé | Résultat attendu |
| --- | --- | ---: | --- |
| **P0** | Fiabilité du runtime commun | 100 % | Contrats cohérents, cycle de vie sûr, erreurs récupérables, tests de régression |
| **P0** | Chargement et validation du contenu | 100 % | Pipeline commun borné, validation de tous les formats actuellement enregistrés et contrat d'intégration hôte |
| **P0** | Contrats de modules et capacités | 100 % | Modules isolés dans le processus, capacités explicites, découverte et création prévisibles |
| **P1** | Orchestration multi-sessions | 70 % | Hôte capable de gérer plusieurs sessions indépendantes sans état partagé accidentel |
| **P1** | Entrées, sorties et horloge hôte | 63 % | Contrats génériques d'entrée, vidéo, audio, cadence et arrêt propre |
| **P1** | Sauvegardes et restauration | 55 % | Stockage persistant robuste et formats de sauvegarde validés/versionnés |
| **P1** | Frontend minimal du framework | 59 % | Parcourir le contenu, sélectionner un module, lancer/arrêter et afficher les erreurs |
| **P2** | Sécurité et distribution des modules | 49 % | Frontières de confiance, provenance, installation atomique et récupération |
| **P2** | Diagnostics, tests et compatibilité | 35 % | Rapports exploitables, tests de conformité communs et matrice CI |
| **P3** | Performance et stabilité | 10 % | Benchmarks reproductibles et optimisation guidée par les mesures |
| **P4** | Extension du catalogue de systèmes | En attente | Ajouter des cœurs seulement après stabilisation des interfaces partagées |
| **P5** | Préparation de versions | 10 % | Paquets reproductibles, politique de support et critères de publication |

> **Point de départ réel :** le workspace Rust, le contrat de cœur, le registre, une partie du cycle de vie de session, les capacités, l'état d'entrée partagé, les fonctions de sauvegarde/restauration déléguées au cœur, la gestion de modules et le CI existent déjà. Il n'existe pas encore d'application complète permettant de charger un jeu et de l'émuler avec audio et vidéo de bout en bout. La réussite du CI vérifie les contrôles configurés, pas la compatibilité d'une console.

> **P0 clôturé pour le périmètre actuel :** les critères de sortie du runtime, du registre de modules et du pipeline de contenu sont couverts par des tests et les quality gates Linux/Windows/macOS. Cela ne revendique ni le sandboxing de code natif ni la prise en charge de formats absents du registre; tout nouveau format doit fournir ses propres validateurs.

## Vérification transversale P0 — persistance du state store

- [x] Supprimer les fichiers temporaires lorsqu'une suppression de sauvegarde ou un renommage du remplacement échoue; les tests vérifient la préservation du primaire et l'absence de fichier temporaire orphelin. Un test via le chemin réel de sauvegarde injecte l'échec du renommage d'installation, vérifie la restauration du primaire et l'absence du temporaire après retour d'erreur.
- [x] Tester la récupération depuis une sauvegarde valide lorsque le fichier primaire dépasse la limite de 4 Mio; le test vérifie que l'état de secours est retourné et que le fichier primaire rejeté n'est pas supprimé silencieusement.
- [x] Le décodeur d'état indexe les identifiants sélectionnés et installés avec des ensembles de hachage pour détecter les doublons sans rescanner les enregistrements déjà lus, tout en conservant l'ordre d'encodage et les erreurs de format dédiées.
- [x] Vérifier les garanties de remplacement et de synchronisation sur les plateformes CI prises en charge. Les tests couvrent l'échec du renommage temporaire, la restauration réussie, l'échec de restauration avec backup récupérable, le nettoyage du temporaire via le chemin réel de `save` et l'erreur de synchronisation injectée. Le chemin Windows utilise un petit crate isolé qui appelle `MoveFileExW` avec `MOVEFILE_WRITE_THROUGH` pour les renommages du primaire, du backup, du temporaire et de la restauration; le cœur reste sans `unsafe`. Unix conserve la synchronisation explicite du répertoire parent. Les quality gates Rust ont réussi sur Linux, Windows et macOS pour le commit `c49a0ca786dbc01849383f27e825054ca6de3d9b`; ils ne simulent pas les coupures d'alimentation.

## P0 — rendre le runtime commun fiable

### P0.3 — Chargement et validation du contenu
- [x] Le détecteur de formats normalise les réponses `NeedMoreData` incohérentes des probes : une probe ne peut plus demander « plus de données » tout en indiquant une longueur déjà couverte par le préfixe. Un test vérifie les longueurs nulles, trop courtes et égales au préfixe.
- [x] La lecture depuis un chemin vérifie d'abord la taille annoncée par les métadonnées pour rejeter rapidement les fichiers manifestement trop volumineux; la lecture en flux reste bornée à la limite + 1 octet afin de couvrir les métadonnées obsolètes et les fichiers qui grandissent pendant la lecture.
- [x] Exposer le pipeline de lancement borné pour les entrées réelles : `launch_content_path` lit un fichier sous limite, `launch_content_reader` lit un flux sous limite, puis les deux réutilisent la même détection et la même validation par le cœur que `launch_content`.
- [x] Couvrir le lancement borné depuis chemin et flux : détection réussie, dépassement de limite, détection ambiguë, rejet par le cœur, signature tronquée et contenu vide. Les tests vérifient que les erreurs sont propagées au lieu de sélectionner arbitrairement un format ou de démarrer une session invalide.
- [x] Vérifier la propagation des erreurs de lecture des sources : chemin absent (NotFound) et flux en échec (PermissionDenied), sans exposer de contenu partiellement lu.
- [x] Exécuter et valider la couverture courante sur Linux, Windows et macOS : les quality gates Rust ont réussi pour le commit `dc7d04c908a34164e672097b01dedf14e81c2ee5`. Le Game Boy est le seul format enregistré aujourd'hui; tout nouveau format doit apporter ses propres validateurs et tests de troncature avant d'être considéré comme pris en charge.


### P0.1 — Cycle de vie de session
- [x] Contrat de cœur commun et erreurs explicites.
- [x] États de session et contrôles d'opérations selon l'état.
- [x] Capacités déclarées et refus des opérations non annoncées.
- [x] État d'entrée commun avec suppression des transitions dupliquées et relâchement explicite; la libération globale vérifie aussi la capacité d'entrée avant d'appeler le cœur.
- [x] API générique de sauvegarde/restauration d'état avec vérification de capacité.
- [x] Formaliser les transitions autorisées dans une table de référence dans `docs/SESSION_LIFECYCLE.md`; une matrice table-driven teste maintenant pause, reprise, exécution simple et exécution bornée dans chacun des cinq états, avec vérification du résultat et de l'état après l'opération. Les tests ciblés couvrent également réinitialisation, budget nul, arrêt sur `Halted` et les erreurs de cœur.
- [x] Garantir un comportement documenté après chaque erreur du cœur : état de session, entrées maintenues, événements et possibilité de reprise. `last_core_error()` conserve les erreurs du cœur des opérations mutables et `clear_last_core_error()` efface uniquement le diagnostic. Les tests couvrent désormais les cinq variantes de `CoreError` (`InvalidContent`, `NotInitialized`, `Unsupported`, `InvalidState`, `Execution`), ainsi que les erreurs de sauvegarde/restauration audio-vidéo, d'entrées, de réinitialisation, d'arrêt, de rechargement et d'exécution bornée. Les tests vérifient la préservation de l'état, des entrées et la reprise selon le contrat. `docs/SESSION_LIFECYCLE.md` documente les erreurs du cœur et les erreurs de précondition qui ne remplacent pas le diagnostic.
- [x] Ajouter un test de séquence longue : charger, pause/reprise répétées, exécution bornée, réinitialisation, arrêt explicite puis rechargement et reprise. Un test séparé couvre l'échec de `stop` pendant la libération des entrées; avec deux contrôles maintenus, il vérifie que le premier est libéré, que le second reste maintenu, que le diagnostic est conservé et que la session ne passe pas à `Stopped`.
- [x] Clarifier la sémantique d'arrêt et de réinitialisation pour chaque état : `stop` arrête l'exécution hôte sans décharger le contenu, libère les entrées maintenues et ajoute l'état `Stopped`; `reset` ou le rechargement reprend la session.

**Critère de sortie :** chaque opération publique possède des préconditions, des effets et une politique d'erreur testés; aucune erreur ne laisse la session dans un état non documenté.

### P0.2 — Contrats et isolation des modules
- [x] Identifiants système stables, registre et vérifications de cohérence des descripteurs.
- [x] Déclarations de capacités par module.
- [x] Refuser le chargement de contenu quand un cœur ne déclare pas la capacité `content_loading`; test de régression sur les API directe et bornée.
- [x] Planification de dépendances et détection des dépendances manquantes/cycles; le manifeste rejette aussi les dépendances directes dupliquées avec une erreur de régression. Les parcours du manifeste et du catalogue de modules utilisent un DFS itératif à pile explicite, et le plan d'installation construit aussi son ordre sans récursion. La validation du catalogue indexe désormais les ID et dépendances avec des tables de hachage au lieu de rescanner toute la liste pour chaque entrée. Des tests couvrent une chaîne de 2 048 modules avec ordre d'installation dépendances-d'abord et un cycle profond de 1 024 modules dans chaque validateur.
- [x] La validation des manifestes construit une fois l'index des ID, puis utilise des ensembles pour détecter les dépendances et cibles dupliquées; les recherches d'existence des dépendances ne rescannent plus tout le catalogue pour chaque référence.
- [x] Le manifeste résout une sélection utilisateur en plan d'installation dépendances-d'abord, avec fermeture transitive, déduplication des dépendances partagées et refus explicite des sélections inconnues, non installables, incompatibles ou non prises en charge par la cible.
- [x] Renforcer la validation structurelle des URL d'artefacts HTTPS : autorités et hôtes vides/malformés, identifiants utilisateur, antislashs, labels invalides ou trop longs, longueur maximale du nom d'hôte, adresses IPv4 numériques malformées, ports non numériques/hors plage et IPv6 entre crochets; des tests couvrent aussi les hôtes/ports valides. Ce contrôle dédié ne remplace pas un parseur d'URL complet.
- [x] Définir la version du contrat API et une règle de compatibilité : même version majeure et version mineure requise inférieure ou égale à celle de l'hôte. Le runtime refuse la création de session pour un cœur incompatible. Une matrice de tests couvre aussi la règle de compatibilité majeur/mineur. Le registre applique maintenant aussi cette vérification à toutes les voies de création, y compris le pipeline de lancement par détection; tests sur création directe et création depuis une détection.
- [x] Pré-enregistrer les descripteurs de cœur sans instancier leur factory : register_with_descriptor refuse une version d'API incompatible avant création, registered_descriptor expose les capacités au host et create vérifie que l'instance correspond exactement aux métadonnées annoncées. Des tests couvrent l'absence d'appel de factory lors de la consultation, le refus précoce d'une API incompatible et les métadonnées divergentes; le cœur Game Boy utilise maintenant ce chemin.
- [x] Fournir aux interfaces la liste des modules compatibles avec la version de l'application et la cible hôte; les modules non installables sont filtrés sans masquer les erreurs de structure du manifeste.
- [x] Planifier la désinstallation en dépendances inverses, en refusant de retirer un module encore requis par un module installé et en exigeant que chaque module demandé soit réellement installé.
- [x] Mettre à jour l'état des modules par lots après activation/désinstallation : validation complète avant mutation, refus des ID dupliqués et suppression groupée des enregistrements pour éviter un état mémoire partiellement mis à jour.
- [x] Pré-vérifier le budget de téléchargement d'une installation complète : additionner les tailles déclarées de chaque paquet et de ses dépendances une seule fois, détecter les dépassements arithmétiques et refuser les plans dépassant la limite configurée avant tout téléchargement.
- [x] Calculer un plan de mise à jour sélectif : ne retourner que les modules absents ou dont la version diffère du catalogue, en conservant l'ordre dépendances-d'abord et en omettant les dépendances déjà à jour.
- [x] Ajouter un journal d'installation versionné et borné, avec transitions de phases contrôlées, sauvegarde atomique par fichier temporaire et action de récupération calculée à partir du dernier jalon durable.
- [x] Ajouter un coordinateur d'installation qui persiste le journal à chaque jalon, orchestre téléchargement/vérification, extraction, activation et enregistrement de l'état via un backend hôte. Un jalon `Activating` durable est écrit avant le changement de version afin que la récupération inspecte le répertoire final après une interruption plutôt que de supprimer aveuglément le staging.
- [x] Définir un contrat de métadonnées runtime : ID, version, version d'API, capacités, provenance éditeur, homepage optionnelle et dépendances/exigences hôte. La validation rejette les éditeurs invalides, les homepages non HTTPS, les dépendances auto-référencées/dupliquées et les exigences d'API incompatibles; la confiance cryptographique reste distincte de la métadonnée déclarative.
- [x] Ajouter un planificateur d'activation runtime : validation des descripteurs, détection des ID dupliqués, dépendances manquantes et cycles, vérification des capacités exigées par l'hôte et ordre d'activation dépendances-d'abord avec parcours itératif.
- [x] Tester deux modules factices distincts dans le même processus : le test de registre crée deux implémentations factices sous des ID différents, charge le premier sans initialiser le second, puis vérifie que les deux exécutent indépendamment.
- [x] Définir les limites de confiance : un manifeste valide prouve la conformité des métadonnées, pas l'innocuité du code. Les règles et limites actuelles sont documentées dans `docs/MODULE_TRUST_BOUNDARIES.md`; aucun sandboxing n'est revendiqué.
- [x] Documenter la stratégie future pour les modules natifs et les éventuels modules hors processus dans `docs/MODULE_TRUST_BOUNDARIES.md`, y compris les limites de la séparation en processus.

**Critère de sortie :** l'hôte peut découvrir, vérifier et instancier un module compatible sans dépendre de son implémentation interne.

### P0.3 — Chargement et validation du contenu
- [x] Introduire un premier modèle générique de contenu possédé, avec limite de taille explicite et erreurs communes (les empreintes et la détection de format restent à faire).
- [x] Rejeter les contenus vides et ceux dépassant la limite explicitement fournie, avant appel au cœur. L'API de session `load_content_bounded` valide une tranche empruntée avant d'allouer une copie et laisse la session vide en cas de rejet.
- [x] Lire un flux avec une limite stricte (au plus limite + 1 octet pour détecter le dépassement), sans exposer un contenu partiellement lu en cas d’erreur ; les tests vérifient aussi que la source n'est pas consommée au-delà de cet octet, y compris avec une limite de zéro.
- [x] Ajouter un helper dédié aux chemins/fichiers qui réutilise la même lecture bornée.
- [x] Traiter les ROM Game Boy ROM-only tronquées : vérifier la taille déclarée avant de remplacer le contenu déjà chargé, avec test de régression.
- [x] Rejeter un logo Nintendo incorrect et un checksum d'en-tête invalide sans remplacer la cartouche précédemment chargée ; les tests vérifient que l'ancienne cartouche reste exécutable.
- [x] Valider le code de taille RAM de l'en-tête Game Boy : accepter les codes définis `0x00` à `0x05`, rejeter les codes réservés avec une erreur dédiée et couvrir les deux chemins par tests.
- [x] Les validateurs structurels et tests de troncature couvrent tous les formats actuellement implémentés : le Game Boy est le seul format enregistré aujourd'hui, et sa longueur ROM déclarée est vérifiée pour tous les types de cartouche, y compris les mappers non exécutables. Tout nouveau format devra fournir son propre validateur et ses tests de troncature avant d'être considéré comme pris en charge. La sonde générique identifie un candidat sans prétendre valider le checksum ni la ROM entière.
- [x] Introduire un contrat de détection générique séparé de la validation structurelle et de l'acceptation par le cœur; les sondes examinent les octets, peuvent demander un préfixe plus long et peuvent signaler plusieurs candidats sans les déclarer valides.
- [x] La sonde Game Boy est utilisable via le détecteur générique après l'application des limites communes; la détection reste informative et ne remplace pas la validation par le cœur.
- [x] La détection générique reçoit uniquement un préfixe d'octets et des sondes explicites; elle ne consulte ni extension ni chemin et ne valide pas elle-même le contenu.
- [x] Relier détection, sélection d'un cœur enregistré et chargement borné dans un pipeline de lancement unique; les tests couvrent lancement réussi, limite dépassée, détection ambiguë, préfixe incomplet et refus du contenu par le cœur.
- [x] Étendre les tests aux tailles limites et aux fichiers tronqués du format Game Boy actuellement pris en charge : les tailles ROM standard `0x00`–`0x08` et spéciales `0x52`–`0x54` sont vérifiées contre leur taille attendue, les codes réservés sont rejetés et une ROM tronquée déclarant une taille spéciale ne remplace pas la cartouche active. La détection générique couvre aussi les signatures partielles, les octets arbitraires et les candidats ambigus; la lecture bornée teste le dépassement exact de la limite d'un octet. Les autres formats devront recevoir leurs propres validateurs et tests lorsqu'ils seront ajoutés.
- [x] Ajouter au registre un point d'entrée qui crée le cœur correspondant à un résultat de détection unique, en refusant l'absence de candidat, les résultats ambigus, les IDs invalides et les sondes encore indécises; le chargement par le cœur reste obligatoire pour valider réellement le contenu.
- [x] Ajouter un pipeline de lancement générique qui borne et possède le contenu, exécute les sondes sur ces octets, refuse les détections absentes/ambiguës/incomplètes, crée le cœur du registre et retourne une session Running uniquement après acceptation du contenu par le cœur. Les tests couvrent le lancement réussi, la limite de taille et l'ambiguïté.
- [x] Tester les erreurs de validation du cœur et les sondes incomplètes dans le pipeline de lancement.
- [x] Tester le refus d'un identifiant de cœur détecté mais non enregistré dans le pipeline de lancement.
- [x] Définir l'intégration du pipeline dans une application hôte dans docs/CONTENT_LAUNCH_INTEGRATION.md : registre de modules, sondes explicites, limites de contenu, choix de l'API de lancement et traitement des erreurs.
- [x] Le framework reste indépendant des formats propriétaires : la détection est assurée par les sondes et la validation structurelle finale par le cœur concerné, comme décrit dans docs/CONTENT_LAUNCH_INTEGRATION.md.

**Critère de sortie :** le framework rejette proprement les contenus invalides et fournit au module un contenu borné avec des métadonnées fiables.

## P1 — construire le chemin complet de lancement

### P1.1 — Orchestrateur et multi-sessions — 70 %
- [x] Ajouter un gestionnaire hôte reliant le registre, le pipeline de chargement et les sessions.
- [x] Distinguer l'identifiant hôte de session de l'identifiant de système; les IDs sont monotones et jamais réutilisés.
- [x] Permettre la création, l'accès et la fermeture de plusieurs sessions indépendantes.
- [x] Définir une limite de sessions configurable et imposer la sérialisation des mutations via l'accès exclusif `&mut`; aucun parallélisme implicite.
- [x] Tester le lancement réussi via détection/registre, l'échec sans session fantôme, la fermeture répétée et l'épuisement des IDs sans bouclage.
- [x] Ajouter un lancement direct depuis un chemin, avec contrôle anticipé de capacité et lecture bornée; tester le chargement depuis disque.

L'implémentation initiale se trouve dans `crates/archiaemu-core/src/host_runtime.rs`; l'identité de contenu persistante et le catalogue hôte restent à intégrer.

### P1.2 — Entrées, vidéo, audio et cadence — 63 %
- [x] Contrats d'entrée génériques avec transitions dédupliquées et libération de toutes les commandes à la perte de focus/déconnexion.
- [x] Trames vidéo avec dimensions, stride explicite, format de pixel et propriété du tampon; lignes rembourrées et tailles incohérentes testées.
- [x] Blocs audio PCM signés 16 bits entrelacés avec fréquence, canaux et nombre de frames validés.
- [x] Accumulateur de cadence à fréquence fixe indépendant de l'acquisition de l'horloge hôte; temps fractionnaire conservé et rattrapage borné après retard.
- [x] Relier la cadence au cycle de vie des sessions : la session n'avance la cadence qu'à l'état Running et réinitialise le temps accumulé dans les états pause/stop/empty/halted, évitant un rattrapage de temps suspendu.
- [x] Définir des files média bornées : la vidéo peut évincer la plus ancienne trame pour limiter la latence; l'audio refuse le nouvel élément quand la file est pleine afin que l'hôte traite explicitement la back-pressure. Tests de capacité, d'ordre et de conservation des données.
- [x] Tests des tailles invalides, du débordement de taille vidéo, du tampon audio incomplet/vide et des retards de cadence.
- [x] Interfaces indépendantes d'une bibliothèque graphique ou audio particulière.
- [x] Tester le transfert des trames vidéo et blocs audio via une session : payload remis une seule fois, lecture avant chargement refusée et capacités média non déclarées signalées explicitement.

### P1.3 — Sauvegardes et restauration — 55 %
- [x] Distinguer sauvegarde persistante du jeu, configuration et save state de l'émulateur dans `docs/SAVE_STATE_POLICY.md`; le frontend propose `save-state <path>` et `load-state <path>` et respecte la capacité déclarée par le cœur.
- [x] Définir un conteneur binaire versionné avec ID/version du cœur, taille bornée, longueur exacte et contrôle CRC-32 d'intégrité (détection de corruption, pas authentification cryptographique).
- [x] Valider les métadonnées et limites avant de fournir le payload au cœur; refuser les versions de format inconnues, systèmes/versions incompatibles, données tronquées ou excédentaires et checksums invalides.
- [x] Écrire de façon atomique via fichier temporaire synchronisé et conserver une sauvegarde de récupération; au chargement, le store peut reprendre depuis le backup si le fichier principal est absent ou invalide.
- [x] Documenter que chaque cœur reste responsable de la validation de son état interne avant mutation dans `docs/CORE_INTERFACE.md`.
- [x] Tester corruption, version inconnue, payload vide/trop grand, incompatibilité de système/cœur, troncature et données excédentaires.
- [x] Tester les fichiers persistés trop grands, le round-trip disque et la récupération depuis un backup lorsque le fichier principal est corrompu ou absent.
- [x] Injecter les échecs de renommage et vérifier la restauration du primaire, le backup récupérable et le nettoyage du temporaire sur les plateformes CI prises en charge.
- [ ] Simuler une interruption brutale du processus ou une coupure d'alimentation à chaque jalon durable; la CI ne simule pas une perte d'alimentation réelle.

### P1.4 — Première application hôte minimale — 57 %

Une première application terminale est en place : `archiaemu --list-systems` expose les systèmes/capacités, `--system` sélectionne explicitement le module, et le REPL permet `open <chemin>`, `sessions`, `switch <id>` et `close [id]` pour gérer plusieurs sessions. `save-state <path>` et `load-state <path>` réutilisent l'enveloppe versionnée et le stockage persistant commun, en refusant les cœurs qui n'annoncent pas cette capacité. Les erreurs de chargement et de commandes sont affichées sans fermer immédiatement l'interface. Les commandes `input <control> <press|release>` et `release-inputs` transmettent les transitions sémantiques au cœur actif et récupèrent les erreurs de capacité sans quitter le REPL. La commande `media` interroge au plus une trame vidéo et un tampon audio disponibles, puis affiche leurs métadonnées; elle ne fait pas encore de rendu graphique ni de lecture audio. Le cœur Game Boy ne déclare pas ces capacités.
- [x] Ajouter une interface terminale pour sélectionner un module, ouvrir plusieurs contenus, basculer entre sessions, fermer une session et lancer/mettre en pause/arrêter.
- [x] Afficher les erreurs de chargement et de commandes, les capacités indisponibles et les états de session sans interrompre la boucle pour une erreur de commande.
- [x] Exposer les transitions d'entrée sémantiques (`input ... press|release`) et la libération globale (`release-inputs`) depuis le REPL; les cœurs sans capacité d'entrée retournent une erreur explicite.
- [x] Exposer un chemin de diagnostic pour récupérer une trame et un tampon audio annoncés par le cœur (`media`), avec dimensions, stride, format, fréquence, canaux et nombre d'échantillons; ce n'est pas encore un backend de rendu/lecture.
- [x] Exposer `run <events>` pour exécuter un budget explicite de 1 à 10 000 appels d'événement, retourner le nombre réellement exécuté, le dernier événement et indiquer si le budget a été épuisé; ajouter des tests des limites de budget. Cela ne remplace pas encore une horloge temps réel.
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
- [x] Ajouter le staging borné des artefacts : copie en flux avec taille exacte, fichier temporaire exclusif, synchronisation disque, vérification SHA-256 fournie par l'hôte et renommage seulement après validation. Le téléchargement réseau, le vérificateur cryptographique concret, l'extraction et l'activation/rollback restent à intégrer.
- [x] Ajouter une validation portable des chemins d'entrées d'archives avant extraction : rejet des chemins absolus, traversées `.`/`..`, séparateurs ambigus, caractères de contrôle, flux NTFS et noms réservés Windows. L'extracteur doit encore gérer les liens symboliques et garantir la résolution sous une racine de confiance.
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
