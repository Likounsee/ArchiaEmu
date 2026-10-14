# ArchiaEmu — roadmap orientée application d'abord

## Direction du projet

**Ordre de priorité non négociable : rendre d'abord l'application ArchiaEmu réellement utilisable, puis développer et intégrer les modules de systèmes/console.** Pour la première étape, on travaille sur l'application générique, son interface, ses parcours utilisateur, son architecture frontend/backend, sa bibliothèque, ses sessions, ses paramètres, ses diagnostics, sa stabilité et sa distribution. On ne développe pas de fonctionnalités matérielles propres à une console pendant cette phase, sauf cœur factice ou adaptateur minimal nécessaire pour tester un contrat commun.

Les fondations du runtime et des modules déjà présentes sont conservées et corrigées si nécessaire, mais leur présence ne signifie pas qu'il faut continuer à enrichir le système de modules avant que l'application soit utilisable. Le terminal reste un outil de diagnostic; il ne remplace pas l'application desktop.

Les pourcentages sont des **estimations fonctionnelles** fondées sur les critères observables, pas sur le temps passé. Les jalons se recouvrent et ne doivent pas être moyennés mécaniquement. Le code écrit ou un test avec un cœur factice ne prouve ni le succès de la CI ni la compatibilité d'une console. Une case n'est cochée qu'après vérification du critère décrit.

## Ordre général des priorités

| Ordre | Phase | État estimé | Objectif |
| --- | --- | ---: | --- |
| **P0** | Fondations déjà présentes | Majoritairement terminé | Garder le runtime, les contrats communs et le pipeline de contenu suffisamment stables pour construire l'application |
| **P1** | **Application desktop utilisable et installable — priorité actuelle** | **≈ 56 %** | Une application locale réellement utilisable, avec parcours fiables, paramètres, diagnostics, dépendances embarquées et installateur Windows .exe |
| **P1.1** | Architecture frontend/backend de l'application | En cours | UI séparée de l'orchestration et de l'exécution, modules d'interface clairs, tests isolés |
| **P1.2** | Parcours de bibliothèque et lancement | En cours | Importer, rechercher, sélectionner, valider, lancer et gérer les erreurs sans état incohérent |
| **P1.3** | Sessions et commandes génériques | En cours | Gérer plusieurs sessions, pause/reprise/arrêt/réinitialisation et fermeture propre |
| **P1.4** | Paramètres, diagnostics et ergonomie | À faire en partie | Préférences fiables, états vides/erreurs explicites, diagnostics exploitables |
| **P1.5** | Sorties média hôte et validation multiplateforme | À faire en partie | Afficher les trames et jouer l'audio lorsqu'un cœur le fournit, tester les adaptateurs et garantir les quality gates |
| **P2** | Intégration et fiabilité des modules | Après l'application utilisable | Découverte, installation, mise à jour, provenance, récupération et frontières de confiance |
| **P3** | Premier module de console réellement fonctionnel | Après P1 | Un système testé de bout en bout, avec limites de compatibilité honnêtes |
| **P4** | Autres modules/systèmes | Après stabilisation de P3 | Étendre le catalogue sans coupler les cœurs à l'application |
| **P5** | Publication et maintenance | En dernier | Paquets reproductibles, documentation de support, mises à jour et politique de version |

> **Définition stricte de fin de P1 :** P1 n'est terminé que si l'application est utilisable en local et livrable à un utilisateur qui n'a pas configuré l'environnement de développement. Le code nécessaire à l'application générique doit déjà être implémenté; les phases suivantes peuvent ajouter des modules/systèmes et leurs capacités, mais ne doivent pas servir à compléter les mécanismes essentiels de l'application. Il faut une interface cohérente, une bibliothèque et des sessions fiables, des préférences persistantes, des diagnostics, des sorties média honnêtes, des tests et une CI validés, ainsi qu'un installateur Windows .exe qui installe tous les fichiers et dépendances requis dans des emplacements locaux définis. Après installation, l'application doit démarrer et fonctionner localement sans dépendre du dépôt Git, du dossier de build ou d'outils Rust. L'installation, le premier démarrage, les chemins de données/configuration, la désinstallation et les erreurs d'installation doivent être vérifiés. Cela ne signifie pas encore qu'une console est jouable : aucun cœur factice ne doit être présenté comme un système fonctionnel.

## P0 — préserver les fondations communes

Le runtime commun, le cycle de vie des sessions, les capacités, le registre, le pipeline de chargement borné, les sauvegardes versionnées et une partie de la gestion des modules existent déjà. Cette phase est en maintenance : corriger les régressions et les contrats qui bloquent l'application, sans en faire la priorité fonctionnelle devant P1.

- [x] Contrat de cœur commun, états de session et erreurs explicites.
- [x] Capacités déclarées et refus des opérations non prises en charge.
- [x] Cycle de vie testé : pause, reprise, exécution bornée, arrêt, réinitialisation et erreurs de cœur.
- [x] Chargement de contenu borné depuis mémoire, chemin et flux, avec propagation des erreurs.
- [x] Détection générique séparée de la validation par le cœur et du lancement.
- [x] API générique de sauvegarde/restauration versionnée, avec stockage persistant et tests de récupération.
- [x] Contrats de modules, vérification de compatibilité d'API et planification des dépendances de base.
- [x] Pipeline d'intégration générique testé avec un cœur factice.

**Règle :** toute correction indispensable à l'application peut être faite ici; les nouvelles fonctionnalités avancées de gestion de modules attendent P2.

## P1 — rendre l'application utilisable (priorité actuelle)

### P1.1 — architecture de l'application et séparation frontend/backend

**État estimé : en cours — environ 45 % pour l'objectif global P1, estimation qualitative.**

Le desktop utilise eframe/egui, possède une bibliothèque et un contrôleur d'application partagé. Les adaptateurs de vue, les commandes typées du contrôleur, l'avancement des sessions via `ApplicationController::advance_session` et les préférences dans `preferences.rs` ont commencé à séparer l'interface de l'exécution.

- [x] Documenter le choix eframe/egui et ses critères multiplateformes.
- [x] Extraire les modèles d'application dans une bibliothèque Rust réutilisable et introduire `ApplicationController`.
- [x] Isoler les adaptateurs desktop pour les entrées et la présentation des trames.
- [x] Ajouter des commandes typées de session/entrée au contrôleur.
- [x] Déplacer l'avancement des sessions et la collecte bornée des médias derrière `ApplicationController::advance_session`.
- [x] Extraire la persistance des préférences dans `preferences.rs`.
- [x] Découper le rendu des pages hors de `main.rs` : accueil, bibliothèque, jeu, sessions, systèmes et paramètres ont désormais chacune leur module UI (`*_page.rs`). La navigation et la composition de l'application restent dans `main.rs`.
- [x] Première adaptation responsive du desktop : navigation latérale remplacée par une barre horizontale défilante sous 980 px, cartes d'accueil adaptées à la largeur, barres d'actions repliables et taille minimale de fenêtre abaissée à 640 × 480.
- [ ] Vérifier visuellement les pages à plusieurs tailles de fenêtre et corriger les éventuels débordements restants; les dispositions adaptatives et la navigation clavier existent, mais la vérification visuelle manuelle n'a pas été exécutée.
- [x] Retirer les accès directs au runtime depuis les vues pour les lectures de sessions; exposer au contrôleur le nombre de sessions et des résumés de session stables. Les commandes de cycle de vie restent aussi derrière le contrôleur.
- [ ] Unifier les états de vue chargement/succès/absence/capacité indisponible/erreur dans un modèle typé commun. Les pages affichent déjà des états vides, indisponibilités, validations en attente et erreurs; la migration complète du système de messages reste à faire.
- [x] Ajouter une première série de tests ciblés du contrôleur pour les transitions invalides, les identifiants fermés, les lancements rejetés et la cohérence de la sélection; les tests de frontend et les autres états de vue restent à faire.
- [ ] Valider les changements avec `cargo fmt --all -- --check`, `cargo test --workspace --locked`, `cargo clippy --workspace --all-targets --locked -- -D warnings` et la CI multiplateforme.

**Critère de sortie :** l'interface orchestre l'application par des API explicites; le code UI ne pilote pas directement l'exécution du cœur et les comportements essentiels sont testables séparément.

### P1.2 — bibliothèque de contenus et lancement fiable

- [x] Afficher la bibliothèque, les systèmes enregistrés et leurs capacités déclarées.
- [x] Importer plusieurs fichiers, afficher les imports rejetés, rechercher titre/chemin, filtrer et trier les entrées.
- [x] Sélectionner un contenu, lancer via le pipeline commun et afficher les erreurs de validation/lancement.
- [x] Supprimer une entrée avec désélection cohérente.
- [x] Persister la bibliothèque entre les redémarrages via les préférences, revalider chaque chemin au chargement et signaler les fichiers absents/invalides avec quelques détails pour permettre leur réimport. Lors d'un import (champ de chemin ou boîte de dialogue multiple), ne pas masquer un échec de sauvegarde : indiquer que les imports restent actifs pour la session mais ne seront pas persistés.
- [x] Enrichir le modèle de bibliothèque avec le chemin source, le nom affiché, la détection réelle par sondes sur les octets (distincte de l'indice d'extension), un indice de système compatible, le dernier résultat de lancement et une disponibilité vérifiée en direct. Les détections ambiguës/incomplètes restent explicites et la validation du cœur reste une étape distincte.
- [x] Prévoir des états vides utiles et des actions claires lorsque la bibliothèque ne contient aucun fichier.
- [x] Détecter à l'affichage les chemins de bibliothèque devenus indisponibles, expliquer qu'une réimportation est nécessaire et désactiver le lancement direct quand le chemin ne désigne plus un fichier.
- [x] Ajouter des tests unitaires des états de chemin : fichier absent, répertoire au lieu d'un fichier et fichier existant en attente de validation du lancement.
- [x] Tester le refus d'un format inconnu sans créer de session fantôme ni modifier la session active.
- [x] Tester le refus par le cœur d'une ROM détectée mais invalide, sans modifier les sessions existantes.
- [x] Tester le rejet d'un contenu reconnu par plusieurs sondes, sans créer de session fantôme ni changer la sélection active.
- [x] Tester la disparition d'un fichier déjà importé et vérifier que sa disponibilité passe à « manquant »; les erreurs d'ouverture sont propagées par le pipeline de chargement. Le test de permissions OS strictement inaccessibles reste dépendant de la plateforme.
- [x] Tester le refus d'un contenu vide ou dépassant la limite de taille sans modifier la session active ni son état.
- [x] Tester qu'un échec de lancement sur un chemin absent ne crée pas de session fantôme et préserve la session active ainsi que les entrées de bibliothèque existantes.

**Critère de sortie :** les parcours import → bibliothèque → sélection → validation → lancement fonctionnent et les erreurs sont récupérables.

### P1.3 — gestion des sessions et commandes génériques

- [x] Gérer plusieurs sessions, la sélection de la session active, pause/reprise, arrêt/réinitialisation et fermeture depuis l'interface.
- [x] Avancer les sessions via le contrôleur avec budget borné et remonter état, capacités, trame la plus récente et erreur.
- [x] Relier les transitions de clavier aux commandes sémantiques partagées et libérer les contrôles lors de la perte de focus.
- [ ] Exécuter et compléter la matrice de tests de création, changement de session, pause/reprise, arrêt, réinitialisation, fermeture d'une session et fermeture globale; plusieurs tests unitaires de contrôleur existent, mais la validation complète du parcours desktop reste en attente.
- [x] Sur erreur d'exécution ou de conversion/récupération vidéo, afficher l'erreur de tick dans le gestionnaire de sessions et effacer la trame mise en cache pour ne pas laisser une image périmée à l'écran.
- [x] Conserver les diagnostics de tick séparément par identifiant de session, puis les retirer à la fermeture de la session concernée ou de toutes les sessions.
- [x] Effacer le diagnostic de tick obsolète après un arrêt ou une réinitialisation réussis, sans toucher aux diagnostics des autres sessions; ajouter un test ciblé de ce nettoyage.
- [ ] Tester les erreurs de tick, les transitions après arrêt/suppression de la session active et le nettoyage des ressources sur ces chemins.
- [x] Relâcher les entrées sur perte de focus, changement/fermeture de session et fermeture globale; tenter également de les libérer quand l'avancement de session échoue ou remonte une erreur média. La vérification manuelle des périphériques reste à exécuter.
- [x] Afficher les capacités réellement déclarées et expliquer explicitement l'absence de vidéo/audio dans la vue de jeu, la liste des sessions et les diagnostics.
- [ ] Ne pas présenter un cœur factice comme une session de console jouable.

**Critère de sortie :** les commandes sont prévisibles et aucun changement de session ou échec ne laisse l'interface dans un état périmé.

### P1.4 — préférences, diagnostics et ergonomie

- [x] Séparer les préférences de présentation du matériel émulé et extraire leur persistance.
- [x] Signaler correctement l'échec de sauvegarde dans l'interface au lieu d'afficher un succès trompeur; tester l'échec d'écriture et la préservation de l'ancien fichier de préférences.
- [x] Synchroniser le fichier temporaire de préférences sur le disque (`sync_all`) avant de le promouvoir en configuration active, et nettoyer le temporaire si l'écriture ou la synchronisation échoue.
- [x] Récupérer la dernière configuration de sauvegarde si une interruption survient après la mise à l'écart du fichier actif mais avant la promotion du nouveau fichier; tester ce scénario de récupération.
- [x] Prévoir thème clair/sombre, échelle UI et dimensions initiales de fenêtre.
- [x] Ajouter une couleur d'accent personnalisable pour boutons/états sélectionnés/surlignages, sans recolorer les fonds du thème.
- [x] Persister la couleur d'accent dans la configuration versionnée tout en acceptant les préférences de version 1.
- [x] Ajouter une page de paramètres claire avec thème clair/sombre, échelle UI, sélecteur de couleur d'accent et réinitialisation de la couleur.
- [x] Tester lecture/écriture, version inconnue, données malformées, valeurs hors limites et paramètres absents, y compris l’aller-retour des chemins contenant des échappements et le rejet des séquences d’échappement invalides.
- [x] Couvrir en tests le premier démarrage sans fichier de préférences, les versions inconnues, le repli sûr pour des valeurs malformées (échelle, accent, chemin) et le bornage de l'échelle UI; l'écriture normalise aussi les échelles non finies vers 1,0 et borne les valeurs hors plage avant persistance. Les tests sont ajoutés, leur exécution reste à confirmer en CI.
- [x] Inclure l'état du vérificateur de mises à jour et le chemin du paquet vérifié dans le panneau et le rapport de diagnostic copiable.
- [ ] Fournir un panneau de diagnostic par session : état, système, capacités, contenu chargé et dernière erreur exploitable.
- [x] Exposer l'indicateur « contenu chargé » dans le résumé contrôleur et l'afficher dans le gestionnaire de sessions.
- [x] Conserver le chemin source associé à chaque session lancée depuis le bureau, l'afficher dans le gestionnaire et le nettoyer à la fermeture de la session ou de toutes les sessions.
- [ ] Distinguer les réglages hôte (interface, audio, vidéo, raccourcis, dossiers) des réglages spécifiques à un cœur, qui attendront l'arrivée des modules.
- [ ] Mettre en place l'internationalisation avant la finition UX : catalogue de langues séparé du code Rust (par exemple `locales/en.json` et `locales/fr.json`), clés stables (`settings.title`, `common.save`), langue sélectionnable dans les paramètres et choix persistant.
- [ ] Remplacer progressivement les chaînes visibles codées en dur par des clés traduites via un module central `i18n.rs`; couvrir navigation, bibliothèque, sessions, jeu, systèmes, paramètres, erreurs et dialogues.
- [ ] Définir une langue de repli si la traduction ou une clé manque, et gérer proprement les textes dynamiques, pluriels et variables; garder les logs techniques et identifiants internes indépendants de la langue.
- [ ] Tester chargement des catalogues, clés absentes, langue inconnue, changement de langue à chaud et restauration après redémarrage; commencer avec français et anglais.
- [ ] Améliorer les messages d'erreur pour indiquer ce qui a échoué et quelle action l'utilisateur peut tenter.
- [ ] Vérifier navigation clavier, focus, redimensionnement, écrans vides et fenêtres de taille minimale.

**Critère de sortie :** l'utilisateur peut comprendre l'état de l'application, modifier et conserver ses préférences, et diagnostiquer un échec sans lire les logs du développeur.

### P1.5 — présentation média et cadence hôte génériques

Cette étape concerne les adaptateurs de l'application, pas l'implémentation vidéo/audio d'une console. Si aucun cœur disponible ne fournit ces sorties, les tests utilisent un cœur de test déterministe et l'application indique honnêtement que le média réel n'est pas disponible.

- [x] Ajouter un adaptateur de rendu vidéo RGBA via texture egui, avec prise en compte du stride et allocations bornées.
- [x] Ajouter une cadence hôte à 60 Hz avec accumulation du temps fractionnaire et rattrapage borné.
- [x] Ajouter des tests de conversion RGBA, de stride et de limite mémoire (validation CI encore à confirmer).
- [x] Tester la pause/reprise et l'absence de progression pendant la pause avec un compteur observable dans un cœur de test.
- [x] Ajouter un adaptateur audio hôte natif via rodio, indépendant du cœur, avec ouverture facultative du périphérique par défaut et repli propre si aucun périphérique n'est disponible.
- [x] Acheminer les buffers PCM signés 16 bits depuis le contrôleur jusqu'au périphérique hôte; la collecte par tick est bornée à 16 buffers et la file de lecture à 8 blocs, avec purge du retard excessif.
- [x] Synchroniser pause/reprise et purger l'audio lors de l'arrêt, de la réinitialisation, du changement ou de la fermeture des sessions.
- [x] Tester la collecte du signal PCM déterministe depuis un cœur de test, la normalisation i16 vers f32 et la limite de file audio.
- [ ] Valider en CI le rendu audio sur les trois plateformes; l'accès à un vrai périphérique audio n'est pas garanti dans les runners headless.
- [x] Documenter formats audio, limites de buffers, repli sans périphérique et comportement en cas de capacité absente dans `docs/AUDIO_OUTPUT.md`.
- [x] Nettoyer la file audio lors du changement/arrêt d'une session; les textures vidéo continuent d'être libérées par les chemins existants.

**Critère de sortie :** les sorties annoncées sont présentées par des adaptateurs testés; aucune fonctionnalité média inexistante n'est présentée comme opérationnelle.

### P1.6 — tests de parcours, CI et finition de l'application
- [x] Ajouter un quality gate Clippy pour le crate desktop sur Linux, en complément de formatage, compilation et tests sur la matrice existante.
- [x] Étendre Clippy du crate desktop à Linux, Windows et macOS afin de vérifier aussi les branches spécifiques aux plateformes; les résultats de cette nouvelle matrice restent à confirmer en CI.
- [x] Installer explicitement les dépendances système X11/Wayland/OpenGL/audio requises avant les contrôles du desktop sur les runners Linux du workflow CI principal; la réussite effective des jobs doit encore être vérifiée.

- [ ] Ajouter des tests de parcours couvrant démarrage, bibliothèque vide, import, lancement réussi, lancement refusé, changement de session, paramètres, arrêt et fermeture globale.
- [x] Ajouter un test d'intégration du lancement réussi depuis la bibliothèque avec une ROM de test construite en mémoire sur disque temporaire, puis pause/reprise/arrêt/fermeture; son exécution reste à confirmer en CI.
- [ ] Exécuter les quality gates Rust sur le dernier commit, pas seulement s'appuyer sur des résultats CI historiques.
- [x] Ajouter `.github/workflows/desktop-ci.yml` pour lancer formatage, tests et Clippy du desktop sur Linux, Windows et macOS (le workflow est créé, ses résultats restent à vérifier).
- [x] Déclencher aussi les quality gates desktop quand le manifeste de mise à jour (`ArchiaEmu/update/**`) change, afin que les changements de canal de publication ne passent pas silencieusement hors de la CI.
- [x] Étendre le workflow aux tests et à Clippy du workspace partagé, en plus du crate desktop exclu du workspace (exécution CI toujours en attente de résultats vérifiables).
- [x] Ajouter à la CI la génération de la documentation du workspace avec `RUSTDOCFLAGS=-D warnings`, un délai maximal de job et une concurrence contrôlée; l'état des runs reste à vérifier.
- [x] Corriger le parcours « Save settings now » : un échec d'écriture ou l'absence de répertoire de configuration ne peut plus être affiché comme une sauvegarde réussie; ajouter un test de défaillance du système de fichiers.
- [ ] Faire passer la CI desktop sur Linux, Windows et macOS; traiter toute régression avant de continuer.
- [ ] Vérifier démarrage et fermeture propres, absence de panique sur erreurs utilisateur et absence de session/ressource orpheline.
- [x] Actualiser le README pour décrire honnêtement les fonctions/limites du projet et les quality gates configurés.
- [x] Ajouter `docs/MANUAL_TEST_CHECKLIST.md` pour les parcours principaux; la checklist reste à exécuter et à documenter sur chaque plateforme.
- [ ] Définir le critère de « version application utilisable » et le vérifier explicitement avant de déclarer P1 terminé.

**Critère de sortie P1 — application utilisable :** bibliothèque et préférences fiables, sessions génériques gérables, diagnostics compréhensibles, adaptateurs média honnêtes, tests reproductibles et CI verte sur les plateformes configurées. La compatibilité d'une console reste hors de ce critère.

### P1.7 — livraison locale et installateur Windows

Cette étape est obligatoire avant de déclarer P1 terminé. La cible initiale de livraison est Windows, cohérente avec le besoin d'un installateur .exe; les autres plateformes doivent continuer à passer leurs quality gates sans être considérées comme livrées si aucun paquet n'est produit.

- [x] Choisir et documenter Inno Setup 6 comme technologie de l'installateur Windows; script versionné dans `packaging/windows/ArchiaEmu.iss`.
- [ ] Construire l'application desktop en mode release depuis un environnement propre, avec versions verrouillées. Le workflow release existe, mais le crate desktop n'a pas encore de lockfile dédié validé; le build du workflow n'utilise donc pas encore `--locked` et ne garantit pas une résolution identique des dépendances d'un run à l'autre.
- [ ] Recenser et embarquer tous les fichiers, bibliothèques natives, ressources et runtimes réellement requis; le workflow stage actuellement le binaire seul, à confirmer sur une VM Windows propre.
- [x] Ajouter un workflow Windows x64 qui construit l'application, génère l'installateur versionné et son checksum SHA-256, publie un artefact de workflow et joint l'installateur aux releases créées depuis un tag `v*`; le premier run et ses résultats restent à vérifier.
- [x] Installer dans un répertoire local choisi, avec mode utilisateur par défaut et possibilité d'élévation; les préférences restent dans `%APPDATA%\\ArchiaEmu\\desktop.conf`, pas à côté de l'exécutable. Vérification réelle des chemins en VM toujours requise.
- [x] Créer un raccourci Menu Démarrer et proposer le raccourci bureau comme option décochée par défaut dans l'assistant.
- [x] Fournir le désinstallateur Inno Setup via Windows et le menu Démarrer, avec une question explicite sur la suppression des préférences/journaux/cache connus; les ROM et fichiers arbitraires ne sont pas ciblés. Vérification réelle en VM toujours requise.
- [ ] Vérifier le premier démarrage depuis le menu Démarrer et depuis le raccourci bureau, sans Rust, dépôt source ni dossier de build présents.
- [ ] Vérifier bibliothèque, préférences, journaux/diagnostics et chemins de contenus après fermeture puis redémarrage.
- [x] Héberger un manifeste stable initial et ajouter depuis les paramètres une vérification manuelle asynchrone distincte des modules; valider le schéma, les versions, les URL HTTPS et les limites de taille/SHA-256, sans annoncer de binaire tant qu'aucun artefact n'est publié.
- [x] Afficher explicitement la comparaison de version dans les paramètres : indiquer la version locale et la version disponible, ou confirmer que l'application est à jour au lieu de laisser le cas « aucune mise à jour » implicite.
- [x] Ajouter le téléchargement manuel d'un artefact publié vers un fichier temporaire de staging, avec limites de taille, hachage SHA-256, validation des hôtes GitHub autorisés et restriction des URL `github.com` aux assets de release de `Likounsee/ArchiaEmu`; aucune exécution/installation automatique.
- [x] Vérifier le `Content-Length` annoncé par le serveur avant de créer le fichier de staging; rejeter les longueurs malformées ou différentes de la taille du manifeste et conserver le contrôle borné du flux si l'en-tête est absent. Tests unitaires ajoutés, exécution à confirmer en CI.
- [x] Limiter le nettoyage d'un téléchargement échoué aux fichiers de staging créés par cette tentative et finaliser le fichier vérifié via une création de lien atomique qui échoue si la destination existe, sans écrasement; tests ajoutés pour le conflit (fichier existant préservé) et le chemin nominal (contenu identique aux deux chemins), exécution à confirmer en CI.
- [x] Refuser les versions d'application non canoniques avec zéros initiaux dans les composants numériques du manifeste et valider à nouveau la version avant toute création de staging; tests couvrant les versions invalides avant accès réseau, exécution à confirmer en CI.
- [x] Rejeter les clés de plateforme inconnues dans le manifeste updater au lieu de les ignorer silencieusement; test de régression pour une clé erronée ajoutée, exécution à confirmer en CI.
- [x] Restreindre aussi l'URL des notes de version au chemin des releases du dépôt officiel `Likounsee/ArchiaEmu`; tests couvrant domaines, dépôts, préfixes trompeurs, identifiants et ports non autorisés, exécution à confirmer en CI.
- [x] Terminer proprement le suivi des tâches updater si un thread se déconnecte avant d'envoyer son résultat, plutôt que de laisser l'interface en état « en cours » avec des rafraîchissements indéfinis; tests unitaires ajoutés, exécution à confirmer en CI.
- [ ] Mettre en place une vérification automatique des mises à jour d'ArchiaEmu, séparée de celle des modules, à partir d'un manifeste de versions hébergé sur le dépôt GitHub officiel (version locale comparée à la version publiée).
- [ ] Pour l'updater de l'application, télécharger vers une zone temporaire, vérifier version, intégrité et provenance, préparer l'installation puis appliquer la mise à jour uniquement lorsque l'application est fermée et qu'aucune écriture n'est en cours; relancer l'application après succès et conserver une récupération/rollback en cas d'échec.
- [ ] Permettre à l'utilisateur de choisir la fréquence de vérification, d'être informé de la version et des notes de version, et de différer une mise à jour; ne jamais remplacer les fichiers exécutables en cours d'utilisation.
- [ ] Vérifier mise à niveau depuis une version précédente, échec/annulation d'installation et désinstallation sans supprimer les contenus personnels sans consentement explicite.
- [x] Documenter la procédure de packaging, les limites de reproductibilité et la checklist d'acceptation Windows dans `docs/WINDOWS_INSTALLER.md`; le comportement réel doit encore être validé.
- [x] Étendre le smoke test Windows pour vérifier qu'une désinstallation silencieuse supprime bien l'exécutable installé sans effacer ni modifier le fichier de préférences utilisateur (la suppression des données doit rester liée à un consentement explicite); exécution CI à confirmer.
- [ ] Tester installateur, désinstallateur et mise à jour sur une machine/VM Windows propre et conserver une preuve de résultat; ne pas cocher le critère sur la seule présence d'un fichier .exe.

**Critère de sortie P1.7 :** un utilisateur peut télécharger l'installateur, installer ArchiaEmu, obtenir ses raccourcis, utiliser les fonctions génériques prises en charge entièrement en local, recevoir les vérifications de mises à jour du programme et mettre à jour de manière sûre sans environnement de développement ni fichiers manquants.

## P2 — catalogue et téléchargement des modules (après P1)

Les contrats de base existent; on reprend leur approfondissement après validation de l'application. **Le rôle de la page Modules est de présenter le catalogue et de permettre le téléchargement individuel des paquets de modules. Elle ne doit ni installer ni activer automatiquement un module.** Télécharger un module doit récupérer uniquement l'artefact de ce module, jamais l'application complète, le dépôt source ou un paquet global contenant tous les modules. Le frontend consomme un catalogue et des capacités, sans dépendre des détails internes des modules.

- [x] Identifiants stables, descripteurs, capacités et compatibilité de version de contrat.
- [x] Validation des dépendances et planification de l'ordre d'activation.
- [x] Plan d'installation/désinstallation, contrôle de budget, journal d'installation borné et récupération par jalons.
- [x] Staging borné des artefacts et validation SHA-256 fournie par l'hôte.
- [x] Validation portable des chemins d'archives avant extraction.
- [ ] Maintenir un manifeste de catalogue de modules séparé du manifeste de l'application, hébergé dans une source officielle vérifiable (par exemple le dépôt GitHub du projet), avec identifiant, version, version minimale compatible, URL de l'artefact individuel, taille, SHA-256, notes et métadonnées de signature.
- [ ] Mettre à jour la page **Modules** pour afficher les modules disponibles, leur description, version, compatibilité, statut stable et taille, avec une action explicite « Télécharger » pour chaque module.
- [ ] Au clic sur « Télécharger », récupérer uniquement l'artefact du module sélectionné, avec progression, annulation, gestion des erreurs et destination clairement indiquée; ne pas télécharger l'application complète ni tous les modules.
- [ ] Ne pas installer, extraire, activer, charger ni mettre à jour automatiquement un module après son téléchargement. Le périmètre de cette page est le catalogue et le téléchargement; toute future fonction d'installation devra faire l'objet d'une décision distincte et d'une action explicite de l'utilisateur.
- [ ] Vérifier la taille et le SHA-256 de l'artefact téléchargé et afficher un résultat clair; ne pas présenter un fichier invalide ou incomplet comme un téléchargement réussi.
- [ ] N'afficher dans le catalogue stable que les versions explicitement déclarées stables et publiables après validation des tests, de la compatibilité et des artefacts; garder les versions de développement/préversions dans un canal séparé ou hors catalogue stable.
- [ ] Fournir un rafraîchissement du catalogue et afficher les nouvelles versions disponibles; cela met à jour la page et les métadonnées, sans remplacer ni installer les fichiers locaux.
- [ ] Intégrer un téléchargeur réseau robuste avec limites, délais, progression et annulation.
- [ ] Utiliser une implémentation cryptographique maintenue et auditée pour vérifier les signatures; ne pas inventer de cryptographie.
- [ ] Vérifier la signature et la provenance avant de faire confiance au catalogue ou à un artefact téléchargé.
- [ ] Définir une destination de téléchargement sûre, éviter l'écrasement silencieux d'un fichier existant et permettre à l'utilisateur de choisir le dossier si nécessaire.
- [ ] Tester les téléchargements interrompus, annulés, corrompus, incompatibles et repris, sans laisser de fichier final trompeur.
- [ ] Produire des diagnostics structurés avec contexte et causes sans exposer de données sensibles.
- [ ] Ajouter fuzzing ciblé pour les parseurs et fichiers non fiables.
- [ ] Vérifier dépendances, licences, provenance et contenu des paquets dans la CI.
- [ ] Afficher dans l'application les modules disponibles, les fichiers déjà téléchargés et les incompatibilités connues, sans prétendre que les modules téléchargés sont installés ou utilisables.

**Critère de sortie :** la page Modules actualise le catalogue et télécharge uniquement l'artefact choisi, avec validation d'intégrité, erreurs récupérables et limites de confiance documentées. Seules les versions explicitement stables et validées apparaissent dans le catalogue stable; les versions en développement restent séparées. Le téléchargement n'installe ni n'active rien. Un manifeste valide ne prouve pas l'innocuité du code natif; aucun sandboxing ne doit être revendiqué sans mécanisme réel.

## Architecture transversale — stratégies d'exécution CPU (préparation de P3/P4)

**État de cette sous-phase : environ 20 %** — les couches sont documentées et un sélecteur de stratégie CPU explicite avec tests unitaires existe; les moteurs CPU, le pont ABI, les services système, le JIT et la traduction GPU ne sont pas implémentés par ce jalon. Cette piste architecturale ne remplace pas la priorité P1 et ne doit pas gonfler artificiellement son pourcentage.

### Contrat et politique CPU

- [x] Analyser les fondations existantes : `EmulationCore`, `CoreRegistry`, `CoreDescriptor`, capacités explicites, `ApplicationController`, sessions, événements, entrées, vidéo/audio, sauvegardes et contrats de modules versionnés.
- [x] Documenter l'exécution native x86-64 avec couche de compatibilité et distinguer CPU, services système/ABI, modèle matériel et traduction GPU.
- [x] Ajouter un contrat commun de planification `execution_strategy` avec architectures invité/hôte, politique explicite et trois résultats : natif x86-64 avec compatibilité, interpréteur, JIT.
- [x] Ajouter des tests unitaires pour la sélection native, l'interpréteur, la préférence JIT et les architectures ARM, PowerPC, MIPS et propriétaires.
- [ ] Exécuter les tests et quality gates du commit exact dans la CI.
- [ ] Ajouter les stratégies réellement implémentées et exigences hôte dans un descripteur versionné, sans casser silencieusement les descripteurs publics existants.
- [ ] Définir un contrat de capacités pour le moteur CPU, le niveau de compatibilité système et le backend graphique; refuser explicitement une stratégie non implémentée.
- [ ] Définir des interfaces indépendantes pour le chargeur/exécutable et l'ABI, les services système, la mémoire virtuelle, les exceptions et les interruptions.
- [ ] Définir un contrat de traduction graphique indépendant du CPU, couvrant commandes, ressources, shaders, synchronisation et backend hôte.
- [ ] Créer une suite de conformité réutilisable : capacités cohérentes, erreurs non prises en charge, cycle de vie, entrées, média, sauvegardes et nettoyage.
- [ ] Tester les moteurs interpréteur comme référence avant de comparer un JIT; ne pas activer le JIT sans validation, et garder un chemin de débogage.
- [ ] Définir des tests d'intégration reproductibles pour ABI/services, mémoire, GPU, audio, entrées et exécution de logiciels de test légaux.
- [ ] Documenter les exigences de sécurité JIT, les contraintes de protection mémoire, les firmwares fournis par l'utilisateur et les limites de compatibilité.

**Critère de sortie :** la sélection de stratégie est intégrée à un descripteur versionné et vérifiée au lancement; chaque module indique précisément ce qu'il implémente; les contrats communs ont des tests de conformité; un programme de test reproductible démontre le comportement annoncé. Un enum, une CI verte ou un cœur factice ne prouve pas la compatibilité d'une console.

**Règles de sélection :**
- x86-64 invité + hôte x86-64 : stratégie native avec couche de compatibilité possible uniquement si elle est explicitement autorisée et implémentée; ce n'est jamais une promesse d'exécution directe universelle.
- Autres architectures : interpréteur de référence; JIT optionnel selon les performances, l'architecture hôte, la sécurité et les capacités du module.
- CPU, compatibilité système et traduction GPU restent trois couches distinctes.
- Une stratégie choisie par la politique est une préférence; le module doit confirmer qu'il la prend en charge.

## P3 — premier module de console réellement fonctionnel (après P1)

Le module Game Boy existant peut servir de première cible, mais son existence ne signifie pas que les jeux sont jouables. Cette phase ne commence qu'une fois P1 validé, sauf corrections minimales requises pour garder l'application testable.

- [ ] Définir une matrice de couverture matérielle et des programmes de test reproductibles.
- [ ] Compléter CPU, flags, interruptions, timers et bus nécessaires au périmètre visé.
- [ ] Implémenter les périphériques et banques mémoire/cartouches nécessaires au périmètre annoncé.
- [ ] Implémenter la vidéo et l'audio du système et les relier aux adaptateurs P1.5.
- [ ] Vérifier les entrées utilisateur, reset, exécution, sauvegarde/restauration et erreurs de cartouche.
- [ ] Exécuter au moins un programme représentatif de bout en bout avec résultat vérifiable.
- [ ] Publier les limitations connues et ne revendiquer que la compatibilité démontrée.

**Critère de sortie :** un périmètre de système documenté fonctionne réellement de bout en bout; un cœur factice ou un crate compilable ne suffit pas.

## P4 — autres modules/systèmes

- [ ] Garder chaque cœur dans un crate indépendant dépendant du contrat commun, pas du frontend.
- [ ] Créer une suite de conformité réutilisable pour tous les cœurs.
- [ ] Ajouter d'autres systèmes seulement après stabilisation des contrats, de l'I/O et du pipeline de lancement.
- [ ] Pour chaque système, fournir une matrice de compatibilité, des programmes de test légaux et des critères de support explicites.
- [ ] Pour les systèmes qui dépendent d'un firmware, documenter les fichiers à fournir légalement par l'utilisateur ou la stratégie de réimplémentation indépendante.
- [ ] Ne pas distribuer de firmware, d'assets propriétaires ou de jeux sans autorisation.
- [ ] Ne jamais présenter une interface recréée comme le firmware original.

## P5 — publication et maintenance

- [ ] Construire des paquets reproductibles pour chaque plateforme cible.
- [ ] Documenter installation, désinstallation, mises à jour, rollback et collecte des diagnostics.
- [ ] Vérifier licences, notices, dépendances et ressources distribuées.
- [ ] Définir politique de versions, plateformes supportées et cycle de maintenance.
- [ ] Publier une liste honnête des systèmes pris en charge, fonctionnalités fonctionnelles et limites connues.

## Règles de priorité

1. **Application utilisable avant développement des modules de console.**
2. Les fondations existantes sont maintenues, mais les améliorations non bloquantes du gestionnaire de modules attendent P2.
3. Corriger d'abord les régressions du build, de la CI et des contrats partagés.
4. Privilégier les fonctionnalités génériques réutilisables et les tests de parcours.
5. Distinguer strictement code présent, tests écrits, tests exécutés, CI verte et compatibilité réellement démontrée.
6. Tous les changements restent exclusivement sur la branche `ArchiaEmu`; ne jamais travailler sur `main`.

## Vérification CI

Depuis le répertoire `ArchiaEmu/` :

```sh
cargo fmt --all -- --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked
```

Après toute modification, consulter les résultats de la CI du commit exact sur Linux, Windows et macOS. Une CI historique verte ne valide pas les commits ultérieurs et ne prouve pas l'émulation complète.
