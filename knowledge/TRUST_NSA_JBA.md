# NSA / JBA trust: locating the system and preparing two meters

**Current reference: [TRUST_REFERENCE.md](TRUST_REFERENCE.md)**, with trigger
inventory, narrative context and NSA/JBA proposals. This document retains the
initial technical findings, corrected after script analysis.

Static analysis from 23 October 2026. Addresses are XBE virtual addresses, not
host addresses in the PC process. The binary fingerprint is in `MODLOG.md`.
The conclusions below come exclusively from local files.

## Findings

Trust used by the interface is a single value, read as a byte at
`object + 0x360`. The object is obtained from the menu manager through
`sub_0017B720`. Its exact class name remains to be confirmed.

The 0 endpoint enables `logo_NSA`; the 100 endpoint enables `logo_JBA`.
These logos indicate the faction in trouble: **positive deltas favor the NSA,
negative deltas favor the JBA**. The initial reversed interpretation of these
endpoints was incorrect and is superseded by the scripts and objectives.
Intermediate values move through the `A_trust` animation. Mission review
calculates positive and negative contributions, subtracts them, then sends a
single delta to the `ModifyTrust` event.

Two meters therefore require two states and classification before that
subtraction. Setting `NSA = 100 - JBA` would reproduce the existing coupling.

## Verified entry points

| Address | Observed role | Source |
| --- | --- | --- |
| `0x00092413` | Call wrapper for the `ModifyTrust(int)` script event | `generated/game/x86_recomp.14.cpp`, line 19943 |
| `0x00095B32` | VM entry: reads the amount and second parameter, negates the amount if that parameter is nonzero, calls the wrapper | `generated/game/x86_recomp.15.cpp`, line 3 |
| `0x000E9CE4` | Finds a record by identifier, processes its state change and applies its trust amount | `generated/game/x86_recomp.22.cpp`, line 5409 |
| `0x000E9B32` | Counts entries of a specified statistics type | `generated/game/x86_recomp.22.cpp` |
| `0x001BB69C` | Aggregates statistics, applies the delta and stores the result in history | `generated/game/x86_recomp.41.cpp`, line 28969 |
| `0x001BA097` | Recalculates the delta for the review preview | `generated/game/x86_recomp.41.cpp`, line 24760 |
| `0x001BDA0C` | Resolves `AInst_Trust`, `A_trust`, `logo_JBA`, `logo_NSA` | `generated/game/x86_recomp.42.cpp`, line 4556 |
| `0x001BDB21` | Positions the animation at the supplied value | `generated/game/x86_recomp.42.cpp`, line 4766 |
| `0x001BDC15` | Enables the NSA logo at 0 and the JBA logo at 100 | `generated/game/x86_recomp.42.cpp`, line 4995 |
| `0x000C357D` | Reads `TRUSTVALUE=` from options and writes byte `+0x360` | `generated/game/x86_recomp.18.cpp`, line 23857 |
| `0x0018121F` | Large menu dispatcher: includes trust display, initialization and restoration | `generated/game/x86_recomp.36.cpp`, line 7699 |

### Why `0x00092413` is indeed `ModifyTrust`

Initializer `0x0011BE6D` registers the UTF-16 string `ModifyTrust` at
`0x00522750` and stores its identifier at `0x0064E924`. The wrapper reads this
identifier, resolves the function through `0x000603DA`, then invokes
`vtable + 0x10` with the delta as its parameter.

**This wrapper is not the function body.** The body has since been found in the
decompressed `common.lin` stream, header `0x17f295`, code `0x17f2a6`: the update
adds the delta and clamps `Trust` between 0 and 100. See the new reference for
evidence and the limitations concerning untested side effects.

Two other names are registered: `GetEchelonTrustLevel` → identifier at
`0x0064E850`, wrapper `0x001429C3`; and `ConvertTrustValueToTrustLevel` →
identifier at `0x0064E7D4`. Trust-tier consumers must also be examined.

## Objectives: classification exists, but in coupled form

In `0x000E9CE4`, the container has an array at `+0xC0`, an element count at
`+0xC4`, and a stride of **0x58 bytes**. Identifier comparison goes through
`0x000D28B9` using `entry + 0x24`.

Observed fields in each entry:

| Offset | Observed use |
| --- | --- |
| `+0x24` | Identifier compared with the requested identifier |
| `+0x38` | Integer trust amount |
| `+0x40` | State, written from the supplied new state |
| `+0x54`, mask `0x02` | Reverses the amount's sign |

Applying the amount depends on conditions including a new state of 1, a nonzero
amount and a context flag. Without the sign flag, the amount is passed unchanged;
with it, the amount is negative. For a positive amount, these correspond to the
NSA and JBA directions respectively. Actual amounts are now inventoried in the
new reference. The gameplay meaning of resource states still needs dynamic
verification before the fields are given definitive names.

A per-objective rule should retain **mission + objective identifier + state
transition** before reducing the result to a delta. A hook on the final delta
alone no longer knows which objective produced it.

## General actions: five review groups

Mission review reads statistics history through `object + 0x710`.
`0x000E9B32` walks an array at `+0xA8`, with size at `+0xAC` and 0x14-byte
entries; the first byte is the type identifier.

The formula in `0x001BB69C` uses five groups:

| Statistics types | Coefficient on the object | Flags on the object referenced by `object + 0xF4` |
| --- | --- | --- |
| `2 + 5` | `+0x4EC` | `+0xC60` |
| `0 + 3` | `+0x4F0` | `+0xC64` |
| `7` | `+0x4F4` | `+0xC68` |
| `10` | `+0x4F8` | `+0xC6C` |
| `6` | `+0x4FC` | `+0xC70` |

For each group, mask `0x01` means its contribution is enabled; mask `0x02`
adds it to the positive accumulator; otherwise it is added to the accumulator
that will be subtracted. Each accumulator is independently clamped between 0 and 15.

The observed calculation is equivalent to:

```text
positive = clamp(sum of positive groups, 0, 15)
negative = clamp(sum of negative groups, 0, 15)
target = clamp(current_trust + positive - negative, 5, 95)
ModifyTrust(target - current_trust)
```

The **5..95 bounds belong to this review path**; they do not establish the
general bounds of `ModifyTrust`. The menu dispatcher also has a call using
`100 - current_value`, and the HUD handles the 0 and 100 endpoints.

Preview function `0x001BA097` duplicates this formula using a starting value
stored at menu `+0xB0`; it writes the delta at `menu + 0xA0`. Both paths need
adapting to keep the review consistent.

Categories have since been cross-checked against the review and its text:
0/3 = enemy/civilian knockouts, 2/5 = enemy/civilian deaths, 7 = detections,
10 = alarms, 6 = bodies found. The new reference distinguishes this identification
from mission-specific coefficients, which remain to be extracted.

## Display and persistence

The HUD goes through `0x0019B7A1`, then `0x001BDB21` and `0x001BDC15`.
OPSAT uses `P_Trust`; mission review uses `P_TrustReview` and `Lst_TrustOverall`.
A second display must cover these screens and their notifications, not merely
draw two HUD bars.

Mission review stores the value in a byte history at
`profile_base + mission_index + 0x1C4B`. Console/map-travel path `0x001CEB87`
also reads and writes that area and passes `?TRUSTVALUE=%d`. Dispatcher
`0x0018121F` restores values from this history and explicitly initializes trust
to **75** in one path. This does not prove that every new game starts at 75.

Saving, checkpoint loading, map travel, mission restart and history all need
handling. Two host globals are insufficient: they can survive a reload that
should roll back their values. Do not reuse bytes adjacent to `+0x360` without
establishing their ownership.

## Proposed architecture — implement after identifying events

Each event has two explicit effects, for example
`{ delta_nsa: +5, delta_jba: 0 }`, with each meter independently clamped.
Cases `(+5,+5)`, `(-5,-5)` and `(+5,-5)` remain possible when justified by a
gameplay rule. There is no automatic opposing effect.

Rule keys must distinguish:

- Objective: mission, identifier, state transition.
- Action: identified statistics type, relevant context, application time.
- Direct script call: function/caller and mission context.

An unknown rule should be logged as unclassified. The old delta's sign alone
cannot determine independent effects on both factions. Repeated triggers and
actions already counted in review must be distinguished to avoid duplicate
rewards or penalties.

Starting values, failure thresholds, narrative conditions and equipment rewards
also require explicit decisions. Strings `LOADOUT_JBA_TRUST` and
`LOADOUT_ECHELON_TRUST` are present in the XBE; their presence alone does not
establish unlock conditions.

## Remaining checks before a working mod

1. The UnrealScript body of `ModifyTrust` and the constant-objective catalogue
   are now identified; check side effects and dynamic calls.
2. Extract mission-specific statistics coefficients and connect narrative
   variants, particularly Hisham's fake death, to exact script conditions.
3. Trace an objective, a direct script call and each review group in-game:
   identifier, context, old state, new state, amount and trust writes.
4. Introduce both persistent states, rules and displays, then adapt threshold
   readers and mission reviews.
5. Verify NSA-only gains, JBA-only gains, effects on both factions, repeated
   events, review, checkpoints and mission transitions.

At this stage, the locations and native calculations above are established by
static reading. The game has not been launched and no two-meter behavior has
yet been installed or validated.
