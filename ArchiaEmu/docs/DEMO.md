# Présenter ArchiaEmu en 3 minutes

Ce guide sert à présenter honnêtement l'état actuel du projet. ArchiaEmu est une application desktop expérimentale : le shell, la bibliothèque, les sessions et les outils de diagnostic sont en développement. Le cœur Game Boy est expérimental : un petit sous-ensemble d'instructions LR35902 existe, mais il ne constitue pas un cœur complet et **aucune compatibilité de jeu n'est garantie**.

## Préparer la démonstration

Depuis la racine du dépôt, avec Rust stable et les dépendances système de compilation installés :

```powershell
cargo run --release --manifest-path ArchiaEmu/crates/archiaemu-desktop/Cargo.toml
```

Sur Windows, récupère l'installateur depuis GitHub : dépôt → **Actions** → **Windows installer** → ouvre une exécution terminée avec succès → **Artifacts** → télécharge `ArchiaEmu-Windows-x64-<version>`, puis extrais le `.zip`. Avant d'exécuter l'installateur, vérifie le SHA-256 dans PowerShell depuis le dossier extrait :

```powershell
$installer = Get-ChildItem .\ArchiaEmu-Setup-*.exe | Select-Object -First 1
if (-not $installer) { throw "Installateur introuvable" }
$checksumFile = "$($installer.FullName).sha256"
$expected = ((Get-Content $checksumFile -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
$actual = (Get-FileHash $installer.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $expected) { throw "ÉCHEC : le SHA-256 ne correspond pas" }
"SHA-256 OK : $($installer.Name)"
```

Si aucun artefact issu d'une exécution réussie n'est disponible, ne présente pas l'installateur comme validé. Pour vérifier un binaire sans lancer l'interface graphique :

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


## Remise au professeur : checklist rapide

1. Ouvrir l'onglet [Actions](https://github.com/Likounsee/ArchiaEmu/actions/workflows/windows-installer.yml) du dépôt et sélectionner la dernière exécution **Windows installer** terminée avec succès sur la branche `ArchiaEmu`.
2. Télécharger l'artefact `ArchiaEmu-Windows-x64-...` et extraire son ZIP. Le ZIP doit contenir l'installateur `.exe`, son fichier `.sha256`, le `Cargo.lock` résolu utilisé pour compiler et `ArchiaEmu-Build-Info.txt` (commit source et lien vers l'exécution CI).
3. Vérifier le SHA-256 avec la commande ci-dessus avant de lancer l'installateur.
4. Installer l'application, la lancer depuis le menu Démarrer et préparer une ROM Game Boy légale dont tu disposes pour montrer l'importation et l'interface. Ne promets pas qu'un jeu complet fonctionnera : le cœur Game Boy est encore expérimental et incomplet.
5. Joindre le lien du dépôt et le commit correspondant à l'artefact au rendu. N'utilise pas un ancien artefact CI si son commit ne correspond pas au code que tu présentes.

**À vérifier avant remise :** la compilation Windows et les tests d'installation doivent être terminés avec succès pour le commit le plus récent. Une exécution réussie d'une ancienne version ne valide pas automatiquement les modifications suivantes.
