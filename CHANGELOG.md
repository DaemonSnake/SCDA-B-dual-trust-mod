# Changelog

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
