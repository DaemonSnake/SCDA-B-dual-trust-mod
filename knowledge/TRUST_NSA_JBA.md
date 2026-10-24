# Confiance NSA / JBA : localisation et préparation de deux jauges

**Référence actuelle : [TRUST_REFERENCE.md](TRUST_REFERENCE.md)**, avec inventaire
des déclencheurs, contexte narratif et propositions NSA/JBA. Le présent document
conserve la première localisation technique, corrigée après l'analyse des scripts.

Analyse statique du 23 octobre 2026. Adresses virtuelles du XBE, et non adresses
hôte du processus PC. Empreinte du binaire dans `MODLOG.md`.
Les conclusions ci-dessous proviennent exclusivement des fichiers locaux.

## Résultat

La confiance utilisée par l'interface est une valeur unique, lue comme un octet
à `objet + 0x360`. L'objet est obtenu depuis le gestionnaire de menus par
`sub_0017B720`. Son nom de classe exact reste à confirmer.

L'extrémité 0 active `logo_NSA`, l'extrémité 100 active `logo_JBA`.
Ces logos indiquent le camp en difficulté : **les deltas positifs favorisent
la NSA, les négatifs la JBA**. La première interprétation inverse de ces
extrémités était erronée et est remplacée par la lecture des scripts et objectifs.
Les valeurs intermédiaires parcourent l'animation `A_trust`.
Le bilan calcule des contributions positives et négatives, les soustrait,
puis transmet un delta unique à l'événement `ModifyTrust`.

Deux jauges exigent donc deux états et une classification avant cette soustraction.
Créer `NSA = 100 - JBA` reproduirait exactement le couplage existant.

## Points d'entrée vérifiés

| Adresse | Rôle observé | Source |
| --- | --- | --- |
| `0x00092413` | Wrapper d'appel de l'événement script `ModifyTrust(int)` | `generated/game/x86_recomp.14.cpp`, ligne 19943 |
| `0x00095B32` | Entrée VM : lit montant et second paramètre, inverse le montant si ce paramètre est non nul, appelle le wrapper | `generated/game/x86_recomp.15.cpp`, ligne 3 |
| `0x000E9CE4` | Recherche un enregistrement par identifiant, traite son changement d'état et applique son montant de confiance | `generated/game/x86_recomp.22.cpp`, ligne 5409 |
| `0x000E9B32` | Compte les entrées d'un type statistique donné | `generated/game/x86_recomp.22.cpp` |
| `0x001BB69C` | Agrège les statistiques, applique le delta et stocke le résultat dans l'historique | `generated/game/x86_recomp.41.cpp`, ligne 28969 |
| `0x001BA097` | Recalcule le delta pour la prévisualisation du bilan | `generated/game/x86_recomp.41.cpp`, ligne 24760 |
| `0x001BDA0C` | Résout `AInst_Trust`, `A_trust`, `logo_JBA`, `logo_NSA` | `generated/game/x86_recomp.42.cpp`, ligne 4556 |
| `0x001BDB21` | Positionne l'animation sur la valeur transmise | `generated/game/x86_recomp.42.cpp`, ligne 4766 |
| `0x001BDC15` | Active le logo NSA à 0 et le logo JBA à 100 | `generated/game/x86_recomp.42.cpp`, ligne 4995 |
| `0x000C357D` | Lit `TRUSTVALUE=` dans les options et écrit l'octet `+0x360` | `generated/game/x86_recomp.18.cpp`, ligne 23857 |
| `0x0018121F` | Gros répartiteur de menus : affichage, initialisation et restauration de confiance entre autres | `generated/game/x86_recomp.36.cpp`, ligne 7699 |

### Pourquoi `0x00092413` est bien `ModifyTrust`

L'initialiseur `0x0011BE6D` enregistre la chaîne UTF-16 `ModifyTrust`
à `0x00522750` et conserve son identifiant dans `0x0064E924`.
Le wrapper lit cet identifiant, résout la fonction via `0x000603DA`, puis
déclenche l'appel virtuel `vtable + 0x10` avec le delta en paramètres.

**Ce wrapper n'est pas le corps de la fonction.** Le corps a depuis été retrouvé
dans le flux décompressé de `common.lin`, en-tête `0x17f295`, code `0x17f2a6` :
la mise à jour ajoute le delta puis borne `Trust` entre 0 et 100. Voir la nouvelle
référence pour les preuves et la limite concernant les effets annexes non testés.

Deux autres noms sont enregistrés : `GetEchelonTrustLevel` → identifiant
dans `0x0064E850`, wrapper `0x001429C3`; et
`ConvertTrustValueToTrustLevel` → identifiant dans `0x0064E7D4`.
Les consommateurs des niveaux de confiance doivent également être examinés.

## Objectifs : la classification existe, mais sous une forme couplée

Dans `0x000E9CE4`, le conteneur possède un tableau à `+0xC0`, un nombre
d'éléments à `+0xC4`, avec un pas de **0x58 octets**.
La comparaison d'identifiant passe par `0x000D28B9` avec `entrée + 0x24`.

Champs observés de chaque entrée :

| Offset | Utilisation constatée |
| --- | --- |
| `+0x24` | Identifiant comparé à l'identifiant demandé |
| `+0x38` | Montant entier de confiance |
| `+0x40` | État, écrit depuis le nouvel état transmis |
| `+0x54`, masque `0x02` | Inverse le signe du montant |

L'application du montant est conditionnée notamment par un nouvel état égal
à 1, un montant non nul et un drapeau du contexte. Avec le drapeau de signe
absent, le montant est transmis tel quel; avec ce drapeau, il est négatif.
Pour un montant positif, cela correspond respectivement aux directions
NSA et JBA de la jauge. Les montants réels sont maintenant inventoriés dans la
nouvelle référence. Il reste à vérifier dynamiquement le sens
métier des états dans les ressources avant de nommer les champs définitivement.

Le bon endroit pour une règle par objectif conserve **mission + identifiant
d'objectif + transition d'état**, avant de convertir le tout en delta.
Un simple hook du delta final ne sait plus quel objectif l'a produit.

## Actions générales : cinq groupes au bilan

Le bilan lit l'historique statistique via `objet + 0x710`.
`0x000E9B32` parcourt un tableau à `+0xA8`, de taille `+0xAC`, avec
des entrées de 0x14 octets; le premier octet est l'identifiant de type.

La formule de `0x001BB69C` utilise cinq groupes :

| Types statistiques | Coefficient sur l'objet | Drapeaux sur l'objet référencé par `objet + 0xF4` |
| --- | --- | --- |
| `2 + 5` | `+0x4EC` | `+0xC60` |
| `0 + 3` | `+0x4F0` | `+0xC64` |
| `7` | `+0x4F4` | `+0xC68` |
| `10` | `+0x4F8` | `+0xC6C` |
| `6` | `+0x4FC` | `+0xC70` |

Pour chaque groupe, masque `0x01` = contribution activée;
masque `0x02` = ajout dans l'accumulateur positif; sinon ajout dans celui
qui sera soustrait. Chaque accumulateur est borné séparément entre 0 et 15.

Le calcul observé équivaut à :

```text
positif = clamp(somme des groupes positifs, 0, 15)
negatif = clamp(somme des groupes négatifs, 0, 15)
cible = clamp(confiance_actuelle + positif - negatif, 5, 95)
ModifyTrust(cible - confiance_actuelle)
```

Ces bornes **5..95 sont celles de ce chemin de bilan**, pas une preuve des
bornes générales de `ModifyTrust`. Le répartiteur de menus contient aussi
un appel avec `100 - valeur_actuelle`, et le HUD gère les extrémités 0 et 100.

La prévisualisation `0x001BA097` duplique cette formule en utilisant une
valeur de départ conservée dans le menu à `+0xB0`; elle écrit le delta à
`menu + 0xA0`. Il faut adapter les deux chemins pour garder le bilan cohérent.

Les catégories ont depuis été recoupées avec le bilan et ses textes :
0/3 = ennemis/civils assommés, 2/5 = ennemis/civils tués, 7 = repérages,
10 = alarmes, 6 = corps découverts. La nouvelle référence distingue cette
identification des coefficients par mission, qui restent à extraire.

## Affichage et persistance

Le HUD passe par `0x0019B7A1`, puis `0x001BDB21` et `0x001BDC15`.
L'écran OPSAT utilise `P_Trust`; le bilan utilise `P_TrustReview` et
`Lst_TrustOverall`. Un second affichage doit couvrir ces écrans et leurs
notifications, pas uniquement dessiner deux barres dans le HUD.

Le bilan stocke la valeur dans un historique d'octets à
`base_profil + index_mission + 0x1C4B`. Le chemin console/changement de carte
`0x001CEB87` lit et écrit aussi cette zone, et transmet `?TRUSTVALUE=%d`.
Le répartiteur `0x0018121F` restaure des valeurs de cet historique et contient
une initialisation explicite à **75** dans un de ses chemins. Ce n'est pas
une preuve que toutes les nouvelles parties démarrent à 75.

Il faudra traiter sauvegarde, chargement de checkpoint, changement de carte,
redémarrage de mission et historique. Deux variables globales hôte ne suffisent
pas : elles peuvent survivre à un rechargement qui devrait revenir en arrière.
Ne pas réutiliser les octets voisins de `+0x360` sans établir leur propriété.

## Architecture proposée — à implémenter après identification des événements

Un événement possède deux effets explicites, par exemple
`{ delta_nsa: +5, delta_jba: 0 }`, avec bornage indépendant de chaque jauge.
Les cas `(+5,+5)`, `(-5,-5)` et `(+5,-5)` restent possibles lorsqu'une règle
de gameplay les justifie. Aucun effet opposé automatique.

Les clés de règles doivent distinguer :

- Objectif : mission, identifiant, transition d'état.
- Action : type statistique identifié, contexte pertinent, moment d'application.
- Appel script direct : fonction/appelant et contexte de mission.

Une règle inconnue doit être journalisée comme non classée. Le signe de
l'ancien delta ne permet pas de déduire à lui seul l'effet sur les deux camps.
Les déclenchements répétés et les actions déjà comptabilisées au bilan doivent
être distingués pour éviter un double gain ou une double pénalité.

Il faudra aussi décider explicitement des valeurs initiales, des seuils d'échec,
des conditions narratives et des récompenses d'équipement. Les chaînes
`LOADOUT_JBA_TRUST` et `LOADOUT_ECHELON_TRUST` sont présentes dans le XBE;
leur présence seule n'établit pas leurs conditions de déblocage.

## Vérifications restantes avant un mod fonctionnel

1. Le corps UnrealScript de `ModifyTrust` et le catalogue des objectifs constants
   sont désormais identifiés; vérifier les effets annexes et les appels dynamiques.
2. Extraire les coefficients statistiques par mission et raccorder les variantes
   narratives, notamment la fausse mort d'Hisham, aux conditions exactes des scripts.
3. Tracer en jeu un objectif, un appel script direct et chaque groupe de bilan :
   identifiant, contexte, ancien état, nouveau, montant et écritures de confiance.
4. Introduire les deux états persistants, les règles et les deux affichages,
   puis adapter les lecteurs des seuils et les bilans.
5. Vérifier gain NSA seul, gain JBA seul, effets sur les deux camps,
   répétition d'événement, bilan, checkpoint et transition de mission.

À ce stade, la localisation et les calculs natifs ci-dessus sont établis par
lecture statique. Le jeu n'a pas été lancé et aucun comportement à deux jauges
n'est encore installé ou validé.
