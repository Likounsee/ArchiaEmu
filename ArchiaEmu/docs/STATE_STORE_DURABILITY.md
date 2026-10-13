# Garanties de persistance du state store

Ce document décrit les garanties actuellement fournies par `ModuleStateStore`. Il distingue la récupération logique de la durabilité face à une coupure de courant : ces propriétés ne sont pas interchangeables.

## Protocole de sauvegarde

1. L'état est encodé et refusé s'il dépasse 4 Mio.
2. Un fichier temporaire est créé dans le même répertoire que le fichier principal, écrit intégralement puis synchronisé avec `File::sync_all`.
3. Le fichier principal existant est déplacé vers une sauvegarde avant l'installation du temporaire.
4. Si l'installation échoue, le code tente de restaurer le fichier principal. Si cette restauration échoue également, la sauvegarde est conservée pour permettre une récupération ultérieure.
5. Une erreur de synchronisation du répertoire après remplacement est renvoyée à l'appelant. À ce stade, le nouveau principal peut déjà être visible; une erreur ne signifie donc pas que l'opération n'a rien modifié.

Les écritures concurrentes sur un même chemin doivent être sérialisées par l'application hôte.

## Garanties par plateforme

- **Unix (dont Linux et macOS)** : après le remplacement, le code ouvre le répertoire parent et appelle `sync_all`. Les erreurs de cette opération sont propagées. Cela renforce la persistance de l'entrée de répertoire, sans garantir à lui seul le comportement de tous les systèmes de fichiers ou de tous les périphériques.
- **Windows et autres plateformes non Unix** : la synchronisation explicite du répertoire parent est actuellement une opération sans effet. Le contenu temporaire est synchronisé avant le remplacement, mais la persistance du renommage après une coupure brutale n'est pas garantie par ce mécanisme. Le code ne doit pas être présenté comme offrant une durabilité identique à Unix.

## Couverture et travaux restants

La suite de tests injecte les échecs de renommage et de synchronisation, vérifie la restauration du principal, la conservation d'une sauvegarde récupérable et le nettoyage des fichiers temporaires. Le workflow CI exécute les tests sur Linux, Windows et macOS; cette exécution vérifie le comportement fonctionnel sur ces plateformes, mais ne simule pas une coupure d'alimentation.

La tâche P0 reste ouverte tant qu'une stratégie explicite de synchronisation/récupération Windows et sa validation dans la matrice CI n'ont pas été décidées. Ne pas remplacer la synchronisation non prise en charge par un succès silencieux tout en affirmant une durabilité complète.
