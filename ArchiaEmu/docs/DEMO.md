# Présenter ArchiaEmu en 3 minutes

Ce guide sert à présenter honnêtement l'état actuel du projet. ArchiaEmu est une application desktop expérimentale : le shell, la bibliothèque, les sessions et les outils de diagnostic sont en développement. Le cœur Game Boy est expérimental : un petit sous-ensemble d'instructions LR35902 existe, mais il ne constitue pas un cœur complet et **aucune compatibilité de jeu n'est garantie**.

## Préparer la démonstration

Depuis la racine du dépôt, avec Rust stable et les dépendances système de compilation installés :

```powershell
cargo run --release --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml
```

Sur Windows, récupère l'installateur depuis GitHub : dépôt → **Actions** → **Windows installer** → ouvre une exécution terminée avec succès → **Artifacts** → télécharge `ArchiaEmu-Windows-x64-<version>`, puis extrais le `.zip`. Vérifie le fichier `.sha256` fourni avant d'exécuter l'installateur. Si aucun artefact issu d'une exécution réussie n'est disponible, ne présente pas l'installateur comme validé. Pour vérifier un binaire sans lancer l'interface graphique :

```powershell
.\archiaemu-desktop.exe --version
.\archiaemu-desktop.exe --help
```

## Déroulé conseillé

1. **Accueil** — montrer les compteurs de bibliothèque, de sessions et de cœurs enregistrés, puis le champ de sélection de contenu.
2. **Bibliothèque** — importer un fichier `.gb` ou `.gbc` légalement détenu, rechercher un titre, filtrer par format et trier les entrées. L'import ne signifie pas que le jeu sera compatible.
3. **Sessions** — montrer les commandes de session disponibles. Expliquer que leur fonctionnement dépend des capacités réellement implémentées par le cœur chargé.
4. **Paramètres** — modifier le thème, la couleur d'accent et l'échelle de l'interface, puis enregistrer et redémarrer pour vérifier la persistance.
5. **Diagnostics** — afficher la version, la plateforme, le nombre d'éléments et de sessions, l'état média et les erreurs récentes. Utiliser « Copy diagnostics to clipboard » pour montrer le rapport de dépannage.
6. **Installation** — si un artefact validé est disponible, installer dans un répertoire de test, démarrer avec `--version`, puis désinstaller. Ne pas annoncer une validation sur machine propre tant que ce test n'a pas été effectué.

## Vérifications avant la présentation

À lancer depuis la racine du dépôt :

```powershell
cargo fmt --manifest-path ArchiaEmu/Cargo.toml --all -- --check
cargo fmt --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml -- --check
cargo test --manifest-path ArchiaEmu/Cargo.toml --workspace --locked
cargo test --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml
cargo clippy --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml --all-targets -- -D warnings
```

La CI doit être vérifiée sur le commit exact présenté. Une compilation réussie ne prouve ni la compatibilité d'une ROM ni la validation de l'installateur sur une machine propre.

## Limites à annoncer

- Le projet est encore en phase P1 : l'objectif actuel est l'application desktop générique et sa livraison.
- Le cœur Game Boy / Game Boy Color reste incomplet : sous-ensemble d'instructions LR35902 seulement, sans PPU, audio, timer/interruptions, joypad matériel ni banques de cartouche. Ne pas promettre que des jeux commerciaux fonctionnent.
- Le téléchargement de mise à jour prépare un fichier vérifié; l'installation automatique et le rollback ne sont pas activés.
- La reproductibilité de la compilation desktop et la validation de l'installation dans une VM propre restent des points à finaliser.
