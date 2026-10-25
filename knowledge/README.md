# SCDA Xbox texture replacement knowledge

Additional research: [NSA/JBA trust reference](TRUST_REFERENCE.md), covering
triggers, narrative context, original values and proposals for two independent
meters.

Local findings verified on **22 October 2026**, using this SCDA-XBERecomp-2 tree and the user's Xbox assets.

**Verified:** regenerated `x_concrete_b_d`, 256 → **1024 × 1024 BC1/DXT1**, with a secondary 512 × 512 chain. Initial loading and restoring the existing checkpoint both work, confirmed visually and by GPU data captures.

## Read in this order

1. [Practical procedure](TEXTURE_REPLACEMENT_PROCEDURE.md)
2. [Binary formats and calculations](TEXTURE_FORMATS.md)
3. [Failures, causes, and evidence](ISSUES_AND_EVIDENCE.md)
4. [Exact verified case and source locations](VERIFIED_CONCRETE_CASE.md)
5. [Requirements for a general tool](AUTOMATION_REQUIREMENTS.md)

## Short procedure

1. Close SCDA. Back up every affected BIN, LIN, and checkpoint; retain original `.save` files and hashes.
2. Identify the texture's qualified name, image, format, dimensions, mip policy, and all stored variants.
3. Test a replacement at its **original size and format** first.
4. Encode new mip chains. Append them before the BIN index; preserve all other textures' offsets.
5. Update the target BIN entry and its LIN copy; update LIN dimensions, clamps, mip count, and allocation reservations.
6. Inspect old checkpoints: they can contain another copy of the texture index. Synchronize matching entries and verify save integrity.
7. Validate staged files and install the coherent set with the game closed.
8. Test initial loading **and checkpoint restoration**, checking the actual GPU dimensions and bytes.
9. Record the result; disable diagnostic logging when finished.

## Scope of “arbitrary size”

One size-changing case is proven. This is not yet a universal importer or a one-command tool. Square power-of-two opaque BC1 is our established starting point. Rectangles, aspect-ratio changes, downsizing, alpha, other formats, cubemaps, and larger memory use still need validation.

The ordinary Xbox header path used here encodes power-of-two dimensions. Arbitrary dimensions such as 1000 × 750 are **not supported by this documented recipe**. Resample to a validated size or implement/test the necessary additional loader/header behavior.

This knowledge folder contains documentation, small reports/traces, and clearly labeled case-specific source snapshots. It does not contain original game assets or player-save contents. The installed working HD texture is unchanged by this documentation task.

- [Independent trust implementation and verification](TRUST_IMPLEMENTATION.md)
