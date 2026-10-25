# Iceland A — v2 control after HUD/wall regressions

Testing the full pack showed a distorted HUD and corrupted walls. The exact cause of the wall corruption is not yet established. This version isolates changes; it is not an in-game-validated fix.

- 144 DXT1 textures in the Iceland A scenery package are enlarged to ×2: 38 recreated images and 106 technical variants.
- 229 resources are retained entirely from stock: UI, shared resources and BC2/BC3 formats.
- Every old mipmap record (pixels and headers) remains identical to stock. Only a new top level is added to the 144 selected textures.
- Dimensions, mip counts and reservations are adjusted only for these textures.

Checks: bytes of the 229 excluded records unchanged; all old mips unchanged; only permitted fields and insertions change; exact zlib round trip.

Close SCDA, then run from this folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Apply
```

The installed LIN is automatically backed up before replacement. `-Action Restore` restores that state (which may be the v1 pack exhibiting regressions). To recover the last validated 53-texture pack before v1, use `-Action Restore -Backup` with the level folder's `x01_Iceland_a.lin.remaster-apply-20261022-170458-507.save` backup; its expected SHA256 is `ac71f8590fd699a1f7c98c1d8464ed31db4ac02e0de1f5bcf046aef8bfb2a381`.

Test the level from the beginning, the HUD, both reported buildings and a transition. Recreated detail may disappear when the engine selects old mips: this is intentional in this control. Do not consider the walls fixed before this test.

The v1 snapshot remains intact at `knowledge/experiments/iceland-a-remaster-snapshot-20261022`.
