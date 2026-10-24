# Référence — confiance NSA / JBA : décisions et règles actives

Version **0.4 — équilibrage entier et décisions consolidées**, 25 octobre 2026.

Jeu ciblé : **Splinter Cell: Double Agent, version B Xbox originale**, via
SCDA-XBERecomp. Le principe des deux confiances indépendantes vient de la
**version A (Xbox 360, PC et PS3)** ; les effets sont adaptés aux objectifs et
au contexte de la campagne B.

Ce document est la référence principale des décisions retenues : montants actifs,
conditions narratives, effets différés, statistiques, récupération, interface et
sauvegardes. Il conserve aussi les preuves de l'inventaire du système original.
Les sections techniques [TRUST_IMPLEMENTATION.md](TRUST_IMPLEMENTATION.md) et
[TRUST_BALANCE.md](TRUST_BALANCE.md) apportent les vérifications et la comparaison
avant/après, sans être nécessaires pour lire les règles actives ci-dessous.

**Périmètre de version :** les valeurs actives décrites ici sont celles du code
actuel, du build local et de **v0.1.0-alpha.2**. Le ZIP **v0.1.0-alpha.1** conserve
l'équilibrage précédent. Il n'a pas été remplacé par cette passe de réglage.
La campagne complète, toutes les crises et toutes les fins restent à valider.

## Lecture et niveau de preuve

- **Établi par les fichiers** : identifiants, montants constants, drapeaux,
  appels natifs, textes d'objectifs et dialogues associés.
- **Interprétation narrative** : ce que les actions signifient pour chaque camp,
  en tenant compte de ce qu'il sait réellement.
- **Règle active** : décision de conception appliquée par le mod ; les colonnes
  NSA/JBA donnent les valeurs entières du réglage courant, pas celles des développeurs.
- **À valider en jeu** : atteignabilité de toutes les branches, notifications,
  rejouabilité des événements, séquences exactes de découverte et équilibrage.

L'inventaire couvre les appels constants reconnus dans toutes les cartes solo
installées, avec contrôle supplémentaire des cartes coop et du code commun.
Il ne constitue pas une preuve formelle d'absence d'appels indirects ou à arguments
dynamiques. Les tests natifs et captures réalisés sont distingués du parcours complet de
campagne, qui reste inachevé.

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
Le catalogue [TRUST_RULES_REFERENCE.json](TRUST_RULES_REFERENCE.json) conserve
les origines, transitions constantes et valeurs narratives de base. Son champ
`balance_policy` décrit leur transformation actuelle. Les champs `proposal` et
la table C++ générée restent volontairement à la base : le runtime applique
`Rebalance` une seule fois. Les 45 lignes du présent document affichent, elles,
les résultats actifs après cette transformation.

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
Le wrapper et des objectifs ont été exercés par des appels natifs. La totalité
des crises, délais, récompenses d'équipement et lecteurs directs reste à vérifier.

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

| Adresse de l'appel | Origine | Traitement actif du mod |
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
appel. Le mod protège donc les attributions répétées avec un registre persistant des
crédits, en complément des protections éventuelles des scripts.

## Règles de conception et équilibrage actifs

### Valeurs indépendantes et valeurs initiales

- Deux scores **entiers**, chacun borné à **0–100** ; pas de décimales ni de
  complément automatique entre les deux camps.
- Le nouvel état du mod commence à **NSA 75 / JBA 75**. Ce point de départ n'a
  pas changé lors du rééquilibrage.
- Une opération clandestine réussie pour la NSA ne coûte pas automatiquement de
  confiance JBA : il faut une découverte, un échec visible ou un refus connu.
- Aider la JBA pour maintenir une infiltration autorisée ne coûte pas
  automatiquement de confiance NSA. Meurtres d'innocents, sacrifice d'alliés et
  attentats gardent leurs conséquences.
- Un bénéfice partagé est possible si l'histoire le justifie. Deux jauges à 100
  restent possibles ; le but du réglage est de rendre leur accumulation moins facile.
- Les objectifs originellement neutres restent à **0/0**.

### Gains divisés par deux et pertes renforcées

Les valeurs narratives de base reprennent les ordres de grandeur 5/10/15/30.
Le réglage actif du 25 octobre leur applique ensuite cette politique :

| Situation de base | Décision active | Exemple NSA/JBA |
| --- | --- | --- |
| Aucune des deux variations n'est négative | Diviser chaque gain par deux, avec arrondi entier inférieur | +5/0 → +2/0 ; +15/0 → +7/0 |
| Les deux camps gagnent sans perte | Diviser les deux gains par deux | +5/+5 → +2/+2 |
| Au moins un camp perd de la confiance | Doubler chaque perte, sauf −30 qui devient −40 ; conserver le gain positif de l'autre camp | −5/+5 → −10/+5 |
| Perte sans récompense opposée | Doubler la perte | 0/−15 → 0/−30 |
| Annulation d'un crédit déjà attribué | Retirer exactement sa variation effective stockée ; ne pas réappliquer la politique | Un ancien gain effectif de +5 s'annule par −5 |

La classification porte sur **la paire narrative complète avant tout report**
d'une perte à la découverte. Un sabotage +30/−30 devient +30/−40, même si la
partie −40 n'est appliquée qu'à la sortie de mission. Son gain immédiat n'est
donc pas réduit à +15 au prétexte que sa perte est temporairement différée.

Ajustement inclus dans alpha.2 (publication mise à jour) : les pertes narratives de base −30 deviennent **−40**
(au lieu de −60). Les autres pertes restent doublées ; le plafond statistique
reste −60. Depuis 75, une telle décision laisse 35 ; une confiance déjà faible
peut toujours déclencher une crise. Les deux désamorçages à −30 chacun restent
cumulables : cette exception porte sur chaque valeur de base, pas sur leur somme.

Exceptions conservées : **état initial 75/75** et **récupérations d'urgence +25**.
Ces dernières doivent encore permettre de sortir d'une crise. Les statistiques
répétables suivent leur propre accumulation entière, détaillée plus bas.

### Attribution unique, choix opposés et saturation

Une attribution par identifiant d'objectif et famille de mission ; une attribution
par bombe distincte. Les variantes `_a`/`_b` partagent la famille, et les deux
visites du QG sont distinguées malgré leurs déclarations de scripts dupliquées.
Le registre conserve le delta demandé et celui réellement appliqué après bornage.

Les paires exclusives sont Barnham vivant/mort, chef de gare vivant/mort,
Yeager transmis à Lambert/dénoncé à Emile, couverture de Lambert
confirmée/révélée, téléphone saboté/préservé et Hisham sauvé/tué. Le nouvel
événement annule d'abord le crédit effectif de son opposé, puis applique sa règle.
Les bombes du second QG et la découverte d'un sabotage possèdent aussi leurs
annulations spécifiques, décrites dans les cas particuliers.

Un gain saturé ne peut pas créer de points lors de son annulation. Si +30 n'a
réellement ajouté que +5 à un score de 95, son annulation retire **5**, pas 30.
L'ancienne compensation scalaire du train est ignorée : le registre effectue
déjà cette annulation. Les doublons ne donnent ni nouveau crédit ni nouveau pop-up.

## Catalogue narratif des 45 objectifs à confiance non nulle

Dans les tables : **ancien** désigne le delta de la jauge unique, avec +NSA et
−JBA. **NSA/JBA actifs** sont les variations indépendantes après le réglage
entier du 25 octobre. La colonne « Ancien » n'est jamais rééquilibrée : elle
reste une preuve du jeu original. Les conditions et reports de découverte de
la dernière colonne font partie de chaque règle.

### Prison d’Ellsworth

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `BarnhamAlive` | Épargner Barnham | +15 | +15 | -30 | Témoin du DOJ protégé; Jamie constate personnellement le refus de le tuer. |
| `Retrieve` | Récupérer le disque sur Jamie | +5 | +2 | +0 | Renseignement clandestin pour Third Echelon; aucune découverte JBA établie. |
| `NeedSeringe` | Trouver la seringue pour Jamie | -10 | +0 | +5 | Préserver le contact qui permet l'infiltration; la NSA a autorisé cette opération. |
| `BarnHamDead` | Aider Jamie à tuer Barnham | -15 | -30 | +15 | Tuer le témoin contredit l'ordre de Lambert et rassure directement Jamie. |
| `Regroup` | Retrouver Jamie | -10 | +0 | +5 | Maintenir Jamie en vie et poursuivre l'infiltration autorisée. |
| `Disc` | Falsifier les registres de décès | -5 | +0 | +2 | Faciliter l'évasion et renforcer la couverture; pas de trahison NSA supplémentaire. |

### QG JBA — première visite

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQA_P_10` | Réduire le brouillage de la JBA | +10 | +5 | +0 | Ouvrir une fenêtre de surveillance NSA en secret; perte JBA seulement si une autre scène établit la découverte. |
| `CarsonMoss` | Renforcer la fausse identité de Sam | -5 | +0 | +2 | Falsifier le contrôle de sécurité de Moss sert aussi l'infiltration autorisée par Lambert. |
| `YeagerLambert` | Transmettre Yeager à Lambert | +15 | +7 | +0 | Lambert prévoit de le faire disparaître avant Emile; ne pas attribuer automatiquement cette exfiltration à Sam côté JBA. |
| `YeagerEmile` | Dénoncer Yeager à Emile | -15 | -30 | +15 | Écarter la piste que Lambert voulait exploiter et fournir à Emile la preuve qu'il attend. |
| `OBJ_HQA_P_20` | Ouvrir le serveur JBA à la NSA | +10 | +5 | +0 | Renseignement clandestin; la pénalité JBA relève d'une découverte avérée, pas de la seule réussite technique. |
| `OBJ_HQA_O_20` | Relever les empreintes d'Enrica | +5 | +2 | +0 | Collecte de renseignement clandestine; les autres empreintes à montant original nul restent neutres. |

### Train blindé

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `P_WoltCode` | Obtenir le code du coffre | -10 | +0 | +5 | Étape du braquage confié par Emile, nécessaire à la couverture. |
| `P_KillChief` | Tuer le chef de gare | -15 | -30 | +15 | Preuve de loyauté exigée par Emile; meurtre d'un innocent expressément interdit par Lambert. |
| `P_ChiefAlive` | Laisser le chef de gare vivant | +15 | +15 | -30 | Respecter l'ordre NSA tout en refusant une preuve de loyauté explicite. |
| `O_DetachCars` | Décrocher les wagons arrière | +5 | +2 | +0 | Éloigner des employés du danger sans empêcher le braquage principal. |
| `P_DisconnectCom` | Couper les communications du train | -10 | +0 | +5 | Retarder l'alerte et permettre le braquage sous couverture. |

### Cozumel

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `ChipCellPhone` | Saboter le déclencheur d'Enrica | +30 | +30 | -40 | NSA +30 au sabotage secret ; JBA −40 à la sortie de mission dans l’implémentation actuelle. Perte différée, pas gain sans contrepartie. |
| `ProCellPhone` | Préserver le déclencheur de la bombe | -30 | -40 | +30 | Permettre volontairement l'attentat malgré l'ordre de Lambert; résolution opposée au sabotage, jamais cumulée avec lui. |
| `PlaceRelay` | Installer le relais avec Enrica | -10 | +0 | +5 | Maintenir la couverture avant le choix sur le téléphone; ne pas punir une seconde fois le résultat final. |
| `PassengerLog` | Récupérer le manifeste des passagers | +5 | +2 | +2 | La description sert l'identification des victimes et Enrica rapporte une demande explicite d'Emile: intérêt des deux camps. |
| `GetBomb` | Récupérer la bombe | -10 | +0 | +5 | Continuer la mission de couverture avec Enrica; le sort du navire possède son propre événement majeur. |
| `CameraRecord` | Effacer les enregistrements vidéo | +5 | +2 | +2 | Le texte exige de préserver le secret de la présence de Sam; préserver sa couverture protège les intérêts opérationnels des deux camps. |
| `ShipEngine` | Immobiliser le navire | +5 | +2 | +0 | Réduire les victimes à terre sans rendre ce sabotage automatiquement connu de la JBA. |

### Mer d’Okhotsk

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `Mercenaries` | Neutraliser les mercenaires du camp | -10 | +0 | +5 | Permettre aux alliés d'Emile d'aborder; l'objectif accepte létal ou non létal. Les pertes humaines sont évaluées séparément. |
| `Communications` | Couper le réseau des mercenaires | -10 | +0 | +5 | Empêcher l'alerte pendant la prise du pétrolier. |
| `Bombs` | Désarmer les charges dans la glace | -5 | +0 | +2 | Maintenir le pétrolier bloqué jusqu'à sa capture; ces charges ne sont pas les bombes d'attentat de la JBA. |
| `KeyCode` | Récupérer la clé de chiffrement | +5 | +2 | +0 | Surveiller le réseau des contrebandiers pour la NSA. |
| `Freight_Done` | Identifier la cargaison | +5 | +2 | +0 | Renseigner la NSA sur ce que la JBA pourrait exploiter; conserver une seule attribution malgré deux branches de complétion. |
| `Tracker_Done` | Poser le traceur du pétrolier | +5 | +2 | +0 | Suivre clandestinement le navire après sa capture par la JBA. |
| `Bombs_Done` | Empêcher le sabordage du pétrolier | -10 | +0 | +5 | Sauver le navire destiné à la JBA; ne pas confondre ce désamorçage avec un sabotage de ses attentats. |

### Kinshasa

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_10_P_50` | Vérifier les partenaires d'Emile | -10 | +0 | +5 | Confirmer le calendrier et la loyauté des deux autres groupes. |
| `OBJ_10_O_10` | Renseigner la NSA sur Takfir | +5 | +2 | +0 | Collecte clandestine sur un groupe terroriste. |
| `OBJ_10_O_20` | Renseigner la NSA sur Massoud | +5 | +2 | +0 | Même intention, cible et objectif distincts. |
| `OBJ_10_P_60` | Tuer Hisham | -15 | -30 | +15 | Exécuter un agent allié à la demande d'Emile; conserver la gravité du choix original même si Lambert privilégie la mission. |
| `HishamBody` | Faire exploser le véhicule censé contenir Hisham | -5 | +0 | +2 | Valider la preuve fournie à Emile. Un autre corps peut servir au simulacre: aucune perte NSA pour une fausse mort réussie; le meurtre réel est déjà pénalisé par son objectif. |
| `OBJ_10_P_30` | Sauver Hisham | +15 | +7 | +0 | Sauvetage discret : NSA +7 / JBA 0. Si Emile voit la fuite : perte JBA −30 distincte, sans second crédit NSA ; retirer tout bonus de preuve déjà attribué. |
| `OBJ_10_P_70` | Sécuriser l'extraction et le canon AA | -10 | +0 | +5 | Permettre le départ du groupe sous couverture; ne pas sanctionner des neutralisations obligatoires comme des victimes civiles. |

### QG JBA — seconde visite

| ID | Événement | Ancien | NSA actif | JBA actif | Intention et conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQB_P_10` | Voler les contacts d'Emile | +10 | +5 | +0 | Obtenir les contacts pour Third Echelon sans informer la JBA. |
| `OBJ_HQB_P_20` | Analyser le mercure rouge | +10 | +5 | +0 | Vérifier l'authenticité de l'arme pour Williams; action clandestine. |
| `P_LambertNOTCover` | Révéler l'identité de Lambert | -15 | -30 | +15 | Conserver l'orientation morale du jeu original et le sacrifice d'un agent allié; Williams encourage pourtant ce choix: tension narrative à conserver explicitement. |
| `P_LambertCover` | Confirmer la couverture de Lambert | +15 | +15 | -30 | Sauver Lambert entretient les soupçons d'Emile selon le texte de l'objectif. |
| `OBJ_HQB_P_30` | Désamorcer la bombe de Nashville | +15 | +15 | -30 | NSA +15 immédiatement ; JBA −30 à la sortie de mission. Crédit et pénalité distincts pour chaque bombe ; annulation si KeepBombs remplace ce résultat. |
| `OBJ_HQB_P_31` | Désamorcer la bombe de Los Angeles | +15 | +15 | -30 | NSA +15 immédiatement ; JBA −30 à la sortie de mission. Crédit et pénalité distincts pour chaque bombe ; annulation si KeepBombs remplace ce résultat. |
| `KeepBombs` | Empêcher la neutralisation des bombes | -30 | -40 | +30 | Faire partir les bombes par une autre route compromet volontairement leur interception par la NSA. |


## Cas particuliers et conditions narratives retenues

### Chef de gare : annulation d'un résultat précédent

Un appel direct `ModifyTrust(15, true)` existe dans `x04_moneytrain_a`, offset
décompressé **`0x3579cc`**, dans `Primary_Obj_Kill_Done_After_Alive`. Son delta
original −15 retire le crédit scalaire du chef épargné. Ce montant historique
ne devient pas une deuxième pénalité dans le mod.

Règle actuelle, sans saturation : `épargné (+15,−30)` → annulation
`(−15,+30)` → `tué (−30,+15)`. Le résultat attribué au choix est donc
`(−30,+15)`, sans ancien bonus résiduel. Aux bornes 0/100, l'annulation utilise
les variations **effectivement** créditées, y compris celles d'une ancienne
sauvegarde. Elle n'est ni doublée ni divisée par deux.

### Hisham : mort réelle, mort simulée et mensonge découvert

`HishamBody` décrit un corps, mais les clés `Speech_0016L` à `Speech_0020L`
de `p_x10_kinshasa_b_hisham_script` introduisent l'utilisation d'un autre corps
de même gabarit. Les rappels `p_x10_kinshasa_b_hisham_barks` demandent de
préparer la voiture. L'ID seul ne prouve donc pas qu'Hisham a été tué.

| Branche / événement | NSA actif | JBA actif | Traitement |
| --- | ---: | ---: | --- |
| Sauver Hisham, `OBJ_10_P_30` | +7 | 0 | Une seule attribution ; la fausse mort peut préserver la couverture |
| Preuve, `HishamBody` | 0 | +2 | Une seule attribution, seulement si aucune fuite constatée n'est enregistrée |
| Emile constate la fuite, `Speech_0050L` | 0 | −30 | Annuler le bonus de preuve effectif, puis appliquer cette perte une seule fois |
| Tuer Hisham, `OBJ_10_P_60` | −30 | +15 | Remplacer le sauvetage et annuler une précédente pénalité de fuite ; preuve +2 possible séparément |
| Emile accepte le récit, `Speech_0024L` | 0 | 0 | Enregistrer l'acceptation ; ne pas effacer une fuite déjà constatée |

Le hook observe l'appel `Localize` **sélectionné par le script** dans
`p_x10_kinshasa_b_comms`, avant l'affichage éventuel des sous-titres.
Les clés `Speech_0036L` à `Speech_0040L` n'établissent pas un aveu suffisamment
clair : aucune pénalité supplémentaire n'est déduite de leur ancienne interprétation.

La branche de fausse mort réussie complète `OBJ_10_P_30`, puis annule
`OBJ_10_P_60` et `HishamBody` (`x10_kinshasa_b`, offsets décodés
`0x2e0e70`–`0x2e0eb7` et suite). **Un objectif de preuve absent ou annulé
ne suffit jamais à établir une découverte.** L'ancien repli « preuve manquante
à la sortie de mission » a été retiré. Une acceptation ou fuite explicite peut
aussi retirer cet ancien crédit erroné d'une sauvegarde antérieure.

Test natif actuel : **75/75 → 82/75 → 82/77 → 82/45**. Le dernier passage
retire le bonus effectif +2 puis applique −30 ; ce n'est pas une pénalité −32
supplémentaire à répéter. Les répétitions, une preuve tardive et une acceptation
après découverte ne recréditent rien. La séquence narrative complète reste à jouer.

### Cozumel : bénéfice partagé et conséquences différées

Le manifeste identifie les victimes éventuelles ; Enrica mentionne aussi la
demande d'Emile dans `p_x06_cozumel_a_enrika_coms:Speech_0082L`.
`PassengerLog` bénéficie donc aux deux camps : **+2/+2**. `CameraRecord`
protège la présence secrète et la couverture de Sam : **+2/+2** également.
`ShipEngine` donne **+2/0**. `PlaceRelay` et `GetBomb` donnent chacun
**0/+5**, sans sanction NSA supplémentaire pour poursuivre l'infiltration.

Le téléphone saboté donne **NSA +30 immédiatement**. La perte **JBA −40**
est appliquée **à la sortie de mission** dans le code actuel, une fois par
crédit de sabotage. Ce report est un choix d'implémentation : l'instant exact
de découverte scénarisée reste à raccorder/valider. Il ne faut ni prétendre que
Sam est découvert au geste clandestin ni attribuer à ce geste le demi-gain des
tâches sans contrepartie : sa paire complète est bien +30/−40.

`ProCellPhone` donne **−40/+30** et remplace le sabotage. Le mod retire son
crédit NSA effectif et toute pénalité `bomb-discovered` déjà enregistrée avant
d'appliquer l'issue opposée. Les morts génériques restent évaluées séparément
selon les catégories statistiques activées.

### Second QG : bombes distinctes et choix final

Nashville et Los Angeles ont chacune leur crédit : **NSA +15 immédiatement**,
puis **JBA −30 à la sortie de mission**. Leur découverte n'est pas instantanée.
Les deux bombes peuvent donc produire au total +30/−60 avant saturation.
`KeepBombs` donne **−40/+30** et retire les crédits de désamorçage ainsi que
leurs pénalités de découverte déjà appliquées. Un désamorçage ultérieur retire
le crédit `KeepBombs` ; chaque nouvelle attribution reste protégée par le registre.

### Lambert et Yeager : intention narrative et décisions exclusives

Dans `p_x11_hq5_r_communications_main:Speech_0015L` à `Speech_0022L`,
Williams encourage le sacrifice de Lambert pour assurer la couverture. Le mod
conserve néanmoins l'orientation morale originale : confirmer la couverture
vaut **+15/−30**, révéler son identité **−30/+15**. Ce choix ne signifie pas
que tous les ordres de la NSA exigent de sauver Lambert.

Pour Yeager, l'exfiltration par Lambert reste clandestine : **+7/0**.
Le dénoncer à Emile vaut **−30/+15**. Ces issues se remplacent, sans cumul.

## Déclencheurs de récupération : 19 règles, pas 19 choix de l'histoire

Les variantes de `P_TrustNSA`, `P_TrustJBA`, `P_TrustNSAHq2` et `TrustNow`
correspondent aux crises de couverture ou de loyauté.

| Famille | Original | Effet actif | Contexte |
| --- | ---: | --- | --- |
| `P_TrustNSA`, `P_TrustNSAHq2` | +25 | NSA +25, JBA 0 | Contacter Williams / prouver que l'opération demeure sous contrôle |
| `P_TrustJBA` | −25 | NSA 0, JBA +25 | Contacter Emile pour rassurer la JBA |
| `TrustNow` | −25 | NSA 0, JBA +25 | Revenir en zone publique et se présenter à Emile au QG |

Les cartes du train, Cozumel, Okhotsk et Kinshasa possèdent leurs variantes;
le QG a ses branches propres. Le JSON et l'annexe gardent chaque origine.
Les appels de complétion correspondants ont été retrouvés.

Les récupérations restent **+25**, exception explicite à la réduction des petits
gains. Le runtime ne les accorde que si le camp concerné est à **20 ou moins**.
Un garde par camp bloque leur répétition ; il est réarmé lorsque ce score repasse
de plus de 20 à 20 ou moins. Une nouvelle visite à un ordinateur ne donne donc
pas arbitrairement +25.

L'adaptateur lance les événements de crise à partir d'une confiance **basse**,
jamais d'une confiance élevée de l'autre camp. Il projette le camp en difficulté
dans le système historique, et mémorise le niveau déjà transmis pour éviter de
relancer l'alerte à chaque événement. Le seuil de projection est inférieur à 50 ;
les niveaux, branches et délais effectifs restent ceux des scripts d'origine.
Aucun nouveau délai de mort n'a été ajouté. Tous les scénarios de crise et leurs
récompenses d'équipement restent à vérifier en campagne, car certains scripts
lisent directement l'ancien scalaire (miroir NSA hors dispatch).

## Déclencheurs techniques et objectifs neutres

### Dix déclarations de test

`TEMPNSA` vaut +20 et `TEMPJBA` −70, dans les deux cartes Cozumel,
Okhotsk B et les deux cartes Kinshasa. Les déclarations et leurs complétions
existent; les noms de branches `TempDebugNSA` / `TempDebugJBA` sont présents
dans les noms de scripts. Les entrées de localisation associées sont nulles.

Décision active : **aucun effet** pour les objectifs dont l'ID commence par
`TEMP`, avec diagnostic si l'un se déclenche. Ce ne sont pas dix choix narratifs.
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
pas des actions narratives. Le mod n'en tire pas de récompense et restaure les
deux scores depuis les métadonnées décrites plus bas. Les anciens deltas de
menu ou appels non classés ne servent pas à inventer un effet narratif.

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

L'inventaire statique n'établit pas les valeurs effectives de tous les coefficients
par mission. Les nombres actifs ci-dessous sont choisis par le mod et ne doivent
pas être présentés comme leurs valeurs originales. Les deux accumulateurs originaux sont plafonnés
à 15 et le résultat de ce chemin est borné à 5..95 avant `ModifyTrust`.

### Règles actives pour les actions répétables

Application au bilan, **NSA seulement**, avec les catégories activées par les
drapeaux de mission. Ces montants sont des décisions du mod ; ce ne sont pas
les coefficients originaux, qui restent décrits séparément ci-dessus.

| Action enregistrée | NSA actif | JBA actif | Condition / limite |
| --- | ---: | ---: | --- |
| Ennemi tué | −4 | 0 | Règle de retenue active ; décès nommés déjà traités par un choix retirés du compte |
| Civil tué | −10 | 0 | Aucun gain JBA automatique ; classification civile fournie par les statistiques |
| Ennemi assommé | +1 pour deux | 0 | Accumuler les KO puis diviser par deux, avec arrondi inférieur ; un seul KO vaut 0 |
| Civil assommé | −2 | 0 | Règle de neutralisation active |
| Repérage | −2 | 0 | Compteur d'incidents du jeu |
| Alarme | −4 | 0 | Règle d'alarme active |
| Corps découvert | −2 | 0 | Règle de discrétion active |

Calcul exact : `gain = floor(min(KO_ennemis, 15) / 2)` si la règle KO est
active ; `perte_base` additionne les anciens coûts activés (2 par ennemi tué,
5 par civil tué, 1 par civil assommé, 1 par repérage, 2 par alarme et 1 par corps).
Le delta NSA est **`gain − 2 × min(perte_base, 30)`**. Les deux accumulateurs
sont plafonnés indépendamment : **gain maximal +7, perte maximale −60**.
Le score final est borné à 0–100 ; le clamp historique 5–95 ne limite plus la
prévisualisation. Le JBA reçoit 0 de ce calcul générique.
Le plafond de perte a été relevé de −30 à **−60** pour que les actions répétées
imprudentes restent coûteuses : six civils ou quinze ennemis tués atteignent
ce plafond si la règle de décès est active et sans bonus de KO. Les repérages,
alarmes et corps découverts partagent ce même plafond ; ce n'est pas un plafond
supplémentaire par catégorie. Les coûts par victime ne changent pas.

Le tableau du bilan affiche les contributions brutes rééquilibrées par catégorie,
et les deux jauges montrent le résultat total effectif après plafonds et saturation.
La prévisualisation utilise une copie de l'état ; elle ne crédite rien. Une fois
le bilan appliqué, elle réutilise son crédit effectif au lieu de le prévoir une
deuxième fois. L'application native est protégée contre les répétitions par carte.

Les morts nommées de Barnham, du chef de gare et d'Hisham sont soustraites des
statistiques de décès si leur choix a déjà été crédité, pour éviter une deuxième
sanction générique. Le code retire d'abord un décès civil, sinon un ennemi ; la
classification d'une cible particulière reste donc une limite à vérifier.
À New York, les décès sont exemptés pour le combat imposé ; les autres règles
activées continuent à s'appliquer.

**Limite policiers :** un policier protégé enregistré comme ennemi ne peut pas
encore être distingué d'un autre ennemi par ces seuls enregistrements. Il ne faut
donc pas promettre −10 pour tout policier : seul le type civil reçoit ce coût.
Les actes attendus par Emile donnent déjà leur confiance JBA via leurs objectifs.
Aucune règle générique « tout meurtre fait gagner la JBA » n'a été ajoutée.

## Interface retenue

- **Style B, rangées par faction** : cadre métallique olive, badge circulaire
  NSA/JBA, vingt segments ivoire, libellés inclinés et score entier. Chaque segment
  représente 5 points ; un segment partiel représente les autres valeurs entières.
  Les deux pistes ont la même longueur. Une confiance à 20 ou moins passe au rouge.
- **OPSAT** : deux rangées permanentes dans la page de confiance ; conservation du
  cadre natif et légende localisée expliquant l'indépendance. Les deux niveaux
  textuels natifs sont lus pour leur propre camp, sans inversion artificielle JBA.
- **En jeu** : aucun affichage permanent de diagnostic. Pop-up de cinq secondes,
  seulement pour le ou les camps affectés ; pas de ligne +0 pour le camp inchangé.
- Les annulations et attributions rapprochées de 250 ms sont regroupées en un
  changement net. Un événement nul ou déjà crédité ne crée pas de notification.
- Le delta signé est **dans la piste, juste après le bord de la nouvelle valeur**,
  et non au milieu de la barre, au milieu des points perdus ou après l'ancien maximum.
  Il tient compte des segments partiels et des espaces ; près de 100, sa position
  est bornée pour rester lisible dans la piste. Aucun fond opaque ne masque la perte.
- Pour une perte, le remplissage ivoire est fixé à la nouvelle valeur et la queue
  rouge part de ce même bord ; elle se rétracte depuis l'ancienne valeur. Pour un
  gain, le remplissage progresse vers la nouvelle valeur avec une lueur verte.
  Transition de 1,25 s après 0,15 s d'attente, léger mouvement vertical du delta,
  puis fondu pendant les 0,65 dernières secondes. Les scores sont appliqués
  immédiatement : seule leur présentation est animée.
- **Bilan** : résultats séparés sous le tableau natif ; le delta est dans chaque
  piste. La prévision est stable et indépendante de l'horloge des pop-up en jeu,
  même si les deux deltas sont identiques. La révélation native du tableau reste
  active ; les captures de référence attendent sa fin.
- Console, paramètres et option de masquage du HUD cachent la présentation.
  Le rendu est assuré par l'hôte ; aucune texture ni ressource LIN du jeu n'est modifiée.

## Sauvegardes et compatibilité des décisions

Le registre stocke les scores, la mission/carte, les crédits demandés/effectifs et
l'état de l'adaptateur d'alertes. Les octets du checkpoint Xbox restent à leur
format original. Les informations additionnelles sont enregistrées atomiquement
dans des fichiers versionnés à côté de l'exécutable :

```text
trust-state/<nom du checkpoint>/<SHA1 du chemin relatif>/<SHA1 des octets du checkpoint>.txt
```

Ce dossier est distinct du cache Xbox ; il doit accompagner les sauvegardes
lors d'une copie, migration ou restauration. Une sauvegarde sans ces métadonnées
importe l'ancien scalaire en **NSA=x / JBA=100−x** : son passé à deux confiances
ne peut pas être reconstruit. Un état neuf reste à 75/75.

Les changements de processus du port utilisent un fichier de passage par instance,
lu uniquement lors d'un relancement signalé. Un démarrage à froid ignore un ancien
fichier de passage. L'initialisation en attente lors d'un chargement depuis le menu
est conservée jusqu'au premier contrôleur jouable.

**Pas de rééquilibrage rétroactif** : charger un ancien état 100/100 le laisse à
100/100. Les futurs événements utilisent les nouvelles règles. Les crédits d'une
ancienne version gardent leur variation effective et sont annulés à cette valeur,
sans être recalculés. Rejouer un objectif déjà crédité ne donne pas sa récompense
réduite une deuxième fois. Une nouvelle campagne est le meilleur comparatif global.

La refonte vise le solo ; coop et versus gardent leur comportement d'origine.
Leur régression complète n'est pas validée. L'historique natif du profil reste
scalaire ; les scores actuels et leur registre indépendant sont dans les
métadonnées du mod. Les graphiques historiques n'ont pas été convertis en deux
historiques. Pour revenir au port d'origine, utiliser une sauvegarde pré-mod et
un build non modifié ; l'ancien jeu ne sait pas lire l'historique JBA indépendant.

## Vérifications et limites actuelles

| Vérification | Résultat / portée |
| --- | --- |
| Tests C++ du modèle | Exception −30 → −40, annulation après saturation, arrondis entiers, gains partagés, arbitrages, autres pertes doublées, saturation, annulations effectives et doublons validés |
| Manifeste puis moteur, appels natifs | 75/75 → 77/77 → 79/77 ; répétition sans gain |
| Choix du téléphone saturé, appels natifs | Fixture alpha.2 : 95/50 → 100/50 → 35/80 avec −60/+30. Politique actuelle : résultat attendu 55/80 avec −40/+30 ; non rejoué en jeu |
| Statistiques : civil tué, KO ennemi, repérage, alarme | Prévision 80/80 → 64/80 ; application 64/80 ; répétition inchangée |
| Sauvetage/preuve/fuite Hisham, objectifs et dialogue synthétiques | 75/75 → 82/75 → 82/77 → 82/45 |
| Récupération NSA native | 10/80 → 35/80 ; autre camp inchangé |
| Persistance / recharge / doublons | Validée sur la version précédente ; format et annulation des anciens crédits conservés, sans prétendre à un parcours complet du nouvel équilibrage |
| Interface | OPSAT, pop-up et bilan capturés en jeu ; table de bilan capturée après révélation |

Ces tests utilisent une instance isolée et des fixtures contrôlées, pas une
certification de toutes les branches narratives. Restent à valider :

- campagne complète avec le nouvel équilibrage et atteignabilité de toutes les branches ;
- crises, délais, équipements et lecteurs directs du scalaire historique ;
- moment précis de découverte des sabotages, actuellement reporté à la sortie ;
- séquence naturelle complète d'Hisham et toutes les fins ;
- distinction des policiers protégés dans les statistiques génériques ;
- chargements inter-cartes usuels et régression multijoueur complète.

Un `LOADGAME` brut lancé depuis le titre a planté dans la désérialisation invitée
lors d'un test antérieur ; utiliser les menus normaux pour les sauvegardes
inter-cartes. Aucun nouveau comportement de téléchargement ou ressource du jeu
n'est requis pour appliquer ces règles.

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
Xbox lin umd Unreal script format ». Ces requêtes concernent l'inventaire initial
du 23 octobre. Les tests natifs
et captures ajoutés ensuite sont recensés ci-dessus ; ils ne constituent pas
un parcours complet de campagne.
