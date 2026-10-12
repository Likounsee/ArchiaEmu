# Contrat de cycle de vie des sessions

Ce document décrit les transitions garanties par `EmulationSession`. Il ne promet pas qu'un cœur particulier émule correctement un système; le cœur reste responsable de la validation de son format et de son état interne.

## États et transitions

| État courant | Opération | Succès | Échec |
| --- | --- | --- | --- |
| `Empty` | `load_content` / chargement borné valide | `Running`, entrées hôte réinitialisées | reste `Empty`; le cœur doit valider avant mutation |
| `Running` | `pause` | `Paused` | transition invalide, état inchangé |
| `Paused` | `resume` | `Running` | transition invalide, état inchangé |
| `Running`, `Paused`, `Halted`, `Stopped` | `reset` | `Running`, entrées hôte réinitialisées | état et entrées hôte inchangés |
| Tout état | rechargement réussi de contenu | `Running`, entrées hôte réinitialisées | état et entrées hôte inchangés |
| `Running` | `run_until_event` / `run_for_events` | reste `Running`, sauf événement `Halted` | l'état hôte reste inchangé |
| `Paused` | exécution | transition invalide | reste `Paused` |
| `Halted`, `Stopped` | exécution | transition invalide | reste dans le même état |
| `Running`, `Paused` | entrée / libération globale | état inchangé; entrée locale mise à jour après succès du cœur | la transition refusée n'est pas marquée comme appliquée |
| `Empty`, `Halted`, `Stopped` | entrée | transition invalide | état inchangé |
| Tout état chargé (`Running`, `Paused`, `Halted`, `Stopped`) | save state et récupération de média | état de session inchangé | erreur explicite; les capacités doivent être annoncées |
| `Empty` | save state ou récupération de média | transition invalide | état inchangé |

## Règles transversales

- Le chargement de contenu est refusé avant l'appel au cœur si `content_loading` n'est pas annoncé.
- Le chargement borné rejette le contenu vide ou trop grand avant d'appeler le cœur; un rejet ne modifie ni l'état de session ni l'état d'entrée.
- Une erreur de cœur ne doit pas être interprétée comme une transition réussie. Pour le chargement et la restauration d'état, le contrat du cœur exige une validation avant mutation.
- `run_for_events(n)` compte les appels au cœur, pas les cycles ni le temps mural. Il s'arrête au budget ou au premier événement autre que `Yielded`; un événement `Halted` fait passer la session à `Halted`.
- La libération globale d'entrées est progressive : les contrôles libérés avec succès sont retirés immédiatement; un contrôle dont la libération échoue reste marqué comme maintenu afin de permettre une nouvelle tentative.
- `stop` arrête l'exécution pilotée par l'hôte sans décharger le contenu. Il libère d'abord les commandes maintenues; si une libération échoue, la session ne passe pas à `Stopped` et les contrôles déjà libérés restent réconciliés. `reset` ou le chargement réussi d'un nouveau contenu permet de reprendre. `Halted` reste distinct : il signifie que le cœur a signalé l'arrêt.

## Portée des garanties

Les tests de `runtime.rs` couvrent les transitions invalides principales, la pause/reprise, l'arrêt du runner sur `Halted`, la récupération par réinitialisation ou rechargement, les erreurs transitoires d'exécution, les échecs de réinitialisation/rechargement, les entrées maintenues et les limites du chargement borné. La matrice doit encore être complétée pour toutes les catégories d'erreur et tous les états/capacités.
