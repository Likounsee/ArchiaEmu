# ArchiaEmu — refonte UI (thème Untitled UI) : où placer chaque fichier

Tous les fichiers vont dans `ArchiaEmu/crates/archiaemu-desktop/src/`.

| Fichier | Action | Rôle |
|---|---|---|
| `theme.rs` | **nouveau** | Palette clair/sombre dérivée de l'accent, `apply()`, composants (carte, badge, bouton principal, ligne de réglage, cartes de choix, icônes dessinées, items de navigation) |
| `shell.rs` | **nouveau** | Rail d'icônes, sidebar contextuelle repliable (animation en largeur uniquement), barre de statut fixe |
| `appearance_page.rs` | **nouveau** | Page Apparence : thème clair/sombre en cartes avec aperçu, pastilles d'accent + sélecteur libre, échelle, langue, bouton « Rétablir » |
| `main.rs` | **remplace** | `mod appearance_page; mod shell; mod theme;`, champs `sidebar_open` et `settings_section`, enum `SettingsSection`, accent par défaut indigo, nouveau `update()`, `navigation_items` et `UiStatus::color` supprimés, correction de `ui_status()` |
| `library_page.rs` | **remplace** | Grille de jeux + panneau de détails (maître/détail), double-clic pour lancer |
| `settings_page.rs` | **remplace** | Aiguille vers Apparence ou Application ; l'ancien contenu (mises à jour, diagnostic) est conservé dans « Application » |

## Points d'attention

- `main.rs`, `library_page.rs` et `settings_page.rs` sont basés sur la branche `ArchiaEmu` du dépôt (commit `b8c5592`). Si tes versions locales ont changé depuis, reporte seulement les modifications listées ci-dessus au lieu d'écraser.
- Aucune nouvelle dépendance (pas de changement de `Cargo.toml` ni `Cargo.lock`) : les icônes sont dessinées avec le painter, la police reste celle d'egui par défaut.
- Code **non compilé** : lance, depuis `ArchiaEmu/` :
  `cargo fmt --all`, puis `cargo clippy --all-targets -- -D warnings`, puis `cargo test -p archiaemu-desktop`, puis `cargo run -p archiaemu-desktop`.
- Les pages Accueil, Sessions, Systèmes et Jeu gardent leur mise en page actuelle (elles prennent seulement les nouvelles couleurs).

## Prompt prêt à coller dans ChatGPT

> Je te donne 6 fichiers Rust pour le projet egui/eframe 0.27.2 `archiaemu-desktop` (dépôt Likounsee/ArchiaEmu, branche ArchiaEmu). Place-les dans `ArchiaEmu/crates/archiaemu-desktop/src/` : `theme.rs`, `shell.rs` et `appearance_page.rs` sont nouveaux ; `main.rs`, `library_page.rs` et `settings_page.rs` remplacent les existants. Vérifie que `main.rs` déclare bien `mod appearance_page; mod shell; mod theme;`. Puis lance `cargo fmt --all`, `cargo clippy --all-targets -- -D warnings` et `cargo test -p archiaemu-desktop`, et corrige uniquement les erreurs de compilation ou de lint éventuelles, sans changer le design ni ajouter de dépendance. N'invente pas d'API egui : reste sur egui 0.27.2.
