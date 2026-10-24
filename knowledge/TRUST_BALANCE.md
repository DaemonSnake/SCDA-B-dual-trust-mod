# Integer balance experiment — 25 October 2026

Included in **v0.1.0-alpha.2**, applying to new trust events only. The older
v0.1.0-alpha.1 ZIP retains its original balance. All scores and deltas stay integers.

The complete current decision reference is [TRUST_REFERENCE.md](TRUST_REFERENCE.md).
This document keeps the compact baseline-to-active comparison for the balance test.

## Policy

- No baseline loss to either faction: halve each positive gain, rounding down.
  Examples: +5/0 becomes +2/0; +15/0 becomes +7/0; +5/+5 becomes +2/+2.
- If either baseline delta is negative, retain positive trade-off rewards.
  Example: -5/+5 becomes -10/+5.
- Double every new trust penalty, including discovered sabotage and Hisham exposure.
- Classify a complete story effect before deferring a discovery penalty. Cozumel
  chip sabotage remains NSA +30 immediately, then JBA -60 at mission exit.
- Initial state stays75/75. Crisis-recovery objectives stay+25 so a recovery
  can still lift the affected camp out of its emergency threshold.
- Existing saved scores/ledger entries are not rescaled. Reversals subtract the
  actual old effective credit once, without doubling or halving that reversal.

## Statistics

All enabled mission-statistics rules remain NSA-only. Sum enemy knockouts, cap
their baseline gain at15, then halve with integer rounding down: two KOs earn
one point; a single KO earns zero. Maximum gain becomes+7. Double the negative
contributions; the combined loss cap is now-60 (raised from-30).
Per-person costs stay-4 per enemy and-10 per civilian. All statistics penalties
share that cap; story penalties remain separate.

| Action | Old NSA effect | New NSA effect |
| --- | ---: | ---: |
| Enemy kill | -2 | -4 |
| Civilian kill | -5 | -10 |
| Enemy KO | +1 each | +1 per pair, rounded down per mission |
| Civilian KO | -1 | -2 |
| Detection | -1 | -2 |
| Alarm | -2 | -4 |
| Body found | -1 | -2 |

## Story rule comparison

The baseline JSON and generated table retain the original narrative proposals;
`Rebalance` in `trust_model.h` applies this policy once when awarding a new event.
Values below show the complete effect, including any delayed discovery loss.

| Objective | Baseline NSA/JBA | New NSA/JBA |
| --- | ---: | ---: |
| `BarnhamAlive` | +15 / -15 | +15 / -30 |
| `Retrieve` | +5 / +0 | +2 / +0 |
| `NeedSeringe` | +0 / +10 | +0 / +5 |
| `BarnHamDead` | -15 / +15 | -30 / +15 |
| `Regroup` | +0 / +10 | +0 / +5 |
| `Disc` | +0 / +5 | +0 / +2 |
| `P_WoltCode` | +0 / +10 | +0 / +5 |
| `P_KillChief` | -15 / +15 | -30 / +15 |
| `P_ChiefAlive` | +15 / -15 | +15 / -30 |
| `O_DetachCars` | +5 / +0 | +2 / +0 |
| `P_DisconnectCom` | +0 / +10 | +0 / +5 |
| `OBJ_HQA_P_10` | +10 / +0 | +5 / +0 |
| `CarsonMoss` | +0 / +5 | +0 / +2 |
| `YeagerLambert` | +15 / +0 | +7 / +0 |
| `YeagerEmile` | -15 / +15 | -30 / +15 |
| `OBJ_HQA_P_20` | +10 / +0 | +5 / +0 |
| `OBJ_HQB_P_10` | +10 / +0 | +5 / +0 |
| `OBJ_HQB_P_20` | +10 / +0 | +5 / +0 |
| `P_LambertNOTCover` | -15 / +15 | -30 / +15 |
| `P_LambertCover` | +15 / -15 | +15 / -30 |
| `OBJ_HQB_P_30` | +15 / -15 | +15 / -30 |
| `OBJ_HQB_P_31` | +15 / -15 | +15 / -30 |
| `KeepBombs` | -30 / +30 | -60 / +30 |
| `OBJ_HQA_O_20` | +5 / +0 | +2 / +0 |
| `ChipCellPhone` | +30 / -30 | +30 / -60 |
| `ProCellPhone` | -30 / +30 | -60 / +30 |
| `PlaceRelay` | +0 / +10 | +0 / +5 |
| `PassengerLog` | +5 / +5 | +2 / +2 |
| `GetBomb` | +0 / +10 | +0 / +5 |
| `CameraRecord` | +5 / +5 | +2 / +2 |
| `ShipEngine` | +5 / +0 | +2 / +0 |
| `Mercenaries` | +0 / +10 | +0 / +5 |
| `Communications` | +0 / +10 | +0 / +5 |
| `Bombs` | +0 / +5 | +0 / +2 |
| `KeyCode` | +5 / +0 | +2 / +0 |
| `Freight_Done` | +5 / +0 | +2 / +0 |
| `Tracker_Done` | +5 / +0 | +2 / +0 |
| `Bombs_Done` | +0 / +10 | +0 / +5 |
| `OBJ_10_P_50` | +0 / +10 | +0 / +5 |
| `OBJ_10_O_10` | +5 / +0 | +2 / +0 |
| `OBJ_10_O_20` | +5 / +0 | +2 / +0 |
| `OBJ_10_P_60` | -15 / +15 | -30 / +15 |
| `HishamBody` | +0 / +5 | +0 / +2 |
| `OBJ_10_P_30` | +15 / +0 | +7 / +0 |
| `OBJ_10_P_70` | +0 / +10 | +0 / +5 |

Hisham exposure is an additional contextual event: JBA -15 becomes -30;
any credited proof bonus is revoked at its actual stored value first. New proof
credits become+2. A saved earlier+5 credit is still reversed as+5, not+2.

## Test procedure

Use a fresh campaign to compare the full progression. Continuing an existing
100/100 save keeps those current scores: future losses and rewards use the new
policy, but past easy gains are not rewritten. Native test fixtures also cover
shared rewards, opposing choices, delayed penalties, statistics and reloads.

## Verification

- Pure model tests: no-loss rounding, both-camp gains, unchanged trade-off
  rewards, doubled losses, duplicate prevention and saturated reversals pass.
- Native PassengerLog:75/75 ->77/77; ShipEngine ->79/77; repeat unchanged.
- Native opposing phone choice at95/50: chip saturates100/50; opposing outcome
  reverses only its effective+5, then applies-60/+30 ->35/80.
- Native statistics fixture (civilian death, enemy KO, detection, alarm):
  forecast80/80 ->64/80; actual application64/80; repeat unchanged.
- Native Hisham path:75/75 ->rescue82/75 ->proof82/77 ->exposure82/45.
- Native emergency recovery:10/80 ->35/80, retaining the existing+25 recovery.

Cap regression checks: five civilians=-50; six/seven civilians=-60; fourteen
enemies=-56; fifteen enemies=-60. Mixed kill/detection/alarm losses share-60.
An identical applied statistics event remains deduplicated.
