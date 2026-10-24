# Independent NSA/JBA trust — implementation and verification

Version 0.3, 24 October 2026. Design values remain in [TRUST_REFERENCE.md](TRUST_REFERENCE.md)
and [TRUST_RULES_REFERENCE.json](TRUST_RULES_REFERENCE.json). This document records
what the native implementation actually does and what has been tested.

## Implementation

- Two independent values, each clamped to 0–100. New state starts at 75/75.
- All 45 nonzero story rules are generated from the reference JSON by
  `python tools/analysis/generate_trust_rules.py`. The 37 originally neutral
  objectives remain neutral, recovery objectives share a per-camp recovery guard,
  and TEMP debug objectives do not affect trust.
- Native `SetGoalStatus` identifies the completed objective; native `ModifyTrust`
  applies its independent rule while preserving the game's objective status,
  notification and script bookkeeping. Completed-event credits survive reloads.
- Exclusive outcomes replace the earlier *effective* contribution. A credit
  clamped at 100 cannot manufacture points when reversed. The train's old scalar
  compensation is deliberately ignored, because the ledger already reverses it.
- Cozumel and HQ bomb sabotage gives its NSA reward immediately; JBA loss is
  deferred until leaving the mission. Hisham rescue gains NSA trust; JBA loses
  15 only on the explicit script-selected dialogue where Emile sees him flee.
  The successful fake-death branch cancels the body objective, so its absence
  must not imply discovery. Exposure revokes the effective proof bonus first.
  Explicit accepted/exposed evidence also reverses the old inferred penalty
  when reading an earlier modded checkpoint.
- Native mission-statistics application is redirected to NSA-only gains/losses:
  enemy death −2, civilian death −5, enemy knockout +1, civilian knockout −1,
  seen −1, alarm −2, body found −1. Existing mission activation flags remain
  authoritative; positive and negative totals are capped at 15. Scripted named
  deaths are subtracted from the generic death count. Final NY deaths are exempt;
  the other enabled statistics still apply.
- A high score in one camp does not imply a crisis in the other. The adapter
  projects only the lower, unhealthy camp into the original warning levels and
  preserves the previously dispatched value, so repeated completions do not
  repeatedly start the same warning. Existing native/script thresholds and timers
  still execute; no new kill timer is introduced. The guest Trust byte mirrors NSA
  outside that dispatch for existing readers.

## Display

The original `AInst_Trust` / `A_Trust` / `A_trust` gauge area is hidden.
The selected faction-row design uses olive metallic frames, circular NSA/JBA
badges, ivory segmented tracks, slanted labels and numeric values. Twenty cells
represent five points each; a partial cell preserves the exact score. Both rows
use the same track length, with fixed room for a signed change. Low trust uses
a muted red track. The native OPSAT shell and localized legend remain intact.

Gameplay has no constant trust panel. Five-second pop-ups show only affected
factions, stacking both when both change. Reversal and replacement contributions
within 250 ms combine into one net notification. Duplicate/zero-effect events do
not trigger a notification. Hide-HUD, console and settings visibility are respected.
OPSAT retains both rows; mission review shows effective changes beside each
faction rather than a separate footer. No retail assets are modified.

The overlay is rendered by the host, so no retail UI textures or LIN assets are
modified. The HUD, real OPSAT trust page and real mission-review page have been
captured. The larger OPSAT panel covers the old logos and status region; the
review panel covers its old gauge while preserving the native rule table.
Review rows show compact NSA contributions, with enemy/civilian kill and knockout
labels corrected. The forecast uses a copy of the independent state and respects
0–100 saturation; each faction row shows its effective change. After application,
the page reuses the ledger credit instead of forecasting the reward a second time.
Both native OPSAT trust-tier reads use their own camp, including compensation
for the original caller's inversion of its second result.

## Persistence and compatibility

Retail checkpoint bytes keep their original format. Additional metadata lives in
`<SCDA.exe folder>/trust-state/<checkpoint filename>/<SHA1(relative checkpoint path)>/<SHA1(checkpoint bytes)>.txt`.
This separates profiles/slots and stays outside Xbox cache cleanup. A sidecar
contains both scores, mission, effective event ledger and the last warning value.
Writes replace a complete temporary file atomically. Keep this folder when moving
or backing up a modded game installation.

A checkpoint without metadata imports its original scalar as NSA=x, JBA=100−x;
its previous independent history cannot be reconstructed. A versioned per-instance
handover file carries state across the recompilation project's process relaunches.
Cold starts ignore that handover. Co-op and versus keep their original behavior.

## Evidence

| Check | Result |
|---|---|
| Pure C++20 state tests | Pass: independence, both-camp gain, duplicate suppression, clamped reversal, checkpoint copy, crisis projection, mission keys |
| Release `SCDA` build | Pass |
| Live HUD, prison and Cozumel | Two independent bars; original shared gauge absent |
| JBA-only reward | 75/75 → 75/85; NSA unchanged |
| Real native setter, synthetic PassengerLog record | 75/75 → 80/80; repeat stays 80/80 |
| Real native setter, synthetic ShipEngine record | 80/80 → 85/80 |
| Real registered Cozumel PlaceRelay objective | 85/80 → 85/90; record status becomes 1; repeat stays 85/90 |
| Real registered ChipCellPhone objective | 85/90 → 100/90; covert JBA effect deferred |
| Real registered opposing ProCellPhone objective | 100/90 → 55/100; prior saturated chip contribution reversed |
| AUTO reload, final cache-independent sidecar location | Deliberate 22/91 restored to saved 80/90 with ledger; manifest duplicate remained suppressed |
| Low NSA and recovery | 10/90 → 35/90 through native recovery call; JBA unchanged |
| Shared host changes, versus executable build | Pass |
| Native OPSAT tiers | NSA/JBA 100/100 → 4/4; 100/10 → 4/1; 10/100 → 1/4 |
| Real OPSAT and mission-review layout | Two bars captured; review rule table retained |
| Native statistics preview and application | Fixture: civilian death, enemy KO, detection, alarm; 80/90 → 73/90; repeat application and preview remain 73/90 |
| Hisham native goal + native localization fixtures | 75/75 → rescue 90/75 → proof 90/80 → witnessed escape 90/60; repeated exposure, later acceptance and proof remain 90/60 |
| Pure C++20 expanded tests | Pass: statistics masks/caps, Hisham proof/exposure and migration from inferred penalty |

Screenshots, script dumps and build logs are private in
`C:/Users/bastien/scda-trust-analysis/`. The opt-in `SCDA_TRUST_TEST` environment
variable enables a disk command bridge for repeatable native calls; normal launches
do not read that bridge. Developer console commands: `trust status`, `trust set N J`,
`trust event ID` (host rule only). These are diagnostics, not campaign events.

## Remaining campaign verification

This is a working native implementation, not a complete playthrough certification.
The following require normal campaign play before treating the balance as final:

- Hisham's full normal story sequence: the exact selected localization branch is
  connected and tested with native fixtures, but was not played end to end.
  Bomb sabotage still uses mission exit as the discovery fallback.
- All crisis/recovery script branches, timeouts and equipment rewards; the original
  game has several direct readers of its scalar Trust byte.
- Protected police classification inside the generic kill records: the present
  split uses the game's enemy/civilian record type and cannot yet identify a
  protected officer represented as an enemy record.
- Native profile mission history still stores legacy scalar fields; the current
  independent values and event history live in host state and checkpoint sidecars.
  The current mission-review forecast is independent; historical profile charts
  have not been converted to two histories.
- Full cross-map menu loading, co-op gameplay and all endings. A raw developer
  `LOADGAME` from the title screen crashed in guest deserialization during testing;
  use normal menu loading for cross-map saves, not that experimental command.

Pre-modification source, executable, DLL and HDD backup:
`C:/Users/bastien/scda-trust-analysis/backups/20261023-232006`.

## State left for the user

All 26 pre-test save files were restored
and checked against the backup with SHA-256; current test saves and host sidecars
were archived outside the project in `C:/Users/bastien/scda-trust-analysis/test-run-final/`.
The final HUD capture is `C:/Users/bastien/scda-trust-analysis/trust-hud-final.png`.
Sidecar version 3 additionally preserves pending initialization and legacy import
across a title-screen load/relaunch; older versions remain readable.

The 24 October follow-up used `out/build/release/trust-lab`, `--instance 2`
and its own HDD. It did not stop the user's main instance or modify its saves.
`SCDA_TEST_OUTPUT_DIRECTORY` optionally redirects the lab executable/DLL; the
normal build uses its default output. The DLL explicitly depends on the host
executable for relinking, avoiding a stale DLL timestamp and runtime rebuild dialog.
Only the lab enables `SCDA_TRUST_TEST` and `SCDA_TRUST_AUTOTEST`; the latter
bypasses loading key prompts without sending keyboard input to the desktop.

Campaign testing display: the compact gameplay panel is hidden unless a five-second trust-change notification is active. OPSAT and mission review retain their two bars.

Animated change presentation: signed deltas are centered inside each affected
track. The fill eases from its previous value to the final score over1.25s after
a0.15s hold. Changed segments briefly pulse (muted red for loss, ivory-green for
gain), then settle. Delta text drifts upward3px and fades during the last0.65s of
the five-second notification. Static review deltas remain inside their tracks.
This animation changes presentation only; the trust model applies immediately.
Live native fixtures verified JBA75->60 (-15) and NSA75->90 (+15), capturing
changing fill/highlight frames in the isolated lab.
Delta labels are anchored to the midpoint of the gained/lost value range, clamped within the track at its edges, rather than centered across the entire meter.
Final delta placement: immediately after max(previous,current), clamped to remain inside the track when the score is near100.
