# IR Analyzer — Application Flipper Zero

Analyse et décode les signaux infrarouge en temps réel directement sur ton Flipper Zero.

## Fonctionnalités

- Réception et décodage automatique des signaux IR (NEC, Samsung, RC5, Sony, Kaseikyo, RAW…)
- Affichage en temps réel du protocole, de l'adresse et de la commande
- Liste scrollable de tous les signaux capturés (jusqu'à 32)
- Vue détaillée par signal avec toutes les infos décodées
- LED verte qui clignote à chaque signal reçu

## Structure des fichiers

```
ir_analyzer/
├── application.fam                        ← Déclaration de l'app (ufbt)
├── ir_analyzer.h                          ← Types et struct principale
├── ir_analyzer.c                          ← Init, worker IR, point d'entrée
└── scenes/
    ├── ir_analyzer_scene.h                ← Headers communs des scènes
    ├── ir_analyzer_scene.c                ← Table des handlers
    ├── ir_analyzer_scene_main.c           ← Écran principal (live)
    ├── ir_analyzer_scene_signal_list.c    ← Liste des signaux capturés
    └── ir_analyzer_scene_signal_detail.c  ← Détail d'un signal
```

## Compilation et installation

### Prérequis

- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) installé
- Flipper Zero connecté en USB

### Étapes

```bash
# 1. Copier le dossier ir_analyzer dans un emplacement de travail
# 2. Ouvrir un terminal dans ce dossier

# Compiler
ufbt

# Compiler ET lancer directement sur le Flipper (le plus pratique)
ufbt launch

# Ou compiler et copier le .fap manuellement sur la SD
ufbt build
# → Le .fap se trouve dans dist/
```

## Utilisation

1. Lancer l'app depuis `Apps > Infrared > IR Analyzer`
2. L'écran affiche **"En attente d'un signal"**
3. Pointer n'importe quelle télécommande vers le Flipper et appuyer sur un bouton
4. Le protocole, l'adresse et la commande s'affichent instantanément
5. Appuyer sur **OK** pour voir la liste de tous les signaux capturés
6. Sélectionner un signal pour voir son détail complet

## Navigation

| Bouton | Action |
|--------|--------|
| OK     | Ouvrir la liste des signaux |
| Back   | Revenir / Quitter |
| Haut/Bas | Naviguer dans la liste |

## Protocoles supportés

Tous les protocoles supportés nativement par le firmware Flipper :
NEC, NEC42, Samsung32, RC5, RC5X, RC6, SIRC, SIRC15, SIRC20, Kaseikyo, RCA, RAW

---

*App créée par Adrien — compatible avec Momentum, Unleashed, OFW*
