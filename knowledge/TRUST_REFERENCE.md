# Reference — NSA / JBA trust: decisions and active rules

Version **0.4 — integer balance and consolidated decisions**, 25 October 2026.

Target game: **Splinter Cell: Double Agent, original Xbox version B**, through
SCDA-XBERecomp. The two independent trust scores are based on
**version A (Xbox 360, PC and PS3)**; their effects are adapted to the objectives
and narrative context of the version B campaign.

This document is the main reference for the agreed decisions: active amounts,
narrative conditions, deferred effects, statistics, recovery, interface and
saves. It also retains evidence from the inventory of the original system.
The technical documents [TRUST_IMPLEMENTATION.md](TRUST_IMPLEMENTATION.md) and
[TRUST_BALANCE.md](TRUST_BALANCE.md) provide verification and before/after
comparisons; they are not required to understand the active rules below.

**Version scope:** the active values described here apply to the current code,
the local build and **v0.1.0-alpha.2**. The **v0.1.0-alpha.1** ZIP retains its
previous balance. This balance pass did not replace it.
The complete campaign, every crisis and all endings still require validation.

## How to read this document and assess the evidence

- **Established from files:** identifiers, constant amounts, flags, native calls,
  objective text and associated dialogue.
- **Narrative interpretation:** what actions mean to each faction, accounting
  for what that faction actually knows.
- **Active rule:** a design decision implemented by the mod; the NSA/JBA columns
  show the current integer balance, not the original developers' values.
- **Requires in-game validation:** reachability of every branch, notifications,
  event replay, exact discovery sequences and balance.

The inventory covers recognized constant calls in all installed solo maps,
with additional checks of co-op maps and common code. It does not formally
prove the absence of indirect calls or calls with dynamic arguments. Native
tests and captured footage are distinguished from a complete campaign
playthrough, which remains unfinished.

## Inventory results

| Set | Static result |
| --- | ---: |
| Solo maps examined | 17 |
| Solo objective declarations | 139 |
| Distinct solo rules after grouping by section + ID + amount | 111 |
| Distinct narrative objectives with nonzero amounts | **45** |
| Trust-recovery objectives | 19 |
| `TEMPNSA` / `TEMPJBA` test declarations | 10 |
| Distinct objectives with zero amounts | 37 |
| Constant solo state-change calls | 208 |
| Retained direct constant `EPattern.ModifyTrust` call | 1 |
| Co-op objective declarations examined | 44, all with zero amounts |

Both HQ maps contain code for **both visits**. A declaration appearing in each
LIN therefore does not represent a reward to apply twice. Some objectives are
also declared in one map and completed in the next.

The addresses of every declaration and state change are listed in
[TRUST_TRIGGER_INVENTORY.md](TRUST_TRIGGER_INVENTORY.md).
The [TRUST_RULES_REFERENCE.json](TRUST_RULES_REFERENCE.json) catalogue retains
origins, constant transitions and baseline narrative values. Its `balance_policy`
field describes the current transformation. The `proposal` fields and generated
C++ table deliberately retain the baseline: the runtime applies `Rebalance`
once. The 45 rows in this document show the active results after that transformation.

## Correcting the meaning of the original value

**Positive delta = NSA direction; negative delta = JBA direction.**
The property is named `EPlayerController.Trust` and corresponds to byte `+0x360`
on the controller used by menus. The initial interpretation, "0 = NSA, 100 = JBA",
was incorrect: the logos enabled at the endpoints identify the faction in trouble.
This correction supersedes that interpretation in the first report.

The declaration for keeping Barnham alive is **+15**; the declaration for his
death is **−15**. The direction parameter agrees with the `bJBATrust` name found
in script names: true reverses the amount's sign.

The `EPlayerController.ModifyTrust` script body was found in `common.lin`.
Its header is at decompressed offset `0x17f295`; its code starts at `0x17f2a6`
and declares 102 bytes. The update corresponds to
`Trust = clamp(Trust + trustpt, 0, 100)`, surrounded by trust-tier handling.
Addition and clamping were cross-checked against their native functions,
`0x00069954` and `0x00069FED` → `0x00036D16`, respectively.
The wrapper and objectives have been exercised through native calls. Complete
coverage of crises, timers, equipment rewards and direct readers remains open.

Downstream-effects audit (25 October): [TRUST_LOW_TRUST_AUDIT.md](TRUST_LOW_TRUST_AUDIT.md).
Mission kits and computer faction components now use the relevant faction's
score in the updated alpha.1 and alpha.2 releases. Kits qualify at 61+; their
quantities are combined by maximum when both factions qualify. Recovery
components appear at 40 or less for their own faction.

## Paths that actually trigger ModifyTrust

```text
Mission script
  → AddGoal(..., amount, JBA flag)
  → SetGoalStatus(ID, completed)
  → native objective processing
  → EPlayerController.ModifyTrust(delta)

Mission script → EPattern.ModifyTrust(amount, bJBATrust)
  → EPlayerController.ModifyTrust(delta)

Mission review → action counts → mission-rule calculation
  → EPlayerController.ModifyTrust(delta)

Menu / technical command → EPlayerController.ModifyTrust(delta)
```

The event wrapper is at `0x00092413`. Its four identified native call sites are:

| Call address | Origin | Active mod handling |
| --- | --- | --- |
| `0x00095B86` | Native VM `EPattern.ModifyTrust` | Classify the call by context; do not infer two effects from its sign alone |
| `0x000E9D59` | Objective completion | Look up the rule by mission, section and ID |
| `0x001BB92A` | General-action review | Process categories before reducing them to one delta |
| `0x00181284` | Menu dispatcher, delta `100 - Trust` | Technical command, no narrative reward |

Native identifiers confirmed by XBE initializers:

| Operation | Native VM | Call bytes | Native function |
| --- | --- | --- | --- |
| `EPattern.AddGoal` | `0xD05` | `6D 05` | `0x0009B19F` |
| State change through EPattern | `0x8AE` | `68 AE` | `0x0009B346` |
| State change through the statistics container | `0x3EB` | `63 EB` | `0x000EA3F2` |
| `EPattern.ModifyTrust` | `0xFC0` | `6F C0` | `0x00095B32` |

In `AddGoal` calls, the **13th parameter** supplies the trust amount; the
**12th** supplies the flag that reverses its sign. This was traced through
`0x0009B19F` → `0x000EA452` to `entry + 0x38` and mask `0x02` at
`entry + 0x54`. The other intermediate parameters are not amounts.
Omitted optional parameters use their native defaults.

Processing at `0x000E9CE4` applies the amount when the requested state is 1.
There is no visible "previous state already completed" check before this call.
The mod therefore prevents repeated awards through a persistent credit ledger,
in addition to any protections provided by the scripts.

## Active design and balance rules

### Independent values and starting scores

- Two **integer** scores, each clamped to **0–100**; no decimals or automatic
  complementary relationship between the factions.
- New mod state starts at **NSA 75 / JBA 75**. This starting point did not change
  during rebalancing.
- A successful covert NSA operation does not automatically cost JBA trust:
  discovery, visible failure or a known refusal is required.
- Helping the JBA to maintain an authorized infiltration does not automatically
  cost NSA trust. Killing innocents, sacrificing allies and terrorist attacks
  retain their consequences.
- Shared benefits are possible when justified by the story. Both meters can
  still reach 100; the balance aims to make that accumulation less easy.
- Originally neutral objectives remain **0/0**.

### Halved gains and stronger losses

Baseline narrative values use the 5/10/15/30 scale. The active adjustment of
25 October then applies the following policy:

| Baseline situation | Active decision | NSA/JBA example |
| --- | --- | --- |
| Neither delta is negative | Halve each gain, rounding down to an integer | +5/0 → +2/0; +15/0 → +7/0 |
| Both factions gain without a loss | Halve both gains | +5/+5 → +2/+2 |
| At least one faction loses trust | Double each loss, except −30 becomes −40; retain the other faction's positive gain | −5/+5 → −10/+5 |
| Loss without an opposing reward | Double the loss | 0/−15 → 0/−30 |
| Reversal of an existing credit | Remove its exact stored effective change; do not reapply the policy | An old effective +5 gain is reversed by −5 |

Classification uses **the complete narrative pair before any discovery loss is
deferred**. A +30/−30 sabotage becomes +30/−40, even if the −40 component is
applied only at mission exit. Its immediate gain is therefore not reduced to
+15 merely because its loss is temporarily deferred.

Adjustment included in alpha.2 (updated release): baseline −30 story losses
become **−40** instead of −60. Other losses remain doubled; the statistics cap
remains −60. Starting from 75, this decision leaves 35; an already low score
can still trigger a crisis. The two disarmings at −30 each can still stack:
the exception applies to each baseline value, not their sum.

Preserved exceptions: **starting scores 75/75** and **emergency recoveries +25**.
The latter must still allow a faction to leave crisis range. Repeatable
statistics use their own integer accumulation, described below.

### One-time awards, opposing choices and saturation

One award per objective identifier and mission family; one award per distinct
bomb. The `_a`/`_b` variants share a family, and the two HQ visits are distinguished
despite duplicated script declarations. The ledger stores both the requested
delta and the delta actually applied after clamping.

Exclusive pairs are Barnham alive/dead, station chief alive/dead, Yeager handed
to Lambert/reported to Emile, Lambert's cover confirmed/revealed, phone sabotaged/
preserved, and Hisham rescued/killed. A new event first reverses the effective
credit of its opposite, then applies its own rule. The second HQ's bombs and
sabotage discovery also have specific reversals, described in the special cases.

A saturated gain cannot create points when reversed. If +30 actually added only
+5 to a score of 95, reversing it removes **5**, not 30. The train's old scalar
compensation is ignored: the ledger already performs that reversal. Duplicates
produce neither a new credit nor a new pop-up.

## Narrative catalogue of the 45 nonzero-trust objectives

In the tables, **Original** is the single-meter delta, with +NSA and −JBA.
**Active NSA/JBA** are the independent changes after the integer adjustment of
25 October. The Original column is never rebalanced: it remains evidence of
the original game. Conditions and deferred discovery in the final column are
part of each rule.

### Ellsworth Prison

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `BarnhamAlive` | Spare Barnham | +15 | +15 | -30 | Protect a DOJ witness; Jamie personally witnesses the refusal to kill him. |
| `Retrieve` | Retrieve the disc from Jamie | +5 | +2 | +0 | Covert intelligence for Third Echelon; no JBA discovery has been established. |
| `NeedSeringe` | Find the syringe for Jamie | -10 | +0 | +5 | Protect the contact who enables the infiltration; the NSA authorized this operation. |
| `BarnHamDead` | Help Jamie kill Barnham | -15 | -30 | +15 | Killing the witness defies Lambert's order and directly reassures Jamie. |
| `Regroup` | Regroup with Jamie | -10 | +0 | +5 | Keep Jamie alive and continue the authorized infiltration. |
| `Disc` | Falsify the death records | -5 | +0 | +2 | Enable the escape and strengthen Sam's cover; no additional betrayal of the NSA. |

### JBA HQ — first visit

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQA_P_10` | Reduce the JBA's jamming | +10 | +5 | +0 | Secretly open an NSA surveillance window; apply a JBA loss only if another scene establishes discovery. |
| `CarsonMoss` | Strengthen Sam's false identity | -5 | +0 | +2 | Falsifying Moss's security check also serves the infiltration authorized by Lambert. |
| `YeagerLambert` | Hand Yeager over to Lambert | +15 | +7 | +0 | Lambert plans to make him disappear before Emile arrives; do not automatically attribute this extraction to Sam in the JBA's eyes. |
| `YeagerEmile` | Report Yeager to Emile | -15 | -30 | +15 | Eliminate the lead Lambert wanted to pursue and give Emile the proof he expects. |
| `OBJ_HQA_P_20` | Open the JBA server to the NSA | +10 | +5 | +0 | Covert intelligence; a JBA penalty requires confirmed discovery, not technical success alone. |
| `OBJ_HQA_O_20` | Collect Enrica's fingerprints | +5 | +2 | +0 | Covert intelligence collection; other fingerprints with an original zero amount remain neutral. |

### Armored train

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `P_WoltCode` | Obtain the vault code | -10 | +0 | +5 | A step in the robbery assigned by Emile, necessary to maintain cover. |
| `P_KillChief` | Kill the station chief | -15 | -30 | +15 | Proof of loyalty demanded by Emile; Lambert explicitly forbids killing an innocent. |
| `P_ChiefAlive` | Leave the station chief alive | +15 | +15 | -30 | Follow the NSA order while refusing an explicit demonstration of loyalty. |
| `O_DetachCars` | Detach the rear carriages | +5 | +2 | +0 | Move employees out of danger without preventing the main robbery. |
| `P_DisconnectCom` | Cut the train's communications | -10 | +0 | +5 | Delay the alert and enable the undercover robbery. |

### Cozumel

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `ChipCellPhone` | Sabotage Enrica's detonator | +30 | +30 | -40 | NSA +30 for the secret sabotage; JBA −40 at mission exit in the current implementation. A deferred loss, not a no-loss reward. |
| `ProCellPhone` | Preserve the bomb's detonator | -30 | -40 | +30 | Deliberately allow the attack despite Lambert's order; this outcome replaces sabotage and never stacks with it. |
| `PlaceRelay` | Install the relay with Enrica | -10 | +0 | +5 | Maintain cover before the phone decision; do not penalize the final outcome a second time. |
| `PassengerLog` | Retrieve the passenger manifest | +5 | +2 | +2 | The description concerns identifying the victims, and Enrica reports an explicit request from Emile: both factions benefit. |
| `GetBomb` | Retrieve the bomb | -10 | +0 | +5 | Continue the undercover mission with Enrica; the ship's fate has its own major event. |
| `CameraRecord` | Erase the video recordings | +5 | +2 | +2 | The text requires keeping Sam's presence secret; maintaining his cover protects both factions' operational interests. |
| `ShipEngine` | Disable the ship | +5 | +2 | +0 | Reduce casualties ashore without making the sabotage automatically known to the JBA. |

### Sea of Okhotsk

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `Mercenaries` | Neutralize the camp's mercenaries | -10 | +0 | +5 | Allow Emile's allies to board; the objective accepts lethal or nonlethal action. Casualties are evaluated separately. |
| `Communications` | Cut the mercenaries' network | -10 | +0 | +5 | Prevent an alert during the tanker takeover. |
| `Bombs` | Disarm the charges in the ice | -5 | +0 | +2 | Keep the tanker trapped until its capture; these charges are not the JBA's terrorist bombs. |
| `KeyCode` | Retrieve the encryption key | +5 | +2 | +0 | Monitor the smugglers' network for the NSA. |
| `Freight_Done` | Identify the cargo | +5 | +2 | +0 | Inform the NSA about what the JBA could exploit; award only once despite two completion branches. |
| `Tracker_Done` | Plant the tanker's tracker | +5 | +2 | +0 | Secretly track the ship after its capture by the JBA. |
| `Bombs_Done` | Prevent the tanker's scuttling | -10 | +0 | +5 | Save the ship intended for the JBA; do not confuse this disarming with sabotaging its terrorist attacks. |

### Kinshasa

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_10_P_50` | Check Emile's partners | -10 | +0 | +5 | Confirm the schedule and the loyalty of the other two groups. |
| `OBJ_10_O_10` | Give the NSA intelligence on Takfir | +5 | +2 | +0 | Covert intelligence collection on a terrorist group. |
| `OBJ_10_O_20` | Give the NSA intelligence on Massoud | +5 | +2 | +0 | Same intention, separate target and objective. |
| `OBJ_10_P_60` | Kill Hisham | -15 | -30 | +15 | Execute an allied agent at Emile's request; preserve the gravity of the original choice even though Lambert prioritizes the mission. |
| `HishamBody` | Blow up the vehicle supposedly containing Hisham | -5 | +0 | +2 | Validate the proof given to Emile. Another body can be used for the deception: no NSA loss for a successful fake death; the actual killing is already penalized by its objective. |
| `OBJ_10_P_30` | Rescue Hisham | +15 | +7 | +0 | Covert rescue: NSA +7 / JBA 0. If Emile witnesses the escape: a separate JBA −30 loss, without a second NSA credit; reverse any proof bonus already awarded. |
| `OBJ_10_P_70` | Secure the extraction and AA gun | -10 | +0 | +5 | Enable the undercover group's departure; do not classify mandatory neutralizations as civilian casualties. |

### JBA HQ — second visit

| ID | Event | Original | Active NSA | Active JBA | Intention and conditions |
| --- | --- | ---: | ---: | ---: | --- |
| `OBJ_HQB_P_10` | Steal Emile's contacts | +10 | +5 | +0 | Obtain the contacts for Third Echelon without informing the JBA. |
| `OBJ_HQB_P_20` | Analyze the red mercury | +10 | +5 | +0 | Verify the weapon's authenticity for Williams; a covert action. |
| `P_LambertNOTCover` | Reveal Lambert's identity | -15 | -30 | +15 | Preserve the original game's moral direction and the sacrifice of an allied agent; Williams nevertheless encourages this choice: explicitly preserve this narrative tension. |
| `P_LambertCover` | Confirm Lambert's cover | +15 | +15 | -30 | According to the objective text, saving Lambert maintains Emile's suspicions. |
| `OBJ_HQB_P_30` | Disarm the Nashville bomb | +15 | +15 | -30 | NSA +15 immediately; JBA −30 at mission exit. Separate credit and penalty for each bomb; reverse them if KeepBombs replaces this outcome. |
| `OBJ_HQB_P_31` | Disarm the Los Angeles bomb | +15 | +15 | -30 | NSA +15 immediately; JBA −30 at mission exit. Separate credit and penalty for each bomb; reverse them if KeepBombs replaces this outcome. |
| `KeepBombs` | Prevent the bombs from being neutralized | -30 | -40 | +30 | Sending the bombs along another route deliberately compromises their interception by the NSA. |

## Special cases and agreed narrative conditions

### Station chief: reversing a previous outcome

A direct `ModifyTrust(15, true)` call exists in `x04_moneytrain_a`, at decompressed
offset **`0x3579cc`**, in `Primary_Obj_Kill_Done_After_Alive`. Its original −15
delta removes the spared chief's scalar credit. This historical amount does
not become a second penalty in the mod.

Current rule without saturation: `spared (+15,−30)` → reversal `(−15,+30)` →
`killed (−30,+15)`. The choice's resulting award is therefore `(−30,+15)`, with
no residual old bonus. At the 0/100 bounds, reversals use the changes
**actually** credited, including those from an old save. They are neither
doubled nor halved.

### Hisham: actual death, fake death and discovered deception

`HishamBody` describes a body, but keys `Speech_0016L` through `Speech_0020L`
in `p_x10_kinshasa_b_hisham_script` introduce using another body of a similar
build. Reminders in `p_x10_kinshasa_b_hisham_barks` ask Sam to prepare the car.
The ID alone therefore does not establish that Hisham was killed.

| Branch / event | Active NSA | Active JBA | Handling |
| --- | ---: | ---: | --- |
| Rescue Hisham, `OBJ_10_P_30` | +7 | 0 | Award once; the fake death can preserve cover |
| Proof, `HishamBody` | 0 | +2 | Award once, only if no witnessed escape is recorded |
| Emile witnesses the escape, `Speech_0050L` | 0 | −30 | Reverse the effective proof bonus, then apply this loss once |
| Kill Hisham, `OBJ_10_P_60` | −30 | +15 | Replace the rescue and reverse any prior escape penalty; the +2 proof reward remains separately possible |
| Emile accepts the account, `Speech_0024L` | 0 | 0 | Record acceptance; do not erase an already witnessed escape |

The hook observes the `Localize` call **selected by the script** in
`p_x10_kinshasa_b_comms`, before subtitles might be displayed. Keys
`Speech_0036L` through `Speech_0040L` do not establish a sufficiently clear
admission: no additional penalty is inferred from their earlier interpretation.

The successful fake-death branch completes `OBJ_10_P_30`, then cancels
`OBJ_10_P_60` and `HishamBody` (`x10_kinshasa_b`, decoded offsets
`0x2e0e70`–`0x2e0eb7` and following). **An absent or canceled proof objective
never establishes discovery on its own.** The old "missing proof at mission
exit" fallback was removed. Explicit acceptance or escape can also remove that
old incorrect credit from an earlier save.

Current native test: **75/75 → 82/75 → 82/77 → 82/45**. The final transition
removes the effective +2 bonus and then applies −30; it is not an additional
−32 penalty to repeat. Repeated calls, late proof and acceptance after discovery
award nothing again. The complete narrative sequence still needs a playthrough.

### Cozumel: shared benefits and deferred consequences

The manifest identifies potential victims; Enrica also mentions Emile's request
in `p_x06_cozumel_a_enrika_coms:Speech_0082L`. `PassengerLog` therefore benefits
both factions: **+2/+2**. `CameraRecord` protects Sam's secret presence and cover:
also **+2/+2**. `ShipEngine` gives **+2/0**. `PlaceRelay` and `GetBomb` each give
**0/+5**, without an additional NSA penalty for continuing the infiltration.

Sabotaging the phone gives **NSA +30 immediately**. The **JBA −40** loss is
applied **at mission exit** in the current code, once per sabotage credit.
This deferral is an implementation choice: the exact scripted discovery point
still needs to be connected/validated. Do not claim Sam is discovered during
the covert action, or give it the halved reward of no-loss tasks: its complete
pair is +30/−40.

`ProCellPhone` gives **−40/+30** and replaces sabotage. The mod removes the
effective NSA credit and any recorded `bomb-discovered` penalty before applying
the opposing outcome. Generic deaths remain separately evaluated according to
the enabled statistics categories.

### Second HQ: distinct bombs and final choice

Nashville and Los Angeles each have their own credit: **NSA +15 immediately**,
then **JBA −30 at mission exit**. Discovery is not instantaneous. Together the
two bombs can therefore produce +30/−60 before saturation. `KeepBombs` gives
**−40/+30** and removes disarming credits and any discovery penalties already
applied. Later disarming removes the `KeepBombs` credit; each new award remains
protected by the ledger.

### Lambert and Yeager: narrative intention and exclusive decisions

In `p_x11_hq5_r_communications_main:Speech_0015L` through `Speech_0022L`,
Williams encourages sacrificing Lambert to maintain cover. Nevertheless, the mod
preserves the original moral direction: confirming his cover gives **+15/−30**,
revealing his identity gives **−30/+15**. This choice does not mean every NSA
order requires saving Lambert.

For Yeager, Lambert's extraction remains covert: **+7/0**. Reporting him to
Emile gives **−30/+15**. These outcomes replace one another rather than stacking.

## Recovery triggers: 19 rules, not 19 story choices

Variants of `P_TrustNSA`, `P_TrustJBA`, `P_TrustNSAHq2` and `TrustNow` correspond
to cover or loyalty crises.

| Family | Original | Active effect | Context |
| --- | ---: | --- | --- |
| `P_TrustNSA`, `P_TrustNSAHq2` | +25 | NSA +25, JBA 0 | Contact Williams / prove that the operation remains under control |
| `P_TrustJBA` | −25 | NSA 0, JBA +25 | Contact Emile to reassure the JBA |
| `TrustNow` | −25 | NSA 0, JBA +25 | Return to the public area and report to Emile at HQ |

The train, Cozumel, Okhotsk and Kinshasa maps have their own variants; HQ has
its own branches. The JSON and appendix retain each origin. The corresponding
completion calls have been found.

Recoveries remain **+25**, an explicit exception to the reduction of small gains.
The runtime awards them only when the relevant faction is at **20 or less**.
A per-faction guard prevents repetition; it rearms when that score crosses from
above 20 to 20 or less. Revisiting a computer therefore does not arbitrarily
award +25.

The adapter starts crisis events from **low** trust, never from high trust in
the other faction. It projects the faction in trouble into the original system
and remembers the dispatched level to avoid restarting the warning at every
event. Projection selects scores below 50; actual tiers, branches and timers
remain those of the original scripts. No new death timer has been added.
Kit and computer-component readers are adapted; complete campaign variants
still require checking. Map entry uses the independent projection, and JBA 40
now projects to warning level 61.

## Technical triggers and neutral objectives

### Ten test declarations

`TEMPNSA` is +20 and `TEMPJBA` is −70 in both Cozumel maps, Okhotsk B and both
Kinshasa maps. The declarations and their completions exist; branch names
`TempDebugNSA` / `TempDebugJBA` appear among script names. Their associated
localization entries are null.

Active decision: **no effect** for objectives whose ID begins with `TEMP`, with
a diagnostic if one triggers. These are not ten narrative choices. Normal
reachability has not been demonstrated; do not state that they have been proven
unreachable.

### Thirty-seven zero-amount solo rules

Keep **NSA 0 / JBA 0** for these events, including Iceland progression, travel/
extraction steps, some fingerprints and New York's final objectives. Their exact
list is in the JSON and the appendix's 139 declarations. An objective's narrative
importance does not necessarily make it an original source of trust; this
reference does not invent a reward for it.

The **44 recognized co-op declarations** also have zero amounts. This redesign
does not add a loyalty system to them.

### Writes that bypass the event

`TRUSTVALUE=` and inter-mission history can write/restore trust without going
through an objective. These are persistence/initialization paths, not narrative
actions. The mod derives no reward from them and restores both scores from the
metadata described below. Old menu deltas or unclassified calls are not used to
invent narrative effects.

## General actions in mission review

The review trust calculation at `0x001BB69C` and its preview at `0x001BA097`
use the same groups. Categories were cross-checked against counters displayed
in `0x001BBFE6` and local `MISSIONREVIEW` labels. Grouping loses distinctions
that are useful for two meters.

| Internal type(s) | Category | Original coefficient | Mission flags |
| --- | --- | --- | --- |
| 2 + 5 | Enemies killed + civilians killed | controller +0x4EC | context +0xC60 |
| 0 + 3 | Enemies knocked out + civilians knocked out | +0x4F0 | +0xC64 |
| 7 | Enemy detections | +0x4F4 | +0xC68 |
| 10 | Alarms | +0x4F8 | +0xC6C |
| 6 | Bodies found | +0x4FC | +0xC70 |

The static inventory does not establish every mission's effective coefficient
values. The active numbers below are chosen by the mod and must not be presented
as the original values. Both original accumulators are capped at 15, and this
path's result is clamped to 5..95 before `ModifyTrust`.

### Active rules for repeatable actions

Applied at mission review, **NSA only**, for categories enabled by mission flags.
These amounts are mod decisions, not the original coefficients, which are
described separately above.

| Recorded action | Active NSA | Active JBA | Condition / limit |
| --- | ---: | ---: | --- |
| Enemy killed | −4 | 0 | Restraint rule enabled; named deaths already handled by a choice are removed from the count |
| Civilian killed | −10 | 0 | No automatic JBA gain; civilian classification comes from statistics |
| Enemy knocked out | +1 per two | 0 | Accumulate KOs then divide by two, rounding down; a single KO gives 0 |
| Civilian knocked out | −2 | 0 | Knockout rule enabled |
| Detection | −2 | 0 | The game's incident counter |
| Alarm | −4 | 0 | Alarm rule enabled |
| Body found | −2 | 0 | Stealth rule enabled |

Exact calculation: `gain = floor(min(enemy_KOs, 15) / 2)` when the KO rule is
enabled; `base_loss` sums enabled old costs (2 per enemy killed, 5 per civilian
killed, 1 per civilian KO, 1 per detection, 2 per alarm and 1 per body).
The NSA delta is **`gain − 2 × min(base_loss, 30)`**. The accumulators have
independent caps: **maximum gain +7, maximum loss −60**. The final score is
clamped to 0–100; the historical 5–95 clamp no longer limits the preview. JBA
receives 0 from this generic calculation. The loss cap was raised from −30 to
**−60** so repeated reckless actions remain costly: six civilian kills or fifteen
enemy kills reach it when the death rule is enabled and there are no KO bonuses.
Detections, alarms and bodies found share this same cap; it is not an additional
cap per category. Per-victim costs are unchanged.

The review table shows raw rebalanced contributions by category, while both
meters show the effective total after caps and saturation. The preview uses a
copy of state and awards nothing. Once the review is applied, it reuses the
effective credit instead of forecasting it again. Native application is protected
against repetition per map.

The named deaths of Barnham, the station chief and Hisham are subtracted from
death statistics if their choice has already been credited, preventing a second
generic penalty. The code removes a civilian death first, otherwise an enemy;
classification of a particular target therefore remains a verification limit.
New York's mandatory combat deaths are exempt; other enabled rules still apply.

**Police limitation:** a protected officer recorded as an enemy cannot yet be
distinguished from another enemy using these records alone. Therefore do not
promise −10 for every officer: only the civilian type receives that cost.
Actions expected by Emile already award JBA trust through their objectives.
No generic "every killing rewards the JBA" rule has been added.

## Selected interface

- **Style B, faction rows:** olive metal frame, circular NSA/JBA badge, twenty
  ivory segments, slanted labels and an integer score. Each segment represents
  5 points; partial segments represent the other integer values. Both tracks
  have the same length. Trust at 20 or less turns red.
- **OPSAT:** two permanent rows on the trust page; the native frame is retained
  with a localized legend explaining independence. Both native textual tiers
  are read for their own faction, without an artificial JBA inversion.
- **Gameplay:** no permanent diagnostic display. Five-second pop-up only for
  affected factions; no +0 row for the unchanged faction.
- Reversals and awards within 250 ms are grouped into one net change. A zero
  event or one already credited does not produce a notification.
- The signed delta is **inside the track, just after the new value's edge**,
  not at the bar's center, in the middle of lost points or beyond the old maximum.
  It accounts for partial segments and gaps; near 100 its position is clamped
  to remain readable inside the track. No opaque background hides the loss.
- For a loss, ivory fill stays at the new value and the red tail starts at that
  same edge, retracting from the old value. For a gain, fill moves toward the
  new value with a green glow. The transition lasts 1.25 s after a 0.15 s hold,
  with a slight vertical delta movement and a fade during the final 0.65 seconds.
  Scores apply immediately; only their presentation is animated.
- **Mission review:** separate results below the native table; each delta is
  inside its track. The forecast is stable and independent of gameplay pop-up
  timing, even when the deltas match. Native table reveal remains enabled;
  reference captures wait until it completes.
- The console, settings and hide-HUD option hide this presentation. Rendering
  is host-side; no game texture or LIN resource is modified.

## Saves and decision compatibility

The ledger stores scores, mission/map, requested/effective credits and warning-
adapter state. Xbox checkpoint bytes retain their original format. Additional
information is written atomically to versioned files beside the executable:

```text
trust-state/<checkpoint name>/<SHA1 of relative path>/<SHA1 of checkpoint bytes>.txt
```

This folder is separate from the Xbox cache; it must accompany saves when
copying, migrating or restoring them. A save without this metadata imports the
old scalar as **NSA=x / JBA=100−x**: its independent-trust history cannot be
reconstructed. New state remains 75/75.

The port's process changes use a per-instance handover file, read only on an
explicitly marked relaunch. A cold start ignores an old handover file. Pending
initialization when loading from the menu is preserved until the first playable
controller appears.

**No retroactive rebalancing:** loading an old 100/100 state leaves it at 100/100.
Future events use the new rules. Older credits retain their effective changes
and are reversed at those values without recalculation. Replaying a credited
objective does not award its reduced reward a second time. A new campaign is
the best overall comparison.

The redesign targets solo play; co-op and versus retain their original behavior.
Full regression coverage is not validated. Native profile history remains a
scalar; current scores and their independent ledger are in the mod metadata.
Historical charts have not been converted to two histories. To return to the
original port, use a pre-mod save and an unmodified build; the old game cannot
read the independent JBA history.

## Current verification and limitations

| Check | Result / scope |
| --- | --- |
| C++ model tests | Validated: −30 → −40 exception, reversal after saturation, integer rounding, shared gains, trade-offs, other doubled losses, saturation, effective reversals and duplicate suppression |
| Manifest then engine, native calls | 75/75 → 77/77 → 79/77; repeating awards nothing |
| Saturated phone choice, native calls | Alpha.2 fixture: 95/50 → 100/50 → 35/80 with −60/+30. Current policy: expected 55/80 with −40/+30; not replayed in-game |
| Statistics: civilian kill, enemy KO, detection, alarm | Forecast 80/80 → 64/80; applied 64/80; repeating is unchanged |
| Hisham rescue/proof/escape, synthetic objectives and dialogue | 75/75 → 82/75 → 82/77 → 82/45 |
| Native NSA recovery | 10/80 → 35/80; other faction unchanged |
| Persistence / reload / duplicates | Validated on the previous version; format and old-credit reversal preserved, without claiming a complete playthrough of the new balance |
| Interface | OPSAT, pop-ups and mission review captured in-game; review table captured after reveal |

These tests use an isolated instance and controlled fixtures, not certification
of every narrative branch. Remaining validation includes:

- the complete campaign with the new balance and reachability of every branch;
- crises, timers, equipment and direct readers of the historical scalar;
- the exact sabotage-discovery moment, currently deferred to mission exit;
- Hisham's full natural sequence and all endings;
- distinguishing protected police in generic statistics;
- ordinary cross-map loading and complete multiplayer regression.

A raw `LOADGAME` issued from the title screen crashed in guest deserialization
during an earlier test; use normal menus for cross-map saves. No new download
behavior or game asset is required to apply these rules.

## Traceability, reproduction and coverage checks

Reference binary `assets/default.xbe`, SHA256:
`C742D61C9B8D8C1C4F886A4A4A69FFB65AB3961E0283DCAD33BFCFA963885143`.
Hashes of the 33 examined LINs (common, menu, 17 solo, 14 co-op) are retained in
the JSON. Some LINs contain texture replacements from earlier work: the offsets
belong to the files actually examined.

Local tool: [inventory_trust.py](../tools/analysis/inventory_trust.py).

```powershell
python tools/analysis/inventory_trust.py --output C:/Users/bastien/scda-trust-analysis
```

It decompresses LINs while checking every block size, reads constant calls and
their parameter terminators, extracts English text for private work and writes
its inventory **outside the project**. It rejects a different XBE hash because
native numbers are specific to this version. Full text, scripts and decompressed
streams remain outside the repository.

Checks performed:

- 183 recognized constant declarations, including 139 solo and 44 co-op; no
  parsing errors in these declarations.
- Amount and direction traced to native objective writes.
- All 74 distinct nonzero-amount solo rules have at least one associated
  constant completion call in their mission family.
- HQ duplicates and alternative station-chief declarations are grouped without
  removing them from the origins appendix.
- The single retained direct constant call is linked to its compensation branch.
- Additional false `6D 05 1F` patterns observed outside localization calls are
  in examined graphics data, not objectives.

Before claiming "all executable triggers are covered", some exclusive branches,
crises and reviews still need tracing, followed by checks for indirect calls /
dynamic arguments using a complete decoder or VM instrumentation. This document
keeps that limitation explicit.

## Context sources

**Primary sources:** scripts/constants in local LINs, the XBE and text from the
provided `dynamicxbox.umd`. Sections and keys cited here allow evidence to be
located in the private extraction. Table rules point to their sections and
offsets in the JSON.

External cross-check: [VampireHorde's Xbox guide](https://gamefaqs.gamespot.com/xbox/926929-tom-clancys-splinter-cell-double-agent/faqs/45455)
describes trust crises and the difference between objectives and action review.
It is a secondary source used to check context, never to invent amounts or
import Xbox 360 choices.

For the resource-analysis method, the [LIN research discussion](https://github.com/EliotVU/Unreal-Library/discussions/134)
explains why its package offsets must not be treated like ordinary Unreal
package offsets. All cited offsets therefore refer to the decompressed stream
actually read, not supposedly valid `SerialOffset` values.

Search queries used: "Splinter Cell Double Agent Xbox original trust objectives
walkthrough NSA JBA GameFAQs" and "Splinter Cell Double Agent Xbox lin umd Unreal
script format". These searches concern the initial inventory of 23 October.
Native tests and captures added afterward are listed above; they do not
constitute a complete campaign playthrough.
