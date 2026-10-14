# ArchiaEmu — checklist de validation manuelle desktop

Cette checklist valide les parcours génériques de l'application, pas la compatibilité d'un système. Le cœur Game Boy actuel est incomplet et ne doit pas être présenté comme un émulateur de jeux jouable.

## Préparation

- [ ] Construire l'application depuis un checkout propre avec la version stable de Rust.
- [ ] Démarrer ArchiaEmu sans fichier de préférences existant.
- [ ] Confirmer que la fenêtre s'ouvre, que les pages principales sont accessibles et que la fermeture ne panique pas.
- [ ] Redimensionner la fenêtre jusqu'à sa taille minimale puis l'agrandir; vérifier navigation, formulaires et boutons sans débordement majeur.

## Bibliothèque et lancement

- [ ] Vérifier l'état vide de la bibliothèque et ses instructions d'import.
- [ ] Importer un fichier de test autorisé, puis vérifier titre/chemin, recherche, filtre, tri et sélection.
- [ ] Importer plusieurs fichiers en mélangeant fichiers acceptés et refusés; vérifier que les erreurs sont explicites et que les imports valides restent accessibles.
- [ ] Tenter de lancer un fichier absent, vide, trop volumineux, de format inconnu ou malformé; vérifier qu'aucune session fantôme n'est créée et que la sélection existante est préservée.
- [ ] Vérifier le lancement réussi uniquement avec un contenu de test compatible et un cœur dont le périmètre est réellement implémenté. Ne pas compter un cœur factice comme preuve de compatibilité console.
- [ ] Supprimer une entrée et vérifier que la sélection et l'état de lancement restent cohérents.

## Sessions, entrées et médias

- [ ] Créer plusieurs sessions de test, sélectionner chacune et vérifier que la session active affichée est la bonne.
- [ ] Mettre en pause puis reprendre; vérifier que l'exécution ne progresse pas pendant la pause.
- [ ] Arrêter, réinitialiser et fermer une session; vérifier que les diagnostics, entrées et médias obsolètes sont nettoyés.
- [ ] Fermer une session non active puis toutes les sessions; vérifier que la sélection active ne pointe jamais vers une session fermée.
- [ ] Changer de page pendant que des contrôles sont maintenus, puis fermer ou arrêter une session; vérifier la libération des entrées.
- [ ] Vérifier que l'absence de trame/audio est indiquée honnêtement si le cœur ne fournit pas ces médias; vérifier que l'absence de périphérique audio ne fait pas planter l'application.

## Préférences, erreurs et redémarrage

- [ ] Modifier thème, échelle UI et couleur d'accent, enregistrer, fermer et redémarrer; vérifier la persistance.
- [ ] Vérifier qu'une erreur d'écriture des préférences affiche un échec et non un succès.
- [ ] Supprimer/déplacer un fichier de bibliothèque importé, redémarrer et vérifier qu'il est signalé comme indisponible sans lancement possible.
- [ ] Provoquer une erreur utilisateur récupérable; vérifier qu'un message exploitable est présenté et que l'application continue de fonctionner.
- [ ] Vérifier l'écran de paramètres et les contrôles au clavier, y compris le focus après navigation.

## Preuves à conserver

- [ ] Noter OS, version du commit et version Rust utilisée.
- [ ] Joindre captures d'écran des états vide, erreur, bibliothèque, sessions et paramètres.
- [ ] Consigner les échecs, les étapes pour les reproduire et les éventuels journaux pertinents sans données personnelles.
- [ ] Exécuter et archiver les résultats de `cargo fmt --all -- --check`, `cargo test --workspace --locked`, `cargo clippy --workspace --all-targets --locked -- -D warnings` et `RUSTDOCFLAGS="-D warnings" cargo doc --workspace --no-deps --locked`.
- [ ] Vérifier séparément les résultats GitHub Actions du commit exact sur Linux, macOS et Windows.

Une case ne doit être cochée qu'après observation réelle. Cette checklist documente des vérifications à effectuer; sa présence dans le dépôt ne signifie pas qu'elles ont déjà été réalisées.
