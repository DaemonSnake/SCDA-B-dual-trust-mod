# Référence — déclencheurs de confiance et séparation NSA / JBA

Version **0.1 — inventaire statique et proposition de règles**, 23 octobre 2026.

Une première implémentation native est disponible. Voir [TRUST_IMPLEMENTATION.md](TRUST_IMPLEMENTATION.md) pour les comportements réalisés, preuves et limites; les propositions ci-dessous restent distinctes des validations en jeu.
Jeu : Splinter Cell Double Agent, **Xbox originale**, assets locaux de ce projet.

Ce document est la référence de conception pour remplacer le système à valeur
unique par deux confiances indépendantes. Il décrit les événements identifiés,
leur contexte narratif, les valeurs originales et les valeurs proposées.
Les propositions ne sont pas encore installées dans le jeu.

## Lecture et niveau de preuve

- **Établi par les fichiers** : identifiants, montants constants, drapeaux,
  appels natifs, textes d'objectifs et dialogues associés.
- **Interprétation narrative** : ce que les actions signifient pour chaque camp,
  en tenant compte de ce qu'il sait réellement.
- **Proposition** : nouveaux deltas NSA/JBA; choix de conception explicites,
  pas valeurs attribuées aux développeurs du jeu original.
- **À valider en jeu** : atteignabilité de toutes les branches, notifications,
  rejouabilité des événements, séquences exactes de découverte et équilibrage.

L'inventaire couvre les appels constants reconnus dans toutes les cartes solo
installées, avec contrôle supplémentaire des cartes coop et du code commun.
Il ne constitue pas une preuve formelle d'absence d'appels indirects ou à arguments
dynamiques. Aucun parcours complet du jeu n'a été enregistré pour cette recherche.

## Résultats de l'inventaire

| Ensemble | Résultat statique |
| --- | ---: |
| Cartes solo examinées | 17 |
| Déclarations d'objectifs solo | 139 |
| Règles solo distinctes après regroupement section + ID + montant | 111 |
| Objectifs narratifs distincts à montant non nul | **45** |
| Objectifs de récupération de confiance | 19 |
| Déclarations de test `TEMPNSA` / `TEMPJBA` | 10 |
| Objectifs distincts à montant nul | 37 |
| Appels constants de changement d'état solo | 208 |
| Appel direct constant `EPattern.ModifyTrust` retenu | 1 |
| Déclarations d'objectifs coop examinées | 44, toutes à montant nul |

Les deux cartes du QG embarquent du code des **deux visites**. Une déclaration
présente dans chaque LIN n'est donc pas une récompense à appliquer deux fois.
Des objectifs sont aussi déclarés dans une carte et complétés dans la suivante.

Les adresses de chaque déclaration et changement d'état sont dans
[TRUST_TRIGGER_INVENTORY.md](TRUST_TRIGGER_INVENTORY.md).
La version structurée des règles, avec toutes leurs origines et transitions
constantes associées, est [TRUST_RULES_REFERENCE.json](TRUST_RULES_REFERENCE.json).

## Correction du sens de la valeur originale

**Delta positif = orientation NSA; delta négatif = orientation JBA.**
La propriété s'appelle `EPlayerController.Trust` et correspond à l'octet `+0x360`
du contrôleur utilisé par les menus. La lecture initiale « 0 = NSA, 100 = JBA »
était erronée : les logos activés aux extrémités signalent le camp en difficulté.
Cette correction remplace cette interprétation dans le premier rapport.

La déclaration de Barnham vivant vaut **+15**; celle de sa mort vaut **−15**.
Le paramètre de direction est cohérent avec le nom `bJBATrust` trouvé dans les
noms de scripts : vrai inverse le signe du montant.

Le corps script `EPlayerController.ModifyTrust` a été retrouvé dans `common.lin`.
Son en-tête est au décalage décompressé `0x17f295`; son code commence à
`0x17f2a6` et annonce 102 octets. La partie de mise à jour correspond à
`Trust = clamp(Trust + trustpt, 0, 100)`, entourée de traitements de niveaux
de confiance. L'addition et le clamp ont été recoupés avec leurs fonctions
natives, respectivement `0x00069954` et `0x00069FED` → `0x00036D16`.
Le détail des effets annexes n'a pas encore été validé dynamiquement.

## Les chemins qui déclenchent réellement ModifyTrust

```text
Script de mission
  → AddGoal(..., montant, drapeau JBA)
  → SetGoalStatus(ID, complété)
  → traitement natif des objectifs
  → EPlayerController.ModifyTrust(delta)

Script de mission → EPattern.ModifyTrust(montant, bJBATrust)
  → EPlayerController.ModifyTrust(delta)

Bilan → comptage des actions → calcul des règles de mission
  → EPlayerController.ModifyTrust(delta)

Menu / commande technique → EPlayerController.ModifyTrust(delta)
```

Le wrapper événement est à `0x00092413`. Ses quatre sites d'appel natifs
identifiés sont :

| Adresse de l'appel | Origine | Traitement dans le futur système |
| --- | --- | --- |
| `0x00095B86` | Native VM `EPattern.ModifyTrust` | Classer l'appel par son contexte; ne pas déduire deux effets de son seul signe |
| `0x000E9D59` | Objectif complété | Chercher la règle par mission, section et ID |
| `0x001BB92A` | Bilan des actions générales | Traiter les catégories avant leur réduction en delta unique |
| `0x00181284` | Répartiteur de menus, delta `100 - Trust` | Commande technique, aucune récompense narrative |

Identifiants natifs confirmés par les initialiseurs du XBE :

| Opération | Native VM | Octets d'appel | Fonction native |
| --- | --- | --- | --- |
| `EPattern.AddGoal` | `0xD05` | `6D 05` | `0x0009B19F` |
| Changement d'état via EPattern | `0x8AE` | `68 AE` | `0x0009B346` |
| Changement d'état via le conteneur de statistiques | `0x3EB` | `63 EB` | `0x000EA3F2` |
| `EPattern.ModifyTrust` | `0xFC0` | `6F C0` | `0x00095B32` |

Dans les appels `AddGoal`, le **13e paramètre** fournit le montant de confiance,
le **12e** le drapeau qui inverse son signe. Cela a été suivi à travers
`0x0009B19F` → `0x000EA452`, jusqu'à `entrée + 0x38` et masque `0x02` de
`entrée + 0x54`. Les autres paramètres intermédiaires ne sont pas des montants.
Les paramètres optionnels omis sont pris avec leurs valeurs natives par défaut.

Le traitement à `0x000E9CE4` applique le montant sur un état demandé égal à 1.
Il n'a pas de vérification visible « ancien état déjà complété » avant cet
appel. Il faut donc protéger les attributions répétées dans le nouveau système,
même si les scripts de mission protègent déjà certaines branches.

## Règles de conception retenues pour cette proposition

Les deux jauges vont de 0 à 100 et sont bornées séparément. Les valeurs ci-dessous
sont des **points de pourcentage**, pas un pourcentage multiplicatif.

1. Préserver les ordres de grandeur originaux : 5 pour une opportunité,
   10 pour une étape importante, 15 pour un choix personnel majeur,
   30 pour le sort d'un attentat. Garder les objectifs originellement nuls à 0.
2. Une opération clandestine réussie pour la NSA ne fait pas automatiquement
   perdre de la confiance JBA. Une perte demande une découverte, un échec
   perceptible ou un refus dont la JBA a connaissance.
3. Aider la JBA dans une opération nécessaire à l'infiltration ne fait pas
   automatiquement perdre de la confiance NSA. Les meurtres d'innocents,
   le sacrifice d'alliés et les attentats conservent leurs enjeux.
4. Autoriser les bénéfices pour les deux camps quand le contexte les justifie.
5. Séparer l'action secrète de sa conséquence découverte, sans doubler son
   montant. Les tables donnent parfois un résultat final et sa variante secrète.
6. Une récompense par objectif et par mission; une récompense par bombe distincte.
   Les variantes exclusives remplacent l'effet de la branche précédente.

Ces principes conservent les priorités narratives de l'original tout en supprimant
la pénalité automatique envers le camp opposé. Ils demandent une passe
d'équilibrage sur une campagne réelle; les anciens totaux ne se transposent pas
directement à deux jauges.

## Catalogue narratif des 45 objectifs à confiance non nulle

Dans les tables : **ancien** désigne le delta de la jauge unique, avec +NSA et
−JBA. **NSA/JBA proposés** sont deux variations indépendantes. Les conditions
de la dernière colonne font partie de la règle.

### Prison d’Ellsworth

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `BarnhamAlive` | Épargner Barnham | +15 | +15 | -15 | Témoin du DOJ protégé; Jamie constate personnellement le refus de le tuer. |
| `Retrieve` | Récupérer le disque sur Jamie | +5 | +5 | +0 | Renseignement clandestin pour Third Echelon; aucune découverte JBA établie. |
| `NeedSeringe` | Trouver la seringue pour Jamie | -10 | +0 | +10 | Préserver le contact qui permet l'infiltration; la NSA a autorisé cette opération. |
| `BarnHamDead` | Aider Jamie à tuer Barnham | -15 | -15 | +15 | Tuer le témoin contredit l'ordre de Lambert et rassure directement Jamie. |
| `Regroup` | Retrouver Jamie | -10 | +0 | +10 | Maintenir Jamie en vie et poursuivre l'infiltration autorisée. |
| `Disc` | Falsifier les registres de décès | -5 | +0 | +5 | Faciliter l'évasion et renforcer la couverture; pas de trahison NSA supplémentaire. |

### QG JBA — première visite

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQA_P_10` | Réduire le brouillage de la JBA | +10 | +10 | +0 | Ouvrir une fenêtre de surveillance NSA en secret; perte JBA seulement si une autre scène établit la découverte. |
| `CarsonMoss` | Renforcer la fausse identité de Sam | -5 | +0 | +5 | Falsifier le contrôle de sécurité de Moss sert aussi l'infiltration autorisée par Lambert. |
| `YeagerLambert` | Transmettre Yeager à Lambert | +15 | +15 | +0 | Lambert prévoit de le faire disparaître avant Emile; ne pas attribuer automatiquement cette exfiltration à Sam côté JBA. |
| `YeagerEmile` | Dénoncer Yeager à Emile | -15 | -15 | +15 | Écarter la piste que Lambert voulait exploiter et fournir à Emile la preuve qu'il attend. |
| `OBJ_HQA_P_20` | Ouvrir le serveur JBA à la NSA | +10 | +10 | +0 | Renseignement clandestin; la pénalité JBA relève d'une découverte avérée, pas de la seule réussite technique. |
| `OBJ_HQA_O_20` | Relever les empreintes d'Enrica | +5 | +5 | +0 | Collecte de renseignement clandestine; les autres empreintes à montant original nul restent neutres. |

### Train blindé

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `P_WoltCode` | Obtenir le code du coffre | -10 | +0 | +10 | Étape du braquage confié par Emile, nécessaire à la couverture. |
| `P_KillChief` | Tuer le chef de gare | -15 | -15 | +15 | Preuve de loyauté exigée par Emile; meurtre d'un innocent expressément interdit par Lambert. |
| `P_ChiefAlive` | Laisser le chef de gare vivant | +15 | +15 | -15 | Respecter l'ordre NSA tout en refusant une preuve de loyauté explicite. |
| `O_DetachCars` | Décrocher les wagons arrière | +5 | +5 | +0 | Éloigner des employés du danger sans empêcher le braquage principal. |
| `P_DisconnectCom` | Couper les communications du train | -10 | +0 | +10 | Retarder l'alerte et permettre le braquage sous couverture. |

### Cozumel

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `ChipCellPhone` | Saboter le déclencheur d'Enrica | +30 | +30 | -30 | Sauver les passagers; perte JBA à l'échec constaté de l'attentat. Pendant le sabotage secret seul: NSA +30, JBA 0. |
| `ProCellPhone` | Préserver le déclencheur de la bombe | -30 | -30 | +30 | Permettre volontairement l'attentat malgré l'ordre de Lambert; résolution opposée au sabotage, jamais cumulée avec lui. |
| `PlaceRelay` | Installer le relais avec Enrica | -10 | +0 | +10 | Maintenir la couverture avant le choix sur le téléphone; ne pas punir une seconde fois le résultat final. |
| `PassengerLog` | Récupérer le manifeste des passagers | +5 | +5 | +5 | La description sert l'identification des victimes et Enrica rapporte une demande explicite d'Emile: intérêt des deux camps. |
| `GetBomb` | Récupérer la bombe | -10 | +0 | +10 | Continuer la mission de couverture avec Enrica; le sort du navire possède son propre événement majeur. |
| `CameraRecord` | Effacer les enregistrements vidéo | +5 | +5 | +5 | Le texte exige de préserver le secret de la présence de Sam; préserver sa couverture protège les intérêts opérationnels des deux camps. |
| `ShipEngine` | Immobiliser le navire | +5 | +5 | +0 | Réduire les victimes à terre sans rendre ce sabotage automatiquement connu de la JBA. |

### Mer d’Okhotsk

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `Mercenaries` | Neutraliser les mercenaires du camp | -10 | +0 | +10 | Permettre aux alliés d'Emile d'aborder; l'objectif accepte létal ou non létal. Les pertes humaines sont évaluées séparément. |
| `Communications` | Couper le réseau des mercenaires | -10 | +0 | +10 | Empêcher l'alerte pendant la prise du pétrolier. |
| `Bombs` | Désarmer les charges dans la glace | -5 | +0 | +5 | Maintenir le pétrolier bloqué jusqu'à sa capture; ces charges ne sont pas les bombes d'attentat de la JBA. |
| `KeyCode` | Récupérer la clé de chiffrement | +5 | +5 | +0 | Surveiller le réseau des contrebandiers pour la NSA. |
| `Freight_Done` | Identifier la cargaison | +5 | +5 | +0 | Renseigner la NSA sur ce que la JBA pourrait exploiter; conserver une seule attribution malgré deux branches de complétion. |
| `Tracker_Done` | Poser le traceur du pétrolier | +5 | +5 | +0 | Suivre clandestinement le navire après sa capture par la JBA. |
| `Bombs_Done` | Empêcher le sabordage du pétrolier | -10 | +0 | +10 | Sauver le navire destiné à la JBA; ne pas confondre ce désamorçage avec un sabotage de ses attentats. |

### Kinshasa

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_10_P_50` | Vérifier les partenaires d'Emile | -10 | +0 | +10 | Confirmer le calendrier et la loyauté des deux autres groupes. |
| `OBJ_10_O_10` | Renseigner la NSA sur Takfir | +5 | +5 | +0 | Collecte clandestine sur un groupe terroriste. |
| `OBJ_10_O_20` | Renseigner la NSA sur Massoud | +5 | +5 | +0 | Même intention, cible et objectif distincts. |
| `OBJ_10_P_60` | Tuer Hisham | -15 | -15 | +15 | Exécuter un agent allié à la demande d'Emile; conserver la gravité du choix original même si Lambert privilégie la mission. |
| `HishamBody` | Faire exploser le véhicule censé contenir Hisham | -5 | +0 | +5 | Valider la preuve fournie à Emile. Un autre corps peut servir au simulacre: aucune perte NSA pour une fausse mort réussie; le meurtre réel est déjà pénalisé par son objectif. |
| `OBJ_10_P_30` | Sauver Hisham | +15 | +15 | +0 | Si la fausse mort convainc Emile: JBA 0. Si Emile le voit fuir, variante NSA +15 / JBA -15, attestée par les dialogues. |
| `OBJ_10_P_70` | Sécuriser l'extraction et le canon AA | -10 | +0 | +10 | Permettre le départ du groupe sous couverture; ne pas sanctionner des neutralisations obligatoires comme des victimes civiles. |

### QG JBA — seconde visite

| ID | Événement | Ancien | NSA | JBA | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQB_P_10` | Voler les contacts d'Emile | +10 | +10 | +0 | Obtenir les contacts pour Third Echelon sans informer la JBA. |
| `OBJ_HQB_P_20` | Analyser le mercure rouge | +10 | +10 | +0 | Vérifier l'authenticité de l'arme pour Williams; action clandestine. |
| `P_LambertNOTCover` | Révéler l'identité de Lambert | -15 | -15 | +15 | Conserver l'orientation morale du jeu original et le sacrifice d'un agent allié; Williams encourage pourtant ce choix: tension narrative à conserver explicitement. |
| `P_LambertCover` | Confirmer la couverture de Lambert | +15 | +15 | -15 | Sauver Lambert entretient les soupçons d'Emile selon le texte de l'objectif. |
| `OBJ_HQB_P_30` | Désamorcer la bombe de Nashville | +15 | +15 | -15 | Empêcher un attentat et compromettre le plan JBA; appliquer la perte JBA lorsque l'échec de la bombe est établi, pas au seul geste clandestin. |
| `OBJ_HQB_P_31` | Désamorcer la bombe de Los Angeles | +15 | +15 | -15 | Même règle que Nashville, avec un identifiant de bombe distinct. |
| `KeepBombs` | Empêcher la neutralisation des bombes | -30 | -30 | +30 | Faire partir les bombes par une autre route compromet volontairement leur interception par la NSA. |


## Cas particuliers à implémenter avec leur contexte

### Chef de gare : annulation d'un résultat précédent

Un appel direct `ModifyTrust(15, true)` existe dans `x04_moneytrain_a`, offset
décompressé **`0x3579cc`**. Il suit les changements d'état de `P_KillChief` et
`P_ChiefAlive`, dans la branche nommée `Primary_Obj_Kill_Done_After_Alive`.
Le delta vaut −15; son rôle est de retirer le crédit antérieur du chef épargné.

Dans le futur système, mémoriser l'effet attribué au choix précédent et annuler
**cet effet exact** avant d'attribuer le choix meurtrier. Exemple sans saturation :
`épargné (+15,−15)` → annulation `(−15,+15)` → tué `(−15,+15)`.
Le résultat net du choix devient bien `(−15,+15)`, sans bonus résiduel.
Aux bornes 0/100, conserver aussi les valeurs avant choix ou les variations
effectives pour qu'une annulation ne crée pas de points par saturation.

### Hisham : mort réelle, mort simulée et mensonge découvert

Les textes des objectifs ne suffisent pas : `HishamBody` décrit le corps d'Hisham,
mais le dialogue `p_x10_kinshasa_b_hisham_script`, clés `Speech_0016L` à
`Speech_0020L`, introduit l'utilisation d'un autre corps de même gabarit.
Les rappels `p_x10_kinshasa_b_hisham_barks` demandent de préparer la voiture.

Le dialogue `p_x10_kinshasa_b_comms` distingue ensuite :

- Emile accepte la mort par explosion : clés `Speech_0020L` à `Speech_0024L`.
- Emile voit Hisham fuir alors que Sam prétend l'avoir tué : clés
  `Speech_0047L` à `Speech_0050L`.
- Les clés `Speech_0036L` à `Speech_0040L` ne constituent pas une preuve
  suffisante d'aveu : leur interprétation initiale était trop forte.

Règles : sauvetage réussi **NSA +15**; preuve convaincante **JBA +5**;
fuite constatée ou refus connu **JBA −15**, sans bonus de preuve. Meurtre réel :
**NSA −15 / JBA +15**, plus la preuve JBA +5 si son objectif est accompli.
Le bonus de preuve n'autorise pas un second crédit de meurtre.
Raccordement natif du 24 octobre : la sélection de `Speech_0050L` dans
`p_x10_kinshasa_b_comms` atteste la fuite constatée ; `Speech_0024L` atteste
l'acceptation du récit. Le hook observe l'appel `Localize` sélectionné par le
script, avant l'affichage des sous-titres. La branche de fausse mort réussie
complète `OBJ_10_P_30`, puis annule `OBJ_10_P_60` et `HishamBody`
(`x10_kinshasa_b`, offsets décodés `0x2e0e70`–`0x2e0eb7` et suite).
L'absence de preuve validée ne doit donc jamais, à elle seule, pénaliser la JBA.
Test natif avec objectifs et dialogues synthétiques : 75/75 → 90/75 → 90/80
→ fuite constatée 90/60, sans cumul lors des répétitions. La séquence normale
complète reste à vérifier en campagne.

### Cozumel : bénéfice partagé et conséquences différées

Le manifeste est décrit comme utile pour identifier les victimes éventuelles,
mais `p_x06_cozumel_a_enrika_coms:Speech_0082L` indique aussi une demande
explicite d'Emile. D'où la proposition **NSA +5 / JBA +5**.

Le virus du téléphone peut être posé secrètement. Réserver la perte JBA de 30
à l'échec de l'attentat une fois établi dans l'histoire, sans accuser Sam par
omniscience. Si l'objectif original de sabotage est aussi le seul point où le
jeu résout l'attentat, ce point peut porter les deux effets. Ce timing doit être
confirmé avant de coder la règle.

Le relais et la récupération de la bombe restent des preuves de coopération
JBA (+10 chacune), pas deux sanctions NSA supplémentaires alors que Sam peut
encore sauver le navire. Les morts éventuelles restent évaluées séparément.

### Lambert : préserver l'ambiguïté de Williams

Le dialogue `p_x11_hq5_r_communications_main:Speech_0015L` à `Speech_0022L`
montre Williams encourageant le sacrifice de Lambert pour assurer la couverture.
Pourtant, la balance originale donne +15 au sauvetage et −15 à sa dénonciation.

La proposition conserve **cette orientation originale**, plutôt que d'assimiler
automatiquement l'opinion de Williams à la confiance institutionnelle NSA.
C'est un choix de conception conscient; ce passage ne permet pas de prétendre
que « tous les ordres NSA exigent de sauver Lambert ».

## Déclencheurs de récupération : 19 règles, pas 19 choix de l'histoire

Les variantes de `P_TrustNSA`, `P_TrustJBA`, `P_TrustNSAHq2` et `TrustNow`
correspondent aux crises de couverture ou de loyauté.

| Famille | Original | Proposition | Contexte |
| --- | ---: | --- | --- |
| `P_TrustNSA`, `P_TrustNSAHq2` | +25 | NSA +25, JBA 0 | Contacter Williams / prouver que l'opération demeure sous contrôle |
| `P_TrustJBA` | −25 | NSA 0, JBA +25 | Contacter Emile pour rassurer la JBA |
| `TrustNow` | −25 | NSA 0, JBA +25 | Revenir en zone publique et se présenter à Emile au QG |

Les cartes du train, Cozumel, Okhotsk et Kinshasa possèdent leurs variantes;
le QG a ses branches propres. Le JSON et l'annexe gardent chaque origine.
Les appels de complétion correspondants ont été retrouvés.

Déclencher une crise sur **la confiance basse du camp concerné**, pas sur la
confiance élevée de l'autre. Les seuils exacts et délais restent ceux des scripts
à relever avant migration. Une seule récupération par épisode de crise; pas de
gain répétable en réutilisant le même ordinateur. Conserver les échecs narratifs
de délai sans inventer une mort automatique à un nouveau seuil.

## Déclencheurs techniques et objectifs neutres

### Dix déclarations de test

`TEMPNSA` vaut +20 et `TEMPJBA` −70, dans les deux cartes Cozumel,
Okhotsk B et les deux cartes Kinshasa. Les déclarations et leurs complétions
existent; les noms de branches `TempDebugNSA` / `TempDebugJBA` sont présents
dans les noms de scripts. Les entrées de localisation associées sont nulles.

Ils ne représentent pas dix actions narratives à équilibrer. Proposition :
**aucun effet en mode deux jauges**, et diagnostic si l'un se déclenche.
Leur atteignabilité normale n'a pas été démontrée; ne pas écrire qu'ils ont été
prouvés inaccessibles.

### Trente-sept règles solo à montant nul

Conserver **NSA 0 / JBA 0** pour ces événements : progression en Islande,
étapes de trajet/extraction, certaines empreintes et objectifs finaux de New York
notamment. Leur liste exacte est dans le JSON et les 139 déclarations de l'annexe.
Un objectif important dans l'histoire n'est pas forcément une source originale
de confiance; cette référence ne lui invente pas de récompense.

Les **44 déclarations coop** reconnues sont également nulles. La présente
refonte ne leur ajoute pas un système de loyauté.

### Écritures qui contournent l'événement

`TRUSTVALUE=` et l'historique inter-missions peuvent écrire/restaurer la confiance
sans passer par un objectif. Ce sont des chemins de persistance/initialisation,
pas des actions narratives. Ils devront restaurer deux valeurs cohérentes et
ne devront jamais être interprétés comme une récompense à rejouer.

## Actions générales au bilan

Le calcul de confiance du bilan à `0x001BB69C` et sa prévisualisation à
`0x001BA097` utilisent les mêmes groupes. Les catégories ont été recoupées avec
les compteurs affichés dans `0x001BBFE6` et les libellés locaux `MISSIONREVIEW`.
Le regroupement perd des distinctions utiles à deux jauges.

| Type(s) interne(s) | Catégorie | Coefficient original | Drapeaux de mission |
| --- | --- | --- | --- |
| 2 + 5 | Ennemis tués + civils tués | contrôleur +0x4EC | contexte +0xC60 |
| 0 + 3 | Ennemis assommés + civils assommés | +0x4F0 | +0xC64 |
| 7 | Repérages par l'ennemi | +0x4F4 | +0xC68 |
| 10 | Alarmes | +0x4F8 | +0xC6C |
| 6 | Corps découverts | +0x4FC | +0xC70 |

Les valeurs effectives des coefficients par mission ne sont pas encore
extraites dans cet inventaire. Ne pas présenter les nombres proposés ci-dessous
comme leurs valeurs originales. Les deux accumulateurs originaux sont plafonnés
à 15 et le résultat de ce chemin est borné à 5..95 avant `ModifyTrust`.

### Proposition de départ pour les actions répétables

Conserver l'application **au bilan**, les catégories activées par la mission
et un plafond de 15 par camp pour cette famille. Un objectif scénarisé déjà
récompensé ne doit pas produire une seconde récompense générique.

| Action | NSA proposé | JBA proposé | Condition |
| --- | ---: | ---: | --- |
| Tuer un civil ou policier protégé | −5 | 0 | Pas de récompense JBA automatique pour une victime sans rapport avec sa demande |
| Tuer un ennemi hors nécessité scénarisée | −2 | 0 | Seulement si la règle de retenue est active dans cette mission |
| Assommer un ennemi | +1 | 0 | Seulement si l'original valorise la méthode non létale; une fois par personnage |
| Assommer un civil sans nécessité | −1 | 0 | Ne pas compter comme la neutralisation professionnelle d'un hostile |
| Être repéré pendant une infiltration clandestine | −1 | 0 | Une fois par incident retenu dans les statistiques |
| Déclencher une alarme | −2 | 0 | Même périmètre que la règle d'alarme originale |
| Faire découvrir un corps | −1 | 0 | Même périmètre que la règle de discrétion originale |

Les actes attendus par Emile donnent déjà leur confiance JBA via les objectifs
explicites. Si l'histoire exige qu'une action répétable renforce la JBA, ajouter
une règle **mission + catégorie + cible/contexte**, jamais « tout meurtre = JBA + ».
À New York après rupture de couverture, et pour des neutralisations explicitement
ordonnées, ne pas réintroduire de pénalité de loyauté contraire au scénario.
La table est une base de réglage à tester, distincte des 45 règles extraites.

## Traçabilité, reproduction et contrôle de couverture

Binaire de référence `assets/default.xbe`, SHA256 :
`C742D61C9B8D8C1C4F886A4A4A69FFB65AB3961E0283DCAD33BFCFA963885143`.
Les empreintes des 33 LIN examinés (commun, menu, 17 solo, 14 coop) sont
conservées dans le JSON. Certains LIN ont des remplacements de textures issus
de travaux précédents : les offsets sont ceux des fichiers réellement examinés.

Outil local : [inventory_trust.py](../tools/analysis/inventory_trust.py).

```powershell
python tools/analysis/inventory_trust.py --output C:/Users/bastien/scda-trust-analysis
```

Il décompresse les LIN en vérifiant chaque taille de bloc, lit les appels
constants et leurs fins de paramètres, extrait les textes anglais pour le
travail privé et écrit son inventaire **hors du projet**. Il refuse une empreinte
XBE différente, car les numéros natifs sont propres à cette version.
Les textes complets, scripts et flux décompressés restent hors du dépôt.

Vérifications effectuées :

- 183 déclarations constantes reconnues, dont 139 solo et 44 coop;
  aucune erreur de lecture sur ces déclarations.
- Montant et direction reliés aux écritures natives des objectifs.
- Les 74 règles solo distinctes à montant non nul ont au moins un appel
  constant de complétion associé dans leur famille de mission.
- Les doublons de QG et les déclarations alternatives du chef de gare sont
  regroupés sans être supprimés de l'annexe des origines.
- Le seul appel direct constant retenu est relié à sa branche de compensation.
- Les faux motifs supplémentaires `6D 05 1F` observés hors des appels de
  localisation sont dans les données graphiques examinées, pas des objectifs.

Avant de dire « tous les déclencheurs exécutables sont couverts », il reste
à tracer au moins une partie des branches exclusives, les crises et les bilans,
puis à vérifier les appels indirects/arguments dynamiques avec un décodeur
complet ou une instrumentation de la VM. Ce document garde cette limite visible.

## Sources de contexte

**Sources principales** : les scripts/constants des LIN locaux, le XBE et les
textes du `dynamicxbox.umd` fourni. Les sections et clés indiquées dans ce
document permettent de retrouver la preuve dans l'extraction privée. Les règles
de la table pointent vers leur section et leurs offsets dans le JSON.

Lecture externe de contrôle : le [guide Xbox de VampireHorde](https://gamefaqs.gamespot.com/xbox/926929-tom-clancys-splinter-cell-double-agent/faqs/45455)
décrit les crises de confiance et la différence entre objectifs et bilan des
actions. Source secondaire, utilisée comme contrôle de contexte, jamais pour
inventer des montants ou importer des choix de la version Xbox 360.

Pour la méthode d'analyse des ressources : la [discussion de recherche sur les LIN](https://github.com/EliotVU/Unreal-Library/discussions/134)
explique pourquoi leurs offsets de packages ne doivent pas être traités comme
ceux d'un paquet Unreal ordinaire. Les décalages cités ici sont donc ceux du
flux décompressé réellement lu, pas des `SerialOffset` supposés valides.

Requêtes de recherche utilisées : « Splinter Cell Double Agent Xbox original
trust objectives walkthrough NSA JBA GameFAQs » et « Splinter Cell Double Agent
Xbox lin umd Unreal script format ». Aucune capture de jeu réalisée pendant
cette recherche; aucune validation dynamique revendiquée.
