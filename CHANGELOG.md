# Changelog

## v0.1.0-alpha.1 — updated 2026-10-25

- Fix faction kit selection: each faction qualifies at61+ using its own score;
  both eligible kits combine maximum supply counts and preserve restrictions.
- Fix normal/secure computer recovery entries to use each faction at<=40,
  including both-low and neither-low states.
- Project independent trust for initial-map patterns; align JBA40 warnings.
- Preserve original alpha.1 reward/loss amounts and statistics caps (+15/-15).
  No alpha.2 rebalancing is included in this release.
- Native timers and existing saved scores remain unchanged. Download this
  updated alpha.1 ZIP again if you obtained the earlier build.

Native fixtures and pure C++ tests cover faction selection, kit quantities,
component show/hide filtering and healthy initial-map projection. See
knowledge/TRUST_LOW_TRUST_AUDIT.md for implementation scope and limitations.
