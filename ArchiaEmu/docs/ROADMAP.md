# ArchiaEmu — roadmap orientée application d'abord

## Direction du projet

**Ordre de priorité non négociable : rendre d'abord l'application ArchiaEmu réellement utilisable, puis développer et intégrer les modules de systèmes/console.** Pour la première étape, on travaille sur l'application générique, son interface, ses parcours utilisateur, son architecture frontend/backend, sa bibliothèque, ses sessions, ses paramètres, ses diagnostics, sa stabilité et sa distribution. On ne développe pas de fonctionnalités matérielles propres à une console pendant cette phase, sauf cœur factice ou adaptateur minimal nécessaire pour tester un contrat commun.

Les fondations du runtime et des modules déjà présentes sont conservées et corrigées si nécessaire, mais leur présence ne signifie pas qu'il faut continuer à enrichir le système de modules avant que l'application soit utilisable. Le terminal reste un outil de diagnostic; il ne remplace pas l'application desktop.

Les pourcentages sont des **estimations fonctionnelles** fondées sur les critères observables, pas sur le temps passé. Les jalons se recouvrent et ne doivent pas être moyennés mécaniquement. Le code écrit ou un test avec un cœur factice ne prouve ni le succès de la CI ni la compatibilité d'une console. Une case n'est cochée qu'après vérification du critère décrit.

## Ordre général des priorités

| Ordre | Phase | État estimé | Objectif |
| --- | --- | ---: | --- |
| **P0** | Fondations déjà présentes | Majoritairement terminé | Garder le runtime, les contrats communs et le pipeline de contenu suffisamment stables pour construire l'application |
| **P1** | **Application desktop utilisable — priorité actuelle** | **≈ 41 %** | Une application cohérente permettant de gérer une bibliothèque et des sessions, avec parcours fiables, paramètres, diagnostics et architecture maintenable |
| **P1.1** | Architecture frontend/backend de l'application | En cours | UI séparée de l'orchestration et de l'exécution, modules d'interface clairs, tests isolés |
| **P1.2** | Parcours de bibliothèque et lancement | En cours | Importer, rechercher, sélectionner, valider, lancer et gérer les erreurs sans état incohérent |
| **P1.3** | Sessions et commandes génériques | En cours | Gérer plusieurs sessions, pause/reprise/arrêt/réinitialisation et fermeture propre |
| **P1.4** | Paramètres, diagnostics et ergonomie | À faire en partie | Préférences fiables, états vides/erreurs explicites, diagnostics exploitables |
| **P1.5** | Sorties média hôte et validation multiplateforme | À faire en partie | Afficher les trames et jouer l'audio lorsqu'un cœur le fournit, tester les adaptateurs et garantir les quality gates |
| **P2** | Intégration et fiabilité des modules | Après l'application utilisable | Découverte, installation, mise à jour, provenance, récupération et frontières de confiance |
| **P3** | Premier module de console réellement fonctionnel | Après P1 | Un système testé de bout en bout, avec limites de compatibilité honnêtes |
| **P4** | Autres modules/systèmes | Après stabilisation de P3 | Étendre le catalogue sans coupler les cœurs à l'application |
| **P5** | Publication et maintenance | En dernier | Paquets reproductibles, documentation de support, mises à jour et politique de version |

> **Définition de « application utilisable » pour P1 :** l'application démarre, présente une interface cohérente, permet de gérer les contenus et les sessions prises en charge, explique clairement les capacités absentes, récupère les erreurs sans se fermer ni laisser de session fantôme, conserve ses préférences, et passe les tests et la CI configurés. Cela ne signifie pas encore qu'une console est jouable. Tant qu'aucun cœur ne produit de vraies sorties, les vues vidéo/audio doivent afficher un état honnête plutôt que prétendre lire un jeu.

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

**État estimé : en cours — environ 41 % pour l'objectif global P1, estimation qualitative.**

Le desktop utilise eframe/egui, possède une bibliothèque et un contrôleur d'application partagé. Les adaptateurs de vue, les commandes typées du contrôleur, l'avancement des sessions via `ApplicationController::advance_session` et les préférences dans `preferences.rs` ont commencé à séparer l'interface de l'exécution.

- [x] Documenter le choix eframe/egui et ses critères multiplateformes.
- [x] Extraire les modèles d'application dans une bibliothèque Rust réutilisable et introduire `ApplicationController`.
- [x] Isoler les adaptateurs desktop pour les entrées et la présentation des trames.
- [x] Ajouter des commandes typées de session/entrée au contrôleur.
- [x] Déplacer l'avancement des sessions et la collecte bornée des médias derrière `ApplicationController::advance_session`.
- [x] Extraire la persistance des préférences dans `preferences.rs`.
- [x] Découper le rendu des pages hors de `main.rs` : accueil, bibliothèque, jeu, sessions, systèmes et paramètres ont désormais chacune leur module UI (`*_page.rs`). La navigation et la composition de l'application restent dans `main.rs`.
- [x] Retirer les accès directs au runtime depuis les vues pour les lectures de sessions; exposer au contrôleur le nombre de sessions et des résumés de session stables. Les commandes de cycle de vie restent aussi derrière le contrôleur.
- [ ] Définir des états de vue explicites pour chargement, succès, absence de contenu, capacité indisponible et erreur.
- [ ] Ajouter des tests du contrôleur et du frontend pour les transitions et les erreurs, sans nécessiter de cœur réel.
- [ ] Valider les changements avec `cargo fmt --all -- --check`, `cargo test --workspace --locked`, `cargo clippy --workspace --all-targets --locked -- -D warnings` et la CI multiplateforme.

**Critère de sortie :** l'interface orchestre l'application par des API explicites; le code UI ne pilote pas directement l'exécution du cœur et les comportements essentiels sont testables séparément.

### P1.2 — bibliothèque de contenus et lancement fiable

- [x] Afficher la bibliothèque, les systèmes enregistrés et leurs capacités déclarées.
- [x] Importer plusieurs fichiers, afficher les imports rejetés, rechercher titre/chemin, filtrer et trier les entrées.
- [x] Sélectionner un contenu, lancer via le pipeline commun et afficher les erreurs de validation/lancement.
- [x] Supprimer une entrée avec désélection cohérente.
- [x] Persister la bibliothèque entre les redémarrages via les préférences, revalider chaque chemin au chargement et signaler les fichiers absents/invalides avec quelques détails pour permettre leur réimport.
- [ ] Clarifier le modèle de contenu : chemin source, nom affiché, format détecté, système compatible, dernier résultat de validation et état de disponibilité.
- [x] Prévoir des états vides utiles et des actions claires lorsque la bibliothèque ne contient aucun fichier.
- [x] Détecter à l'affichage les chemins de bibliothèque devenus indisponibles, expliquer qu'une réimportation est nécessaire et désactiver le lancement direct quand le chemin ne désigne plus un fichier.
- [x] Ajouter des tests unitaires des états de chemin : fichier absent, répertoire au lieu d'un fichier et fichier existant en attente de validation du lancement.
- [ ] Tester fichiers absents, fichiers vides, formats inconnus, contenu ambigu, contenu trop grand, contenu refusé par le cœur et fichier inaccessible.
- [ ] Garantir qu'un échec de lancement n'ajoute pas de session fantôme et n'endommage pas la bibliothèque.

**Critère de sortie :** les parcours import → bibliothèque → sélection → validation → lancement fonctionnent et les erreurs sont récupérables.

### P1.3 — gestion des sessions et commandes génériques

- [x] Gérer plusieurs sessions, la sélection de la session active, pause/reprise, arrêt/réinitialisation et fermeture depuis l'interface.
- [x] Avancer les sessions via le contrôleur avec budget borné et remonter état, capacités, trame la plus récente et erreur.
- [x] Relier les transitions de clavier aux commandes sémantiques partagées et libérer les contrôles lors de la perte de focus.
- [ ] Tester explicitement création, changement de session, pause/reprise, arrêt, réinitialisation, fermeture d'une session et fermeture globale.
- [x] Sur erreur d'exécution ou de conversion/récupération vidéo, afficher l'erreur de tick dans le gestionnaire de sessions et effacer la trame mise en cache pour ne pas laisser une image périmée à l'écran.
- [x] Conserver les diagnostics de tick séparément par identifiant de session, puis les retirer à la fermeture de la session concernée ou de toutes les sessions.
- [x] Effacer le diagnostic de tick obsolète après un arrêt ou une réinitialisation réussis, sans toucher aux diagnostics des autres sessions; ajouter un test ciblé de ce nettoyage.
- [ ] Tester les erreurs de tick, les transitions après arrêt/suppression de la session active et le nettoyage des ressources sur ces chemins.
- [ ] Vérifier que les entrées sont relâchées dans tous les chemins de perte de focus, de fermeture et d'erreur.
- [ ] Afficher les capacités réellement disponibles et désactiver ou expliquer les commandes non prises en charge.
- [ ] Ne pas présenter un cœur factice comme une session de console jouable.

**Critère de sortie :** les commandes sont prévisibles et aucun changement de session ou échec ne laisse l'interface dans un état périmé.

### P1.4 — préférences, diagnostics et ergonomie

- [x] Séparer les préférences de présentation du matériel émulé et extraire leur persistance.
- [x] Prévoir thème clair/sombre, échelle UI et dimensions initiales de fenêtre.
- [x] Ajouter une couleur d'accent personnalisable pour boutons/états sélectionnés/surlignages, sans recolorer les fonds du thème.
- [x] Persister la couleur d'accent dans la configuration versionnée tout en acceptant les préférences de version 1.
- [x] Ajouter une page de paramètres claire avec thème clair/sombre, échelle UI, sélecteur de couleur d'accent et réinitialisation de la couleur.
- [ ] Tester lecture/écriture, version inconnue, données malformées, valeurs hors limites et paramètres absents.
- [x] Couvrir en tests le premier démarrage sans fichier de préférences, les versions inconnues, le repli sûr pour des valeurs malformées (échelle, accent, chemin) et le bornage de l'échelle UI.
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
- [ ] Valider en CI les tests de conversion RGBA, de stride et de limite mémoire.
- [ ] Vérifier pause/reprise, arrêt et absence de progression pendant la pause avec un cœur de test dont le compteur est observable.
- [ ] Ajouter un adaptateur audio hôte derrière une interface testable et indépendante du cœur.
- [ ] Acheminer les blocs audio avec gestion des buffers invalides, files pleines, back-pressure et arrêt propre.
- [ ] Tester une mire vidéo et un signal audio déterministes produits par un cœur de test.
- [ ] Documenter formats de pixels, formats audio, limites de mémoire et comportement en cas de capacité absente.
- [ ] Confirmer le nettoyage des textures et des ressources audio lors du changement/arrêt d'une session.

**Critère de sortie :** les sorties annoncées sont présentées par des adaptateurs testés; aucune fonctionnalité média inexistante n'est présentée comme opérationnelle.

### P1.6 — tests de parcours, CI et finition de l'application

- [ ] Ajouter des tests de parcours couvrant démarrage, bibliothèque vide, import, lancement réussi, lancement refusé, changement de session, paramètres, arrêt et fermeture globale.
- [ ] Exécuter les quality gates Rust sur le dernier commit, pas seulement s'appuyer sur des résultats CI historiques.
- [ ] Faire passer la CI desktop sur Linux, Windows et macOS; traiter toute régression avant de continuer.
- [ ] Vérifier démarrage et fermeture propres, absence de panique sur erreurs utilisateur et absence de session/ressource orpheline.
- [ ] Actualiser le README avec les fonctions disponibles, les limitations et les étapes de lancement de l'application.
- [ ] Ajouter une checklist de test manuel courte pour les parcours principaux.
- [ ] Définir le critère de « version application utilisable » et le vérifier explicitement avant de déclarer P1 terminé.

**Critère de sortie P1 — application utilisable :** bibliothèque et préférences fiables, sessions génériques gérables, diagnostics compréhensibles, adaptateurs média honnêtes, tests reproductibles et CI verte sur les plateformes configurées. La compatibilité d'une console reste hors de ce critère.

## P2 — modules : installation, confiance et maintenance (après P1)

Les contrats de base existent; on reprend leur approfondissement après validation de l'application. Le frontend doit consommer un catalogue et des capacités, sans dépendre des détails internes des modules.

- [x] Identifiants stables, descripteurs, capacités et compatibilité de version de contrat.
- [x] Validation des dépendances et planification de l'ordre d'activation.
- [x] Plan d'installation/désinstallation, contrôle de budget, journal d'installation borné et récupération par jalons.
- [x] Staging borné des artefacts et validation SHA-256 fournie par l'hôte.
- [x] Validation portable des chemins d'archives avant extraction.
- [ ] Intégrer un téléchargeur réseau robuste avec limites, délais et annulation.
- [ ] Utiliser une implémentation cryptographique maintenue et auditée pour vérifier les signatures; ne pas inventer de cryptographie.
- [ ] Vérifier la signature et la provenance avant de faire confiance à un catalogue ou à un paquet.
- [ ] Rendre les mises à jour résistantes au retour vers une ancienne révision.
- [ ] Définir permissions, chemins autorisés et politique de liens symboliques pour l'extraction.
- [ ] Vérifier extraction, activation atomique, rollback et récupération après interruption.
- [ ] Produire des diagnostics structurés avec contexte et causes sans exposer de données sensibles.
- [ ] Ajouter fuzzing ciblé pour les parseurs et fichiers non fiables.
- [ ] Vérifier dépendances, licences, provenance et contenu des paquets dans la CI.
- [ ] Afficher dans l'application les modules disponibles, installés, incompatibles et leurs capacités, sans annoncer une compatibilité non démontrée.

**Critère de sortie :** les modules peuvent être découverts et gérés avec une provenance vérifiable, des opérations récupérables et des limites de confiance documentées. Un manifeste valide ne prouve pas l'innocuité du code natif; aucun sandboxing ne doit être revendiqué sans mécanisme réel.

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
