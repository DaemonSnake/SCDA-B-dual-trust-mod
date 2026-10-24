# Changelog

## v0.1.0-alpha.2 — 2026-10-25

### Rebalancing

- Reduce original -30 story penalties from -60 to **-40**, affecting cruise-bomb
  sabotage discovery, allowing the cruise bombing and keeping the final bombs.
  Opposing +30 rewards remain unchanged. Other story penalties remain doubled;
  the shared statistics loss cap remains -60. Applies only to new awards, without
  rewriting saved scores or previous effective credits.

- Halve gains on objectives with no baseline loss to either faction, rounding down
  to whole points: +5/0 becomes +2/0; shared +5/+5 becomes +2/+2.
- Double new trust penalties while retaining positive rewards on trade-off
  choices: -5/+5 becomes -10/+5. Classify deferred effects before postponement;
  Cozumel sabotage gives NSA+30 immediately and JBA-40 at mission exit.
- Halve accumulated enemy-KO gains, rounded down, with a positive cap of+7.
- Double per-action statistics losses: enemy kill-4, civilian kill-10, civilian
  KO-2, detection-2, alarm-4 and discovered body-2 (NSA only, when enabled).
- Set the shared statistics loss cap to **-60 NSA**. This is up from-15 in
  alpha.1 and from-30 in the intermediate unreleased balance test. All negative
  statistic categories share this cap; story penalties remain separate.
- Keep75/75 starting scores and emergency+25 recoveries. Existing save scores
  and credited actions are not retroactively rescaled.

### Interface and documentation

- Fix mission-review rows inheriting a coincident gameplay pop-up's clock.
- Anchor signed deltas at the exact new segmented fill edge, remove the opaque
  backplate and retract red loss tails toward that same edge.
- Refresh the in-game animation and settled review captures.
- Consolidate all45 active story values, exceptions, statistics, UI and save
  decisions in the canonical trust reference. Add a README rebalancing section.

### Packaging and testing

- Clean Windows x64 build from the tagged source, with standalone compiler tools,
  installation instructions, changelog, build manifest and SHA-256 checksums.
- Trust-model regression tests cover integer rounding, saturation, reversals,
  duplicate prevention and the-60 cap. Native fixtures verified shared rewards,
  opposing phone choices, statistics, Hisham evidence and emergency recovery.
- Still an alpha for campaign testing: crises/equipment/all endings, exact sabotage
  discovery timing, protected-police classification and multiplayer need further QA.

## v0.1.0-alpha.1 — 2026-10-24

First playable Windows x64 prerelease for campaign testing, based on source
commit `87e12fa` of this fork and upstream SCDA-XBERecomp.

- Separate 0–100 NSA and JBA trust values, inspired by version A and adapted to
  the original Xbox version B campaign.
- Independent story-objective effects, NSA mission statistics, effective-credit
  reversal for opposite choices and checkpoint sidecars.
- Metallic faction meters in OPSAT and mission review; temporary affected-faction
  notifications with animated fill/loss tails and deltas aligned to the new value.
- Windows package with host executables, compiler tools, required runtimes,
  installation instructions and SHA-256 checksums. Game files and generated
  game-code DLLs are excluded.

Campaign crises, equipment rewards, all endings and multiplayer regression
testing remain incomplete. Bomb-sabotage discovery uses mission exit. Protected
police in generic enemy statistics and scalar profile history remain limitations.
See [implementation notes](knowledge/TRUST_IMPLEMENTATION.md) for evidence.
