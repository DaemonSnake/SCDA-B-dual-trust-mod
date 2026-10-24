# Journal de reverse engineering — confiance NSA / JBA

## 2026-10-23 — recherche statique locale

Demande : identifier le système de confiance Xbox pour préparer deux jauges
indépendantes et des effets classés par action. « GBA » dans la demande est
interprété comme JBA, nom présent dans les ressources du jeu.

Analyse du XBE fourni et des instructions dans `generated/game`, sans modification
du jeu, des ressources ou des sauvegardes. Aucun test dynamique effectué.

XBE SHA256 : `C742D61C9B8D8C1C4F886A4A4A69FFB65AB3961E0283DCAD33BFCFA963885143`.

Résultats et points d'entrée : [knowledge/TRUST_NSA_JBA.md](knowledge/TRUST_NSA_JBA.md).
Les extractions intermédiaires restent hors du projet dans
`C:/Users/bastien/scda-trust-analysis/`.

Faits établis : valeur de confiance lue comme un octet à `+0x360` sur l'objet
retourné par `0x0017B720`; logos d'alerte NSA à 0 et JBA à 100;
wrapper script `ModifyTrust` à `0x00092413`; traitement des objectifs à
`0x000E9CE4`; agrégation des actions au bilan à `0x001BB69C`, avec calcul
de prévisualisation distinct à `0x001BA097`.

Cette première passe ne constituait pas un inventaire des scripts; voir la suite.

## 2026-10-23 — inventaire et référence narrative

Référence : [knowledge/TRUST_REFERENCE.md](knowledge/TRUST_REFERENCE.md).
Annexe de 139 déclarations solo et 208 changements d'état :
[knowledge/TRUST_TRIGGER_INVENTORY.md](knowledge/TRUST_TRIGGER_INVENTORY.md).
Catalogue structuré : `knowledge/TRUST_RULES_REFERENCE.json`.
Outil reproductible : `tools/analysis/inventory_trust.py`.

33 LIN examinés : commun, menu, 17 solo, 14 coop. 183 déclarations constantes
reconnues sans erreur de lecture. Les 44 coop sont à montant nul. Les 139 solo
se regroupent en 111 règles : 45 narratives non nulles, 19 récupérations,
10 entrées techniques TEMP et 37 neutres. Les 74 règles solo non nulles ont
chacune une complétion constante associée dans leur famille de mission.

Correction importante : **+delta = NSA, -delta = JBA**. Les logos aux extrémités
signalaient le camp en difficulté; la première déduction inverse était erronée.
Corps script trouvé dans common décompressé, en-tête `0x17f295`, code
`0x17f2a6`, taille annoncée 102 : addition du delta et clamp 0..100 recoupés
avec les natives 0x92 et 0xFB. Classe/propriété : `EPlayerController.Trust`.

Natives suivies : AddGoal 0xD05 (`0x0009B19F`), SetGoalStatus via EPattern
0x8AE (`0x0009B346`), via conteneur 0x3EB (`0x000EA3F2`), ModifyTrust
via EPattern 0xFC0 (`0x00095B32`). Montant AddGoal : 13e paramètre; drapeau
JBA : 12e. Appel direct constant dans le train à `0x3579cc` décompressé,
branche `Primary_Obj_Kill_Done_After_Alive` : compensation de crédit précédent.

Contexte tiré des objectifs et dialogues locaux. Cas marquants : manifeste de
Cozumel demandé par Emile aussi; simulation de mort d'Hisham avec un autre corps;
dialogue distinct quand Emile voit la fuite; Williams encourage le sacrifice de
Lambert malgré l'orientation de confiance originale. Les valeurs des deux
jauges sont des propositions éditoriales, distinctes des montants extraits.

Les flux décompressés et textes complets restent dans
`C:/Users/bastien/scda-trust-analysis/`, hors du projet. Aucun patch gameplay,
aucune modification de ressources/sauvegardes, aucun lancement du jeu.
L'exhaustivité dynamique n'est pas revendiquée : branchements, expressions
dynamiques et coefficients statistiques par mission restent à vérifier.

## 2026-10-23 — Implementation of independent NSA/JBA trust (in progress)

User authorized implementation and a replacement for the shared visual gauge.
Before edits: full source, reference, executable, DLL and HDD backup at
`C:/Users/bastien/scda-trust-analysis/backups/20261023-232006`.

Native host implementation in `src/game/trust.cpp`, pure state model and generated
45-rule story table. No edits to the game's LIN assets or generated guest code.
SetGoalStatus (`0xE9CE4`) supplies the objective identity to ModifyTrust (`0x92413`).
Positive original trust means NSA. Debug and unknown legacy deltas are logged,
not converted into automatic opposing-camp changes. Original objective bookkeeping
is retained. Credits include effective (clamped) changes for reversals and dedup.

Checkpoint files retain their retail format. Host file-close callbacks attach
versioned, atomic sidecars under `<checkpoint directory>/trust-state/<filename>/`;
the filename is SHA-1 of the checkpoint bytes. Both values, mission, objective
ledger and last dispatched legacy crisis value are saved. Missing sidecar imports
the old scalar as NSA=x, JBA=100-x. Process relaunch gets a separate per-instance
handover file; only `--relaunched` consumes it.

HUD root areas AInst_Trust/A_Trust/A_trust are suppressed and replaced by two
independent ImGui bars. P_Trust and P_TrustReview request the larger display.
The overlay respects hide-HUD, console and settings panel visibility.

Verification so far: C++20 state tests and release SCDA build pass. Live prison
HUD screenshot confirms both bars. JBA-only NeedSeringe: 75/75 -> 75/85, NSA
unchanged. AUTO reload restored deliberately changed 22/91 back to saved75/85.
The final per-filename sidecar path was added after that first reload test.
A debug `open` loads a playable level without allocating its campaign objective
container, so real objective completion needs a native-call harness or a normally
initialized campaign. A raw cross-map LOADGAME from the title/menu crashed in
original guest deserialization; use the real menu's TRAVEL flow for cross-map
loading. That experimental command is not installed in normal game flows.

Opt-in SCDA_TRUST_TEST file bridge exists for test commands and snapshots. It is
disabled unless that environment variable is supplied. Runtime script inspection
confirmed UFunction Script TArray at +0x40 and captured the actual ModifyTrust,
GetEchelonTrustLevel, ConvertTrustValueToTrustLevel and ManageTrustLevelEvents
bytecode outside the repo, in `C:/Users/bastien/scda-trust-analysis/`.

Remaining validation and exact campaign edge cases are tracked in
`knowledge/TRUST_IMPLEMENTATION.md`; this is not a claim of full campaign QA.

### Final verification and user handover

Final solo executable/DLL and versus host builds passed; state assertions passed.
Actual registered Cozumel PlaceRelay completed through native SetGoalStatus:
80/80 -> 80/90, repeat unchanged. Native test records also verified both-camp
PassengerLog gain, NSA-only ShipEngine gain and saturated mutually exclusive
ChipCellPhone/ProCellPhone reversal. Final central sidecar reload restored80/90
from a deliberate22/91 and retained duplicate suppression. Low NSA10/JBA90
recovered through native P_TrustNSA to35/90 without altering JBA.

Moved the HUD panel to the lower left after a screenshot showed the upper-left
position overlapping Lambert's radio dialogue. Final screenshot:
`C:/Users/bastien/scda-trust-analysis/trust-hud-final.png` (80/90).

Sidecar format3 also preserves whether initialization/legacy import is pending;
a title-screen load must carry that flag through process relaunch before the
first playable controller exists. Cold starts discard stale handover metadata.

Restored all26 original .sav files from the pre-modification backup and verified
SHA-256 equality before reopening the frontend. The pre-restoration saves,
sidecars and test results were retained in
`C:/Users/bastien/scda-trust-analysis/test-run-final/`. No test command is queued.
The user requested an explanation for game shutdowns; explained that builds and
native tests required executable replacement, apologized for not warning first,
and left the game open. Do not close or drive this instance again without the
user's agreement. Remaining campaign/UI QA is explicit in TRUST_IMPLEMENTATION.md.

## 2026-10-24 — independent OPSAT/review UI and story evidence

Continued with an isolated trust-lab executable (--instance 2, separate HDD),
without closing or driving the user's main instance. Optional CMake
SCDA_TEST_OUTPUT_DIRECTORY redirects lab outputs; DLL relink now explicitly
tracks the host executable to prevent stale timestamp runtime rebuild prompts.

Native OPSAT fill 0x19890F reads 0x1429C3 twice and inverts its second tier.
The adapter supplies each camp separately and compensates that inversion.
Verified tiers: 100/100 ->4/4, 100/10 ->4/1, 10/100 ->1/4.
Real OPSAT and review screenshots confirmed the replacement panels. Preserve
Lst_TrustOverall: it is the rule table, not the old gauge. Review forecast
0x1BA097 now uses a state copy, respects saturation and reuses applied credits.
Native rule value/name helpers 0x1BB4F7/0x1BB476 show NSA contributions and
accurate enemy/civilian kill/KO labels. Fixture through actual statistics
application 0x1BB69C: 80/90 ->73/90; repeat and renewed preview stay73/90.

Hisham correction: x10_kinshasa_b decoded 0x2e0e70 and following script
completes rescue and cancels both kill and body objectives after successful
fake death. Missing HishamBody must not imply exposure. Actual script-selected
Localize(section=p_x10_kinshasa_b_comms, key=Speech_0050L) explicitly reports
Emile seeing the escape; Speech_0024L accepts the story. Hook after original
Localize 0x70FC9 observes the selection independent of subtitle visibility.
Exposure revokes proof credit, then applies JBA-15 once. Explicit evidence also
undoes older hisham-no-proof inference credits. Earlier interpretation of
Speech_0036L..0040L as a confession was insufficient and corrected in reference.
Native goal/localization fixture:75/75 ->90/75 ->90/80 ->90/60. Repeated exposure,
acceptance after exposure and repeated proof stay90/60. Pure tests include this
and inferred-penalty migration. Full story playthrough is still outstanding.

Private evidence: lab-opsat-fixed.png, lab-review-final.png,
lab-ui-validation.json and lab-hisham-validation.txt under
C:/Users/bastien/scda-trust-analysis/. Remaining campaign/equipment/crisis,
protected-police records and legacy profile-history limits are documented in
knowledge/TRUST_IMPLEMENTATION.md (v0.3). Original user saves were untouched
by this follow-up; test command bridge is opt-in only.

### Independent trust change pop-ups

Changed() now retains effective signed deltas for five seconds; contributions
within250ms aggregate exclusive-choice reversal and replacement. Both camp rows
show their own change, including+0, in the gameplay notification and OPSAT.
A changed score can reveal the compact panel without the old gauge. Duplicate
and zero-effect callbacks do not trigger a toast. Native fixtures captured:
NSA+15/JBA+0 and NSA+0/JBA-15. Evidence:trust-popup-independent.png,
trust-popup-loss.png, trust-opsat-popup.png in the private analysis folder.
Tests used only the separate --instance2 lab and its HDD.

Campaign testing preference: removed the constant gameplay trust panel. DrawUi now shows compact trust only during its five-second change notification; OPSAT and mission review remain visible. No game process was running when the build started.

## Selected faction-row visual design

Implemented user-selected proposal B procedurally in the host draw list:
olive beveled metal, machining marks, corner fasteners, circular faction seals,
slanted ivory text and20 recessed segments. Partial cells retain exact0..100
values; track length is fixed across both camps even when only one has a delta.
Gameplay pop-ups include affected factions only and remain temporary. OPSAT
and review retain both rows; review effective changes sit beside each score.
No game texture or LIN edits, and no new external font/bitmap dependencies.
Verified live gain pop-up and OPSAT using the separate --instance2 lab.
Final screenshots verified equal-length OPSAT tracks at75/60, JBA-only -15 pop-up and native review forecast68/60 with NSA-7. Evidence:trust-style-b-opsat-final.png, trust-style-b-loss.png, trust-style-b-review.png in the private analysis folder. Pure model tests passed. No user game process was stopped.

### Animated deltas inside faction meters

Moved signed changes into each affected segmented track. Fill uses smoothstep
from value-effective_delta to final score, with a1.25s transition after0.15s.
Affected cell ranges pulse briefly, including ghosted lost segments; delta rises
3logical pixels and fades at the end of the five-second pop-up. Review deltas
remain readable inside the meter without replaying an unrelated live animation.
Shadow alpha follows text alpha. Gameplay scores, ledger and saves are unchanged.
Live native fixtures: JBA75->60 and NSA75->90. Private frames:
trust-animation-loss-start.png, trust-animation-loss-end.png,
trust-animation-gain.png. Only the isolated test instance was used.
Delta position corrected after user feedback: midpoint(previous,current) mapped to track coordinates, with edge clamping to keep the full label inside the bar.
Delta placement refined per user: label starts just to the right of max(previous,current), with track-edge clamping.
Mission review presentation correction: summary rows no longer inherit an equal-valued live notification's timer, avoiding fill animation/fade followed by a snap when the five-second popup expires. README review capture will wait until the native rule reveal completes.
Delta/fill alignment revised after screenshot review: use exact new-value segmented endpoint (including gaps/partial cells) for label placement. Losses fix ivory fill at the final score and retract the red tail toward that endpoint; the old max-value label anchor is removed. README media refreshed.
Removed the opaque delta backplate: it covered the red loss tail and made the highlight appear detached. Delta readability now uses its fading text shadow only.

## v0.1.0-alpha.1 Windows prerelease

Built the published87e12fa source in a clean detached checkout, excluding local
texture experiments. Solo/versus host builds and pure trust-model tests passed.
Bundled xbe_recomp/Clang19.1.5/LLD19.1.5 independently translated, compiled and
linked both owner-provided game images (129/65 translation units). Those private
outputs are excluded from the release. Packaged solo executable reached a playable
map using its newly compiled DLL; native rescue/exposure fixtures75/75->90/60.
Archive41files,67635074bytes; SHA256:
641f1d8d4c386770838470de1703978b1ffcbd0d9b5d22215e4fdeb9aec25164.
Lint found zero failures; two documentation warnings are local evidence paths
already in the research notes, not bundled game data or runtime dependencies.
Published as prerelease with ZIP and SHA256SUMS.txt. Added binary installation
instructions and changelog. Campaign limitations remain explicitly documented.

## 2026-10-25 — integer-only balance experiment

User reported both camps reaching100 by Cozumel without deliberate optimization.
New Rebalance policy halves no-loss gains (rounding down) and doubles negative
story effects, while retaining positive trade-off rewards. Apply to the complete
pair before deferring discovery losses; do not transform Undo, saved values or
legacy effective credits. Contextual Hisham proof/exposure and deferred sabotage
use the same policy. Emergency recovery remains+25; initial state stays75/75.
Statistics halve accumulated KO gains before integer rounding and double both
loss contributions and their cap (+7/-30). Native review rows updated as well.
Baseline narrative JSON/table remain unchanged; active metadata and full45-rule
before/after comparison are in TRUST_BALANCE.md. Existing released ZIP unchanged.
Pure tests passed. Native PassengerLog75/75->77/77 and ShipEngine->79/77;
repeat unchanged. Opposing phone choice95/50->100/50->35/80 confirms loss-60,
reward+30 and exact saturated credit reversal. Native statistics80/80 forecasts
and applies64/80; repeat unchanged. Tests use the isolated instance2 HDD only.
Native Hisham sequence75/75->82/75->82/77->82/45 verified reduced rescue/proof and doubled exposure. Emergency recovery10/80->35/80 verified unchanged+25.

## Canonical trust reference consolidated

Corrected TRUST_REFERENCE.md itself rather than only adding a balance-note link.
All45 story rows now show active integer values, preserving original scalar
values as historical evidence. Consolidated deferred sabotage, Hisham evidence,
choice reversals/saturation, crisis/recovery guards, current statistics/caps,
selected faction-row UI/animation placement, checkpoint compatibility, version
scope and verification limits. Updated narrative examples and removed stale
future-implementation wording. Verified all45 rows against JSON baseline plus
current runtime policy and checked local links. Companion documents now point
to this canonical current reference; no runtime code or game saves changed.

## Statistics loss cap raised to60

At user request, changed only the combined statistics negative accumulator limit:
min(base_loss,30)*2 now allows-60 instead of-30. Enemy/civilian costs remain-4/-10;
other statistic penalties unchanged, positive cap+7 unchanged, JBA unaffected.
All loss categories share the one cap; story events are separate. Updated the
canonical reference formula, companion docs, metadata, README and changelog.
Regression assertions passed at5/6/7civilian deaths (-50/-60/-60),14/15enemy
deaths (-56/-60), mixed categories, inactive rules and duplicate suppression.
Local solo executable/DLL rebuilt; published alpha1 package is unchanged.


## Low-trust consumer audit — 25 October 2026

Read-only audit of alpha.2 downstream trust consumers. Live reflected thresholds
0/40/60/100 and Kinshasa warning/critical pattern mappings resolved. Identified
EMissionManager.SetLoadupType (NSA-tier-based faction kit) and native
EComputerSystem.DisplayFactionComponents 0x142BAE (normal/secure computer faction
components) as remaining opposing-scalar assumptions. OPSAT substitution applies
only inside 0x19890F; those other consumers read NSA. Also flagged initial-map
StartInitialPattern and both-low dispatch selection; ending coverage remains open.
See knowledge/TRUST_LOW_TRUST_AUDIT.md for exact evidence, branches and limits.
Private snapshot/bytecode remains outside the repo. No runtime or release changes.


## Independent downstream faction consumers

Adapted named script SetLoadupType and StartInitialPattern via native script
executor0x680BA. Kits qualify independently at61+, merge counts by maximum and
preserve explicit equipment restrictions; elite and multiplayer remain native.
Replaced native DisplayFactionComponents0x142BAE with independent <=40 faction
filtering for NormalAccess and SecureAccess, retaining argument evaluation,
hide flags and active computer/EEV refresh. JBA40 warning projection aligned.
Native fixtures verified loadout types9/9/10/7 for75/75,75/0,0/75,0/0, actual
kit configuration and both component arrays/all faction tags. Backported to both
releases with each release's prior balance retained. Full mission/end coverage
remains limited as documented.
