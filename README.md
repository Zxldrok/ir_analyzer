# IR Analyzer — Application Flipper Zero

Analyse, décode, rejoue et retransmet les signaux infrarouges en temps réel sur Flipper Zero.

## Fonctionnalités

- **Capture** de tout signal IR (NEC, Samsung, RC5, Sony, Kaseikyo, RAW…)
- **Déduplication** — les signaux identiques sont comptés, pas dupliqués
- **Turbo burst** — envoie le signal 1 à 5 fois en rafale
- **Repeater mode** — retransmet automatiquement tout signal reçu avec un délai configurable (50–1000ms)
- **Save** — sauvegarde un ou tous les signaux au format `.ir` sur la carte SD
- **Delete** — supprime un signal de la liste
- **Waveform** — visualisation graphique des timings pour les signaux RAW
- **Session stats** — durée de session et nombre de signaux capturés
- **LED feedback** — verte (nouveau signal), jaune (déjà vu), bleue (transmission)

## Structure

```
ir_analyzer/
├── application.fam       ← Déclaration de l'app (ufbt)
├── ir_analyzer.h         ← Types, structs, defines
├── ir_analyzer.c         ← Tout le code : init, worker IR, UI, input
├── ir_analyzer_icon.png  ← Icône 10x10
└── .gitignore
```

## Compilation

```bash
# Prérequis : uFBT (https://github.com/flipperdevices/flipperzero-ufbt)
ufbt          # Compiler
ufbt launch   # Compiler + lancer sur Flipper branché
```

Le `.fap` est généré dans `dist/`.

## Navigation

### Écran Live
| Bouton | Action |
|--------|--------|
| OK | Liste des signaux |
| Back long | Quitter l'app |
| Long OK | Activer/désactiver le **repeater mode** |
| ← / → | Ajuster le délai du repeater |

### Liste des signaux
| Bouton | Action |
|--------|--------|
| ↑ / ↓ | Naviguer |
| ← / → | Sauter de 4 en 4 |
| OK | Détail du signal |
| Long OK | **Sauvegarder tous** les signaux |
| Back | Retour live |
| Long Back | **Effacer toute** la liste |

### Détail d'un signal
| Bouton | Action |
|--------|--------|
| ← / → | Changer le nombre de répétitions (turbo 1–5) |
| OK | **Sauvegarder** le signal |
| Long OK | **Transmettre** le signal (× turbo repeats) |
| Back | Retour liste |
| Long Back | **Supprimer** le signal |

### Transmission
| Bouton | Action |
|--------|--------|
| Back | Arrêter la transmission |

## Protocoles supportés

NEC, NEC42, Samsung32, RC5, RC5X, RC6, SIRC, SIRC15, SIRC20, Kaseikyo, RCA, RAW.

---

*App créée par Adrien — compatible Momentum, Unleashed, OFW*
