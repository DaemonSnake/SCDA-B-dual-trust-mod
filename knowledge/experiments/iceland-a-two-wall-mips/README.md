# Targeted mipmap test for two walls

Base: v2 control pack, SHA256 `044708acea5e5c7a00dbc72df267f43c39167253d92de592278adf31d3bd12d2`.

Only the pixel data of 11 old mipmaps is regenerated from the ×2 candidates: `x_factorywall_ex_a_d` (sheet-metal/brick facade) and `entrence_arch_d` (concrete frame). Visual identification is based on atlas layout; the GPU binding was not captured.

Offline evidence: identical decompressed size; restoring only these ranges reproduces the v2 base exactly. Metadata, the ×2 top mip, UI, normal maps, specular masks and every other texture are identical to v2. BC1 decoding and alpha were checked. `previews/` contains the re-encoded mips.

After closing the game: `powershell -NoProfile -ExecutionPolicy Bypass -File .\Apply-Pack.ps1 -Action Apply`. The script creates a new `.save` backup. `-Action Restore` restores the version preceding this test.

Test the small building's facade and the concrete door frame, near and far, then after a reload. Record whether black patterns return and at what distance. In-game results remain unconfirmed. If the defect returns, changes are limited to these two texture chains; testing each separately can then identify which one is involved. If it does not return, this does not prove that every other texture or format supports the same operation.
