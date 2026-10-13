# Intégration du pipeline de chargement dans l'hôte

Ce document définit le contrat d'intégration entre une future application hôte et le runtime commun. Il ne prétend pas fournir une interface graphique complète.

## Flux recommandé

1. **Construire le registre** à partir des modules réellement installés et compatibles avec la version d'API et la cible de l'hôte. Un descripteur déclaratif ne suffit pas à prouver qu'un cœur sait exécuter un système.
2. **Fournir des sondes explicites** (ContentFormatProbe) pour les formats que l'hôte souhaite détecter. Une sonde examine uniquement les octets reçus; elle ne doit pas inférer le format à partir du nom de fichier, de l'extension ou du chemin.
3. **Choisir une limite de taille** adaptée à la politique de l'hôte via ContentLimits. La taille indiquée par les métadonnées du fichier ne remplace pas la limite appliquée à la lecture effective.
4. **Appeler le point d'entrée adapté à la source** :
   - launch_content_path pour un chemin de fichier;
   - launch_content_reader pour un flux fourni par l'hôte;
   - launch_content pour un tampon déjà en mémoire.
5. **Présenter les erreurs sans deviner** : LaunchError::Content signale un contenu vide, trop grand ou illisible; LaunchError::Registry signale notamment l'absence, l'ambiguïté ou l'incomplétude de la détection; LaunchError::Session signale le refus du contenu par le cœur.
6. **Ne créer l'interface de session qu'après un lancement réussi**. L'hôte doit ensuite respecter les capacités annoncées par le module et les états de session; le succès du chargement ne prouve pas que l'exécution complète, la vidéo ou l'audio sont implémentés.

## Exemple d'intégration

- Créer un CoreRegistry.
- Enregistrer un cœur installé et compatible, par exemple via register_game_boy.
- Construire une instance de GameBoyCartridgeProbe et la passer comme tableau de références ContentFormatProbe.
- Appeler launch_content_path avec ContentLimits explicite, le tableau de sondes et le registre.
- Propager l'erreur LaunchError à l'interface au lieu de choisir arbitrairement un cœur.
- Après le lancement, inspecter les capacités et les états de session avant d'activer les commandes correspondantes.

L'exemple montre le branchement du pipeline, pas une promesse que le cœur Game Boy est complet. La sonde identifie un candidat; la validation du header et de la ROM ainsi que les capacités d'exécution restent des responsabilités distinctes.

## Frontière de responsabilité

- Le runtime commun applique les limites, détecte les candidats, refuse les résultats ambigus/incomplets et transmet un contenu possédé et borné.
- Chaque cœur valide sa structure spécifique avant de muter son état. Une sonde qui correspond à une signature n'est pas une validation complète.
- L'hôte gère l'interface utilisateur, la politique de limites, la découverte des modules et la présentation des erreurs; il ne réimplémente pas les parseurs propriétaires.
- Aucun fallback silencieux vers un autre cœur ne doit être utilisé quand la détection échoue ou retourne plusieurs candidats.
