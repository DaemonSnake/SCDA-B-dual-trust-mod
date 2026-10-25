# Iceland A — experimental ×2 pack snapshot

Saved state: **373 resources**, including **38 recreated images** and **335 technically interpolated ×2 variants**. Aspect ratios are preserved. Mipmaps and metadata are rebuilt. Normal maps are interpolated and then renormalized; original alpha masks are preserved at the old levels. This full pack has not yet been validated in-game. Technical variants are not AI creations.

## Apply

Close SCDA, then open PowerShell in this folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Status
powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Apply
```

The default game path is `I:\game\xbox\SCDA-XBERecomp-2`. For another location, add `-GameRoot 'D:\path\to\game'`.

The script verifies the pack's SHA-256, requires SCDA to be closed, backs up the current LIN with a unique `.save` suffix, then replaces the file and verifies the result. It modifies only `assets\Default\DataXb\LMaps\x01_Iceland_a\x01_Iceland_a.lin`. It does not launch the game. No external BIN is needed for this level. It does not modify player saves, other levels or the executable.

If a copy of `x01_iceland_a.lin` exists in the `out\build\release\hdd` cache, the script stops and reports its path: its format must be checked before any action. Do not delete player saves to clear the cache. First test with a fresh launch of the Iceland A level; old-save behavior is not validated for this pack.

## Restore

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Restore
```

Restores the backup created by the last `Apply` recorded in `installation-history.jsonl`. The state replaced during restoration is also backed up. To select a specific backup:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Restore -Backup 'I:\path\file.lin.save'
```

## Preserved contents

- `staging/`: ready-to-install LIN and detailed manifest (hashes, dimensions, mips and provenance of the 373 resources).
- `originals/`: original PNG, DDS, mip and raw-data extractions.
- `alternatives/`: generated masters, images fitted to exact dimensions and technical variants.
- `drafts-v1/`, `quality-notes.json`, `generation-guidance.md`: trials and quality decisions, including rejections.
- `inventory.json`, `generation-queue.json`, `project-status.json`, `material-batch-*.json`: inventory and progress. Some provenance paths describe the original working location; installation uses only relative paths under `staging/`.
- `snapshot-tools/`: archived preparation/rebuilding scripts and local dependencies for resuming work. They retain their original working paths; **Apply-Pack.ps1 requires neither Python nor these scripts**.
- `source-backups/` in the project copy: stock LIN and the LIN present when this snapshot was made, with hashes.
- `SHA256SUMS.json` in the project copy: archived-file integrity checks.

## Validation limits

Offline checks pass: identity of all 373 exports by exact size, dimensions, formats, alpha, permitted ranges, preservation of other bytes and compression round trip. The full installation, including UI, BC2/BC3 and shared resources, still needs in-game testing. A successful script result confirms the pack was copied, not that it renders correctly.
