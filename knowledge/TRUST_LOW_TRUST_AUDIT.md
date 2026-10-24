# Low-trust consumers audit — 25 October 2026

## Implemented update for alpha.1 and alpha.2

The findings below describe the pre-fix alpha.2 source (`098fbbc`). The updated
release sources fix the confirmed shared-score consumers:

- Mission loadouts check each faction independently at **61 or above**. NSA only
  gives the NSA kit; JBA only gives the JBA kit; neither gives the default kit.
  Both eligible combine their existing configuration quantities by maximum per
  supply, rather than adding duplicate supplies. Equipment flags are combined;
  explicit restrictions take precedence. Elite/multiplayer choices are preserved.
- Computer recovery components check the corresponding faction at **40 or less**.
  Both low exposes both factions' components, neither low exposes neither.
  Unaffiliated and shared components retain their native behavior. Normal and
  secure access lists are both handled, including hiding and interaction refresh.
- Initial-map trust patterns use the same low-faction projection as event dispatch;
  healthy 75/75 enters through neutral instead of implying low JBA from high NSA.
- JBA 40 now projects to native scalar 61 so the warning boundary agrees with its
  independent Suspicious label. Zero still selects its native critical pattern.

The original single native crisis system still prioritizes the lower faction
(NSA on an equal-score tie); separate simultaneous timers were not introduced.
Ending conditions and exhaustive mission-content mapping remain outside this fix.
Alpha.1 retains its original award/statistics balance; alpha.2 retains integer
rebalancing, the -40 major-story rule and the -60 statistics loss cap.

Tests cover eligibility, exact thresholds, conservative gear flags, native script
loadout selection, native kit construction, both computer arrays/all faction tags,
show/hide argument decoding and healthy initial-map projection. Detailed results
are recorded in the implementation notes. Timeout retesting was excluded as
requested by the user.

## Original findings (before the fix)

Scope: original Xbox single-player image.
Evidence combines native recompilation, a private snapshot of an isolated running
Kinshasa B instance, reflected Unreal properties and resolved script bytecode.
Raw game memory/bytecode stays outside this repository.

## Status levels

The live controller's four boundaries are 0, 40, 60 and 100, in
`m_iEchelonTrust_Alarmed`, `_Suspicious`, `_Confident`, `_Trustworthy`.
`GetEchelonTrustLevel` and `ConvertTrustValueToTrustLevel` implement:

| Trust | Native tier | English label |
| --- | ---: | --- |
| 0 | 0 | Alarmed |
| 1–40 | 1 | Suspicious |
| 41–60 | 2 | Neutral |
| 61–99 | 3 | Confident |
| 100 | 4 | Trusting |

These are native status boundaries, distinct from the mod's recovery-award guard
(20 or less) and its crisis-projection selection (a score below 50).

## What trust affects

| Consumer | Original use | Current independent-score behavior |
| --- | --- | --- |
| OPSAT status labels | One NSA tier and its inverse for JBA | Adapted: each faction's own score feeds its status label |
| Gameplay meter, notifications, mission review | Shared trust scalar | Adapted to independent scores and deltas |
| Warning dialogue/pattern | Suspicious NSA or the opposite high-scalar JBA branch | Fed by `CrisisValue` during trust dispatch; still one selected faction at a time |
| Critical trust objective | Alarmed NSA or the opposite terminal JBA branch | Native patterns reached through the projection; NSA native-loss test observed the recovery objective/countdown |
| Recovery award | Restore the shared scalar in one direction | Adapted: +25 to the affected faction, only at <=20, with one-shot/rearm guards |
| Mission loadout choice | Choose NSA/JBA/default kit from one tier | **Legacy dependency remains:** uses the NSA mirror, not independent JBA eligibility |
| Computer faction entries | Show/hide faction-specific normal/secure access components | **Legacy dependency remains:** uses one NSA-derived tier to select faction components |
| Initial mission trust pattern | Choose initial NSA/JBA pattern from one tier | Still calls the native getter outside the OPSAT adapter; independent map-entry behavior needs follow-up |
| Travel and old native profile/review history | Store/pass one scalar | Native scalar remains an NSA mirror; sidecars preserve both real scores and ledger |

### Warnings and critical objectives

`EPlayerController.ManageTrustLevelEvents` selects a pattern for each native tier.
The live Kinshasa B LevelInfo resolves these exact pairs:

| Native tier | Pattern | Label |
| ---: | --- | --- |
| 0 | P_trustgameplay | TrustGameplayNSA |
| 1 | P_warningtrust | WarningNSA |
| 2 | P_trustgameplay | NeutralReInit |
| 3 | P_warningtrust | WarningJBA |
| 4 | P_trustgameplay | TrustGameplayJBA |

`CrisisValue` projects the lower faction if either is below 50: low NSA uses
NSA; low JBA uses `100 - JBA`. Outside dispatch the original controller byte is
restored to NSA. A high faction alone therefore does not trigger a new crisis
through this adapter. The projection preserves native pattern/timer behavior.
At zero, the native critical pattern supplies an objective and countdown;
immediate game over at zero is not what the observed native flow did.
Timeout-to-game-over testing was explicitly excluded at the user's request.

There are two limitations to this scalar projection:

- Both-low scores select only one faction (NSA wins an equal-score tie). This
  audit does not establish simultaneous independent warning/crisis lifecycles.
- Native boundary inversion is asymmetric: NSA 40 projects tier 1, whereas JBA 40
  projects scalar 60/tier 2. JBA 1–39 projects the tier-3 warning branch; JBA 0
  projects tier 4. Thus the OPSAT label and warning boundary differ at JBA 40.

Pattern-specific reminders, recovery interaction and failure scripts exist in
mission data. Their exact timing/variation has not been exhaustively replayed.
No evidence here establishes a global penalty to aiming, damage, stealth or NPC
perception from low trust.

### Loadouts: confirmed remaining shared-score dependency

`EMissionManager.PreFillInventory` calls `SetLoadupType`. Its solo, non-elite
branch invokes `GetEchelonTrustLevel` and chooses:

| Native NSA tier | Selected loadout type |
| --- | --- |
| >=3 (NSA 61–100) | 9, ELOADOUTTYPE_SC4_ECHELON |
| <=1 (NSA 0–40) | 10, ELOADOUTTYPE_SC4_JBA |
| 2 (NSA 41–60) | 7, ELOADOUTTYPE_SC4_DEFAULT |

Elite difficulty and multiplayer have preceding separate branches. The current
getter hook substitutes JBA only for the second read inside the OPSAT function;
this mission-manager caller therefore uses NSA. Consequences of the code path:

- NSA 75 / JBA 0 still selects the NSA loadout.
- NSA 10 / JBA 10 selects the JBA loadout despite low JBA trust.
- NSA 100 / JBA 100 selects the NSA loadout, not a union of both kits.

These examples are static evaluations of the resolved branch, not newly played
mission-start inventory tests. The native loadout constructor `0x000EA5A7`
selects `LOADOUT_ECHELON_TRUST` and `LOADOUT_JBA_TRUST` configuration sections
for types 9/10. Actual item quantities depend on the selected configuration;
this audit does not claim a new universal list of reward gadgets.

### Computers: confirmed remaining shared-score dependency

Native `0x00142BAE` is reflected as
`EComputerSystem.DisplayFactionComponents` (native index 0x675).
It calls the same getter at return address `0x00142CB9`, then processes the
`NormalAccess` (+0x428) and `SecureAccess` (+0x434) component arrays.
When displaying faction components it selects:

- tier <2: faction 1, eCMPFaction_Echelon;
- tier >2: faction 2, eCMPFaction_JBA;
- tier 2: faction 3, eCMPFaction_Both.

Components with faction None (0) or Both (3) also pass the faction filter.
These enum values were verified from the live reflected `ECMPFaction` enum;
the faction field is `ComputerAccessComponent.Faction` at +0x2D.
The routine changes the component visibility flag (bit 8), and refreshes the
interaction if the player is using that computer/EEV. This is a faction-specific
computer-content gate, not evidence of a general change in hacking difficulty.
It still assumes the original opposing relationship and has no JBA score read.
The exact mission-specific emails/objectives behind each component still need
individual content mapping.

## Evidence and bounds

- Reflected live controller properties at +0x4DC/+0x4E0/+0x4E4/+0x4E8;
  original trust byte +0x360.
- Native getter wrapper `0x001429C3`; OPSAT caller `0x0019890F` has the mod's
  special second-read adapter. Other callers keep NSA.
- Native computer filter `0x00142BAE`, getter call `0x00142CB4`.
- Resolved script functions `EMissionManager.SetLoadupType`,
  `EPlayerController.StartInitialPattern`, `ManageTrustLevelEvents`,
  `GetEchelonTrustLevel`, `ConvertTrustValueToTrustLevel`.
- A scan of 5,111 script-bearing live function objects identified getter calls
  in StartInitialPattern, SetTrustLevel, ModifyTrust and SetLoadupType.
  SetTrustLevel/ModifyTrust are trust writers/dispatchers; the first and last
  are additional consumers. This loaded-object scan does not cover every
  unloaded map, state bytecode, dynamic dispatch or native scalar reader.
- UI/native review/travel/developer-menu scalar readers were separated from
  gameplay gates; matching an arbitrary structure offset +0x360 alone does
  not prove that structure contains the player's trust.
- No direct trust-score ending condition was proven. Ending dependencies must
  not be asserted absent without auditing final-mission scripts and globals.
  Narrative objective decisions can affect endings independently of trust.

## Follow-up implementation targets

1. Define independent faction eligibility for mission loadout selection.
2. Adapt faction computer components to the intended independent objective rules.
3. Audit initial-map pattern dispatch and both-low score handling.
4. Map remaining mission-state/global dependencies before claiming exhaustive
   campaign/ending coverage.
