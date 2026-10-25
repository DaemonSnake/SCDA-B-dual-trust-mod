# Appendix — technical trust inventory

Offsets are hexadecimal positions in the decompressed LIN stream, not compressed-file offsets or memory addresses.
Generated from local evidence collected on 23 October 2026. The reference document is [TRUST_REFERENCE.md](TRUST_REFERENCE.md).

## The 139 solo declarations

| Map containing the code | Offset | Context section | ID | Original delta (+NSA / −JBA) |
| --- | --- | --- | --- | ---: |
| `x01_Iceland_a` | `0x196932` | `p_x01_iceland_a_objectives` | `OBJ_01_P_01` | +0 |
| `x01_Iceland_a` | `0x196afd` | `p_x01_iceland_a_objectives` | `Ice_A_BackT` | +0 |
| `x01_Iceland_a` | `0x196c57` | `p_x01_iceland_a_objectives` | `knocknpcs` | +0 |
| `x01_iceland_b` | `0x3833a7` | `p_x01_iceland_b_objectives` | `OBJ_01_P_20` | +0 |
| `x01_iceland_b` | `0x383483` | `p_x01_iceland_b_objectives` | `OBJ_01_O_10` | +0 |
| `x01_iceland_b` | `0x383874` | `p_x01_iceland_b_objectives` | `OBJ_01_O_20` | +0 |
| `x01_iceland_b` | `0x38b0a4` | `p_x01_iceland_b_comoverride` | `OBJ_01_P_30` | +0 |
| `x02_prison_a` | `0x32ab00` | `p_x02_prison_a_objectives` | `WalkieTalkie` | +0 |
| `x02_prison_a` | `0x32ac33` | `p_x02_prison_a_objectives` | `Escape` | +0 |
| `x02_prison_a` | `0x32acbb` | `p_x02_prison_a_objectives` | `BarnhamAlive` | +15 |
| `x02_prison_a` | `0x32ad33` | `p_x02_prison_a_objectives` | `Retrieve` | +5 |
| `x02_prison_a` | `0x32b1a7` | `p_x02_prison_a_objectives` | `NeedSeringe` | -10 |
| `x02_prison_a` | `0x32ed52` | `p_x02_prison_a_visitationguard` | `BarnHamDead` | -15 |
| `x02_prison_b` | `0x3315a3` | `p_x02_prison_b_objectives` | `Regroup` | -10 |
| `x02_prison_b` | `0x331893` | `p_x02_prison_b_objectives` | `Disc` | -5 |
| `x02_prison_b` | `0x331c5a` | `p_x02_prison_b_objectives` | `Chamber` | +0 |
| `x04_moneytrain_a` | `0x357102` | `p_x04_moneytrain_a_objectives` | `P_WoltCode` | -10 |
| `x04_moneytrain_a` | `0x357421` | `p_x04_moneytrain_a_objectives` | `P_GoFrontTrain` | +0 |
| `x04_moneytrain_a` | `0x357694` | `p_x04_moneytrain_a_objectives` | `P_GoFrontTrain` | +0 |
| `x04_moneytrain_a` | `0x3578a8` | `p_x04_moneytrain_a_objectives` | `P_KillChief` | -15 |
| `x04_moneytrain_a` | `0x357927` | `p_x04_moneytrain_a_objectives` | `P_ChiefAlive` | +15 |
| `x04_moneytrain_a` | `0x357f50` | `p_x04_moneytrain_a_objectives` | `P_ChiefAlive` | +15 |
| `x04_moneytrain_a` | `0x357fd0` | `p_x04_moneytrain_a_objectives` | `P_KillChief` | -15 |
| `x04_moneytrain_a` | `0x36437e` | `p_x04_moneytrain_a_trustgameplay` | `P_TrustJBA` | -25 |
| `x04_moneytrain_a` | `0x36470d` | `p_x04_moneytrain_a_trustgameplay` | `P_TrustNSA` | +25 |
| `x04_moneytrain_b` | `0x259642` | `p_x04_moneytrain_b_objectives` | `O_DetachCars` | +5 |
| `x04_moneytrain_b` | `0x259809` | `p_x04_moneytrain_b_objectives` | `P_DisconnectCom` | -10 |
| `x04_moneytrain_b` | `0x260b16` | `p_x04_moneytrain_b_trustgameplay` | `P_TrustJBA` | -25 |
| `x04_moneytrain_b` | `0x260f5a` | `p_x04_moneytrain_b_trustgameplay` | `P_TrustNSA` | +25 |
| `x05_hq2_r` | `0x3e0b31` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_10` | +10 |
| `x05_hq2_r` | `0x3e0bea` | `p_x05_hq2_r_objectives` | `CarsonMoss` | -5 |
| `x05_hq2_r` | `0x3e0c71` | `p_x05_hq2_r_objectives` | `YeagerLambert` | +15 |
| `x05_hq2_r` | `0x3e0cf9` | `p_x05_hq2_r_objectives` | `YeagerEmile` | -15 |
| `x05_hq2_r` | `0x3e1389` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_20` | +10 |
| `x05_hq2_r` | `0x3e18d9` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_40` | +0 |
| `x05_hq2_r` | `0x3e5d37` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_10` | +10 |
| `x05_hq2_r` | `0x3e5f87` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_20` | +10 |
| `x05_hq2_r` | `0x3e602e` | `p_x11_hq5_r_objectives` | `P_LambertNOTCover` | -15 |
| `x05_hq2_r` | `0x3e60a4` | `p_x11_hq5_r_objectives` | `P_LambertCover` | +15 |
| `x05_hq2_r` | `0x3e6117` | `p_x11_hq5_r_objectives` | `FindBunker` | +0 |
| `x05_hq2_r` | `0x3e6452` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_30` | +15 |
| `x05_hq2_r` | `0x3e64c4` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_31` | +15 |
| `x05_hq2_r` | `0x3e6536` | `p_x11_hq5_r_objectives` | `KeepBombs` | -30 |
| `x05_hq2_r` | `0x3e6f01` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_40` | +0 |
| `x05_hq2_r` | `0x3f8841` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_10` | +0 |
| `x05_hq2_r` | `0x3f8a89` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_20` | +5 |
| `x05_hq2_r` | `0x3f8d4b` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_30` | +0 |
| `x05_hq2_r` | `0x3f8f8e` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_40` | +0 |
| `x05_hq2_r` | `0x3f91e6` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_50` | +0 |
| `x05_hq2_r` | `0x3fb2bd` | `p_x05_hq2_r_trustgameplay` | `TrustNow` | -25 |
| `x05_hq2_r` | `0x3fb696` | `p_x05_hq2_r_trustgameplay` | `TrustNow` | -25 |
| `x05_hq2_r` | `0x3fbd53` | `p_x05_hq2_r_trustgameplay` | `P_TrustNSA` | +25 |
| `x05_hq2_r` | `0x3fc193` | `p_x05_hq2_r_trustgameplay` | `P_TrustNSAHq2` | +25 |
| `x06_cozumel_a` | `0x2f0e0b` | `p_x06_cozumel_a_objectives` | `ChipCellPhone` | +30 |
| `x06_cozumel_a` | `0x2f0e86` | `p_x06_cozumel_a_objectives` | `ProCellPhone` | -30 |
| `x06_cozumel_a` | `0x2f0fab` | `p_x06_cozumel_a_objectives` | `PlaceRelay` | -10 |
| `x06_cozumel_a` | `0x2f10b8` | `p_x06_cozumel_a_objectives` | `PassengerLog` | +5 |
| `x06_cozumel_a` | `0x2f1296` | `p_x06_cozumel_a_objectives` | `GetDown` | +0 |
| `x06_cozumel_a` | `0x2f1456` | `p_x06_cozumel_a_objectives` | `GetBomb` | -10 |
| `x06_cozumel_a` | `0x2fd255` | `p_x06_cozumel_a_trustgameplay` | `P_TrustJBA` | -25 |
| `x06_cozumel_a` | `0x2fd5e8` | `p_x06_cozumel_a_trustgameplay` | `P_TrustNSA` | +25 |
| `x06_cozumel_a` | `0x2fd9bc` | `p_x06_cozumel_a_trustgameplay` | `TEMPNSA` | +20 |
| `x06_cozumel_a` | `0x2fda55` | `p_x06_cozumel_a_trustgameplay` | `TEMPJBA` | -70 |
| `x06_cozumel_b` | `0x26dd22` | `p_x06_cozumel_b_objectives` | `PlantBomb` | +0 |
| `x06_cozumel_b` | `0x26e00b` | `p_x06_cozumel_b_objectives` | `PierExtraction` | +0 |
| `x06_cozumel_b` | `0x26e369` | `p_x06_cozumel_b_objectives` | `CameraRecord` | +5 |
| `x06_cozumel_b` | `0x26e544` | `p_x06_cozumel_b_objectives` | `ShipEngine` | +5 |
| `x06_cozumel_b` | `0x276ead` | `p_x06_cozumel_b_trustgameplay` | `P_TrustJBA` | -25 |
| `x06_cozumel_b` | `0x277260` | `p_x06_cozumel_b_trustgameplay` | `P_TrustNSA` | +25 |
| `x06_cozumel_b` | `0x277632` | `p_x06_cozumel_b_trustgameplay` | `TEMPNSA` | +20 |
| `x06_cozumel_b` | `0x2776cb` | `p_x06_cozumel_b_trustgameplay` | `TEMPJBA` | -70 |
| `x08_okhotsk_a` | `0x277652` | `p_x08_okhotsk_a_objectives` | `Mercenaries` | -10 |
| `x08_okhotsk_a` | `0x27771d` | `p_x08_okhotsk_a_objectives` | `Communications` | -10 |
| `x08_okhotsk_a` | `0x2777eb` | `p_x08_okhotsk_a_objectives` | `Bombs` | -5 |
| `x08_okhotsk_a` | `0x27786a` | `p_x08_okhotsk_a_objectives` | `KeyCode` | +5 |
| `x08_okhotsk_a` | `0x277968` | `p_x08_okhotsk_a_objectives` | `Tanker` | +0 |
| `x08_okhotsk_a` | `0x2823da` | `p_x08_okhotsk_a_trustgameplay` | `P_TrustJBA` | -25 |
| `x08_okhotsk_a` | `0x282821` | `p_x08_okhotsk_a_trustgameplay` | `P_TrustNSA` | +25 |
| `x08_okhotsk_b` | `0x3ab432` | `p_x08_okhotsk_b_objectives` | `Freight_Done` | +5 |
| `x08_okhotsk_b` | `0x3ab518` | `p_x08_okhotsk_b_objectives` | `Tracker_Done` | +5 |
| `x08_okhotsk_b` | `0x3ab74b` | `p_x08_okhotsk_b_objectives` | `Captain_Extraction` | +0 |
| `x08_okhotsk_b` | `0x3abb46` | `p_x08_okhotsk_b_objectives` | `Bombs_Done` | -10 |
| `x08_okhotsk_b` | `0x3b9281` | `p_x08_okhotsk_b_trustgameplay` | `P_TrustJBA` | -25 |
| `x08_okhotsk_b` | `0x3b9756` | `p_x08_okhotsk_b_trustgameplay` | `P_TrustNSA` | +25 |
| `x08_okhotsk_b` | `0x3b9b36` | `p_x08_okhotsk_b_trustgameplay` | `TEMPNSA` | +20 |
| `x08_okhotsk_b` | `0x3b9bcf` | `p_x08_okhotsk_b_trustgameplay` | `TEMPJBA` | -70 |
| `x10_kinshasa_a` | `0x3d1da8` | `p_x10_kinshasa_a_objectives` | `OBJ_10_P_50` | -10 |
| `x10_kinshasa_a` | `0x3d1e24` | `p_x10_kinshasa_a_objectives` | `OBJ_10_O_10` | +5 |
| `x10_kinshasa_a` | `0x3d1ea0` | `p_x10_kinshasa_a_objectives` | `OBJ_10_O_20` | +5 |
| `x10_kinshasa_a` | `0x3d21b6` | `p_x10_kinshasa_a_objectives` | `OBJ_10_O_100` | +0 |
| `x10_kinshasa_a` | `0x3d26ca` | `p_x10_kinshasa_a_objectives` | `OBJ_10_P_200` | +0 |
| `x10_kinshasa_a` | `0x3d2a63` | `p_x10_kinshasa_a_objectives` | `OBJ_10_P_60` | -15 |
| `x10_kinshasa_a` | `0x3d2adf` | `p_x10_kinshasa_a_objectives` | `HishamBody` | -5 |
| `x10_kinshasa_a` | `0x3d2b90` | `p_x10_kinshasa_a_objectives` | `OBJ_10_P_30` | +15 |
| `x10_kinshasa_a` | `0x3df547` | `p_x10_kinshasa_a_trustgameplay` | `P_TrustJBA` | -25 |
| `x10_kinshasa_a` | `0x3df8e6` | `p_x10_kinshasa_a_trustgameplay` | `P_TrustNSA` | +25 |
| `x10_kinshasa_a` | `0x3dfcbb` | `p_x10_kinshasa_a_trustgameplay` | `TEMPNSA` | +20 |
| `x10_kinshasa_a` | `0x3dfd56` | `p_x10_kinshasa_a_trustgameplay` | `TEMPJBA` | -70 |
| `x10_kinshasa_b` | `0x2e0ffd` | `p_x10_kinshasa_b_objectives` | `OBJ_10_P_70` | -10 |
| `x10_kinshasa_b` | `0x2e11d7` | `p_x10_kinshasa_b_objectives` | `OBJ_10_P_500` | +0 |
| `x10_kinshasa_b` | `0x2ec7c3` | `p_x10_kinshasa_b_trustgameplay` | `P_TrustJBA` | -25 |
| `x10_kinshasa_b` | `0x2ecb62` | `p_x10_kinshasa_b_trustgameplay` | `P_TrustNSA` | +25 |
| `x10_kinshasa_b` | `0x2ecf3f` | `p_x10_kinshasa_b_trustgameplay` | `TEMPNSA` | +20 |
| `x10_kinshasa_b` | `0x2ecfda` | `p_x10_kinshasa_b_trustgameplay` | `TEMPJBA` | -70 |
| `x11_hq5_r` | `0x3fcea2` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_10` | +10 |
| `x11_hq5_r` | `0x3fcf5b` | `p_x05_hq2_r_objectives` | `CarsonMoss` | -5 |
| `x11_hq5_r` | `0x3fcfe2` | `p_x05_hq2_r_objectives` | `YeagerLambert` | +15 |
| `x11_hq5_r` | `0x3fd06a` | `p_x05_hq2_r_objectives` | `YeagerEmile` | -15 |
| `x11_hq5_r` | `0x3fd6fa` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_20` | +10 |
| `x11_hq5_r` | `0x3fdc4a` | `p_x05_hq2_r_objectives` | `OBJ_HQA_P_40` | +0 |
| `x11_hq5_r` | `0x4020a8` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_10` | +10 |
| `x11_hq5_r` | `0x4022f8` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_20` | +10 |
| `x11_hq5_r` | `0x40239f` | `p_x11_hq5_r_objectives` | `P_LambertNOTCover` | -15 |
| `x11_hq5_r` | `0x402415` | `p_x11_hq5_r_objectives` | `P_LambertCover` | +15 |
| `x11_hq5_r` | `0x402488` | `p_x11_hq5_r_objectives` | `FindBunker` | +0 |
| `x11_hq5_r` | `0x4027c3` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_30` | +15 |
| `x11_hq5_r` | `0x402835` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_31` | +15 |
| `x11_hq5_r` | `0x4028a7` | `p_x11_hq5_r_objectives` | `KeepBombs` | -30 |
| `x11_hq5_r` | `0x403272` | `p_x11_hq5_r_objectives` | `OBJ_HQB_P_40` | +0 |
| `x11_hq5_r` | `0x414bb2` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_10` | +0 |
| `x11_hq5_r` | `0x414dfa` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_20` | +5 |
| `x11_hq5_r` | `0x4150bc` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_30` | +0 |
| `x11_hq5_r` | `0x4152ff` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_40` | +0 |
| `x11_hq5_r` | `0x415557` | `p_x05_hq2_r_fingerprints` | `OBJ_HQA_O_50` | +0 |
| `x11_hq5_r` | `0x41762e` | `p_x05_hq2_r_trustgameplay` | `TrustNow` | -25 |
| `x11_hq5_r` | `0x417a07` | `p_x05_hq2_r_trustgameplay` | `TrustNow` | -25 |
| `x11_hq5_r` | `0x4180c4` | `p_x05_hq2_r_trustgameplay` | `P_TrustNSA` | +25 |
| `x11_hq5_r` | `0x418504` | `p_x05_hq2_r_trustgameplay` | `P_TrustNSAHq2` | +25 |
| `x12_ny_a` | `0x3d56ec` | `p_x12_ny_a_objectives` | `bomba` | +0 |
| `x12_ny_a` | `0x3d57a6` | `p_x12_ny_a_objectives` | `hikilla` | +0 |
| `x12_ny_a` | `0x3d5811` | `p_x12_ny_a_objectives` | `killalla` | +0 |
| `x12_ny_a` | `0x3da555` | `p_x12_ny_a_enricahelps` | `ReachB` | +0 |
| `x12_ny_b` | `0x302605` | `p_x12_ny_b_objectives` | `bombb` | +0 |
| `x12_ny_b` | `0x3026bf` | `p_x12_ny_b_objectives` | `hikillb` | +0 |
| `x12_ny_b` | `0x30272b` | `p_x12_ny_b_objectives` | `killallb` | +0 |
| `x12_ny_b` | `0x302aaf` | `p_x12_ny_b_objectives` | `ReachC` | +0 |
| `x12_ny_c` | `0x409de6` | `p_x12_ny_c_objectives` | `bombc` | +0 |
| `x12_ny_c` | `0x409ea0` | `p_x12_ny_c_objectives` | `hikillc` | +0 |
| `x12_ny_c` | `0x409f0d` | `p_x12_ny_c_objectives` | `killallc` | +0 |

## The 208 constant solo state changes

State 1 = completed, state 2 = canceled. A call's presence does not prove its branch is reachable. Cancellation alone does not trigger the trust amount.

| Map containing the code | Offset | ID | State |
| --- | --- | --- | ---: |
| `x01_Iceland_a` | `0x196c45` | `Ice_A_BackT` | 1 |
| `x01_Iceland_a` | `0x196dfa` | `knocknpcs` | 1 |
| `x01_Iceland_a` | `0x196f45` | `knocknpcs` | 2 |
| `x01_Iceland_a` | `0x1a62ac` | `knocknpcs` | 2 |
| `x01_iceland_b` | `0x383039` | `OBJ_01_P_01` | 1 |
| `x01_iceland_b` | `0x383140` | `Ice_A_BackT` | 2 |
| `x01_iceland_b` | `0x383203` | `knocknpcs` | 2 |
| `x01_iceland_b` | `0x38357c` | `OBJ_01_P_20` | 1 |
| `x01_iceland_b` | `0x383775` | `OBJ_01_P_30` | 1 |
| `x01_iceland_b` | `0x3837b9` | `OBJ_01_O_10` | 1 |
| `x01_iceland_b` | `0x383c27` | `OBJ_01_O_20` | 1 |
| `x02_prison_a` | `0x32af01` | `WalkieTalkie` | 1 |
| `x02_prison_a` | `0x32b2c1` | `NeedSeringe` | 1 |
| `x02_prison_a` | `0x334ce0` | `BarnHamDead` | 1 |
| `x02_prison_a` | `0x334cf2` | `BarnHamAlive` | 2 |
| `x02_prison_a` | `0x334dc5` | `BarnHamAlive` | 1 |
| `x02_prison_a` | `0x334dd8` | `BarnHamDead` | 2 |
| `x02_prison_b` | `0x3317ea` | `Retrieve` | 1 |
| `x02_prison_b` | `0x331a2f` | `Disc` | 1 |
| `x02_prison_b` | `0x331bdb` | `Regroup` | 1 |
| `x02_prison_b` | `0x331d26` | `Chamber` | 1 |
| `x02_prison_b` | `0x331d3a` | `Escape` | 1 |
| `x04_moneytrain_a` | `0x3573b0` | `P_WoltCode` | 1 |
| `x04_moneytrain_a` | `0x357623` | `P_WoltCode` | 1 |
| `x04_moneytrain_a` | `0x3579a7` | `P_KillChief` | 1 |
| `x04_moneytrain_a` | `0x3579b9` | `P_ChiefAlive` | 2 |
| `x04_moneytrain_a` | `0x357aea` | `P_KillChief` | 1 |
| `x04_moneytrain_a` | `0x357afc` | `P_ChiefAlive` | 2 |
| `x04_moneytrain_a` | `0x35e15a` | `P_KillChief` | 2 |
| `x04_moneytrain_a` | `0x35e16c` | `P_ChiefAlive` | 1 |
| `x04_moneytrain_a` | `0x3644fc` | `P_TrustJBA` | 2 |
| `x04_moneytrain_a` | `0x3645cb` | `P_TrustJBA` | 1 |
| `x04_moneytrain_a` | `0x36488b` | `P_TrustNSA` | 2 |
| `x04_moneytrain_a` | `0x36495a` | `P_TrustNSA` | 1 |
| `x04_moneytrain_b` | `0x25a03f` | `O_DetachCars` | 1 |
| `x04_moneytrain_b` | `0x25a310` | `P_DisconnectCom` | 1 |
| `x04_moneytrain_b` | `0x25a6ce` | `P_GoFrontTrain` | 1 |
| `x04_moneytrain_b` | `0x25a7a1` | `P_GoFrontTrain` | 1 |
| `x04_moneytrain_b` | `0x25a84a` | `P_GoFrontTrain` | 1 |
| `x04_moneytrain_b` | `0x25a8c4` | `P_GoFrontTrain` | 1 |
| `x04_moneytrain_b` | `0x260ccb` | `P_TrustJBA` | 2 |
| `x04_moneytrain_b` | `0x260dc7` | `P_TrustJBA` | 1 |
| `x04_moneytrain_b` | `0x26110f` | `P_TrustNSA` | 2 |
| `x04_moneytrain_b` | `0x26120e` | `P_TrustNSA` | 1 |
| `x05_hq2_r` | `0x3e0929` | `OBJ_HQA_P_40` | 1 |
| `x05_hq2_r` | `0x3e0e04` | `OBJ_HQA_P_10` | 1 |
| `x05_hq2_r` | `0x3e1517` | `OBJ_HQA_P_20` | 1 |
| `x05_hq2_r` | `0x3e17e8` | `CarsonMoss` | 1 |
| `x05_hq2_r` | `0x3e19f6` | `OBJ_HQA_P_40` | 1 |
| `x05_hq2_r` | `0x3e4e97` | `OBJ_HQB_P_40` | 1 |
| `x05_hq2_r` | `0x3e5ec0` | `OBJ_HQB_P_10` | 1 |
| `x05_hq2_r` | `0x3e6229` | `OBJ_HQB_P_20` | 1 |
| `x05_hq2_r` | `0x3e6753` | `OBJ_HQB_P_30` | 1 |
| `x05_hq2_r` | `0x3e6862` | `KeepBombs` | 2 |
| `x05_hq2_r` | `0x3e68f6` | `OBJ_HQB_P_31` | 1 |
| `x05_hq2_r` | `0x3e6a05` | `KeepBombs` | 2 |
| `x05_hq2_r` | `0x3e6b28` | `OBJ_HQB_P_30` | 2 |
| `x05_hq2_r` | `0x3e6bc1` | `OBJ_HQB_P_31` | 2 |
| `x05_hq2_r` | `0x3e7013` | `OBJ_HQB_P_40` | 1 |
| `x05_hq2_r` | `0x3ee73d` | `KeepBombs` | 1 |
| `x05_hq2_r` | `0x3ee936` | `KeepBombs` | 1 |
| `x05_hq2_r` | `0x3f2a44` | `KeepBombs` | 1 |
| `x05_hq2_r` | `0x3f2aed` | `KeepBombs` | 1 |
| `x05_hq2_r` | `0x3f2bcb` | `KeepBombs` | 1 |
| `x05_hq2_r` | `0x3f8baa` | `OBJ_HQA_O_20` | 1 |
| `x05_hq2_r` | `0x3f9c0e` | `YeagerLambert` | 2 |
| `x05_hq2_r` | `0x3f9c22` | `YeagerEmile` | 1 |
| `x05_hq2_r` | `0x3f9cb5` | `YeagerLambert` | 1 |
| `x05_hq2_r` | `0x3f9cc9` | `YeagerEmile` | 2 |
| `x05_hq2_r` | `0x3f9f53` | `P_LambertCover` | 2 |
| `x05_hq2_r` | `0x3f9f68` | `P_LambertNOTCover` | 1 |
| `x05_hq2_r` | `0x3fa0c0` | `P_LambertCover` | 1 |
| `x05_hq2_r` | `0x3fa0d5` | `P_LambertNOTCover` | 2 |
| `x05_hq2_r` | `0x3fb424` | `TrustNow` | 2 |
| `x05_hq2_r` | `0x3fb53d` | `TrustNow` | 1 |
| `x05_hq2_r` | `0x3fb8a7` | `TrustNow` | 2 |
| `x05_hq2_r` | `0x3fb9c4` | `TrustNow` | 1 |
| `x05_hq2_r` | `0x3fbea4` | `P_TrustNSA` | 2 |
| `x05_hq2_r` | `0x3fbffa` | `P_TrustNSA` | 1 |
| `x05_hq2_r` | `0x3fc2ea` | `P_TrustNSAHq2` | 2 |
| `x05_hq2_r` | `0x3fc3a8` | `P_TrustNSAHq2` | 1 |
| `x05_hq2_r` | `0x3fcc3a` | `FindBunker` | 1 |
| `x06_cozumel_a` | `0x2f11d2` | `PlaceRelay` | 1 |
| `x06_cozumel_a` | `0x2f1398` | `GetDown` | 1 |
| `x06_cozumel_a` | `0x2f1589` | `PassengerLog` | 1 |
| `x06_cozumel_a` | `0x2fd3c1` | `P_TrustJBA` | 2 |
| `x06_cozumel_a` | `0x2fd49e` | `P_TrustJBA` | 1 |
| `x06_cozumel_a` | `0x2fd754` | `P_TrustNSA` | 2 |
| `x06_cozumel_a` | `0x2fd831` | `P_TrustNSA` | 1 |
| `x06_cozumel_a` | `0x2fda34` | `TEMPNSA` | 1 |
| `x06_cozumel_a` | `0x2fdacd` | `TEMPJBA` | 1 |
| `x06_cozumel_b` | `0x26dcae` | `GetBomb` | 1 |
| `x06_cozumel_b` | `0x26defe` | `PlantBomb` | 1 |
| `x06_cozumel_b` | `0x26e1c0` | `ChipCellPhone` | 1 |
| `x06_cozumel_b` | `0x26e1d4` | `ProCellPhone` | 2 |
| `x06_cozumel_b` | `0x26e2a5` | `ProCellPhone` | 1 |
| `x06_cozumel_b` | `0x26e2b8` | `ChipCellPhone` | 2 |
| `x06_cozumel_b` | `0x26e491` | `CameraRecord` | 1 |
| `x06_cozumel_b` | `0x26e63a` | `ShipEngine` | 1 |
| `x06_cozumel_b` | `0x26e808` | `PierExtraction` | 1 |
| `x06_cozumel_b` | `0x277019` | `P_TrustJBA` | 2 |
| `x06_cozumel_b` | `0x2770f6` | `P_TrustJBA` | 1 |
| `x06_cozumel_b` | `0x2773cc` | `P_TrustNSA` | 2 |
| `x06_cozumel_b` | `0x2774a9` | `P_TrustNSA` | 1 |
| `x06_cozumel_b` | `0x2776aa` | `TEMPNSA` | 1 |
| `x06_cozumel_b` | `0x277743` | `TEMPJBA` | 1 |
| `x08_okhotsk_a` | `0x277ac2` | `Mercenaries` | 1 |
| `x08_okhotsk_a` | `0x277bde` | `Communications` | 1 |
| `x08_okhotsk_a` | `0x277cc5` | `Bombs` | 1 |
| `x08_okhotsk_a` | `0x277d8c` | `KeyCode` | 1 |
| `x08_okhotsk_a` | `0x27804c` | `Bombs` | 2 |
| `x08_okhotsk_a` | `0x27806f` | `KeyCode` | 2 |
| `x08_okhotsk_a` | `0x282546` | `P_TrustJBA` | 2 |
| `x08_okhotsk_a` | `0x282607` | `P_TrustJBA` | 1 |
| `x08_okhotsk_a` | `0x28298d` | `P_TrustNSA` | 2 |
| `x08_okhotsk_a` | `0x282a4e` | `P_TrustNSA` | 1 |
| `x08_okhotsk_b` | `0x3abc24` | `Tanker` | 2 |
| `x08_okhotsk_b` | `0x3abd0d` | `Tracker_Done` | 1 |
| `x08_okhotsk_b` | `0x3abdec` | `Freight_Done` | 1 |
| `x08_okhotsk_b` | `0x3abecb` | `Freight_Done` | 1 |
| `x08_okhotsk_b` | `0x3abff9` | `Bombs_Done` | 1 |
| `x08_okhotsk_b` | `0x3ac220` | `Captain_extraction` | 1 |
| `x08_okhotsk_b` | `0x3b941f` | `P_TrustJBA` | 2 |
| `x08_okhotsk_b` | `0x3b9529` | `P_TrustJBA` | 1 |
| `x08_okhotsk_b` | `0x3b98c2` | `P_TrustNSA` | 2 |
| `x08_okhotsk_b` | `0x3b9983` | `P_TrustNSA` | 1 |
| `x08_okhotsk_b` | `0x3b9bae` | `TEMPNSA` | 1 |
| `x08_okhotsk_b` | `0x3b9c47` | `TEMPJBA` | 1 |
| `x10_kinshasa_a` | `0x3d22e9` | `OBJ_10_O_20` | 2 |
| `x10_kinshasa_a` | `0x3d2331` | `OBJ_10_O_10` | 2 |
| `x10_kinshasa_a` | `0x3d25b2` | `OBJ_10_P_50` | 1 |
| `x10_kinshasa_a` | `0x3d275d` | `OBJ_10_P_200` | 1 |
| `x10_kinshasa_a` | `0x3d2812` | `OBJ_10_O_10` | 1 |
| `x10_kinshasa_a` | `0x3d28e8` | `OBJ_10_O_20` | 1 |
| `x10_kinshasa_a` | `0x3d66ea` | `OBJ_10_O_100` | 1 |
| `x10_kinshasa_a` | `0x3df6b9` | `P_TrustJBA` | 2 |
| `x10_kinshasa_a` | `0x3df798` | `P_TrustJBA` | 1 |
| `x10_kinshasa_a` | `0x3dfa58` | `P_TrustNSA` | 2 |
| `x10_kinshasa_a` | `0x3dfb37` | `P_TrustNSA` | 1 |
| `x10_kinshasa_a` | `0x3dfd35` | `TEMPNSA` | 1 |
| `x10_kinshasa_a` | `0x3dfdd0` | `TEMPJBA` | 1 |
| `x10_kinshasa_b` | `0x2e0cbd` | `OBJ_10_P_60` | 1 |
| `x10_kinshasa_b` | `0x2e0ccf` | `OBJ_10_P_30` | 2 |
| `x10_kinshasa_b` | `0x2e0eb7` | `OBJ_10_P_30` | 1 |
| `x10_kinshasa_b` | `0x2e0ec9` | `OBJ_10_P_60` | 2 |
| `x10_kinshasa_b` | `0x2e0edb` | `HishamBody` | 2 |
| `x10_kinshasa_b` | `0x2e10e0` | `OBJ_10_P_70` | 1 |
| `x10_kinshasa_b` | `0x2e1372` | `OBJ_10_P_500` | 1 |
| `x10_kinshasa_b` | `0x2e5194` | `HishamBody` | 1 |
| `x10_kinshasa_b` | `0x2e5216` | `HishamBody` | 2 |
| `x10_kinshasa_b` | `0x2ec935` | `P_TrustJBA` | 2 |
| `x10_kinshasa_b` | `0x2eca14` | `P_TrustJBA` | 1 |
| `x10_kinshasa_b` | `0x2eccd4` | `P_TrustNSA` | 2 |
| `x10_kinshasa_b` | `0x2ecdb3` | `P_TrustNSA` | 1 |
| `x10_kinshasa_b` | `0x2ecfb9` | `TEMPNSA` | 1 |
| `x10_kinshasa_b` | `0x2ed054` | `TEMPJBA` | 1 |
| `x11_hq5_r` | `0x3fcc9a` | `OBJ_HQA_P_40` | 1 |
| `x11_hq5_r` | `0x3fd175` | `OBJ_HQA_P_10` | 1 |
| `x11_hq5_r` | `0x3fd888` | `OBJ_HQA_P_20` | 1 |
| `x11_hq5_r` | `0x3fdb59` | `CarsonMoss` | 1 |
| `x11_hq5_r` | `0x3fdd67` | `OBJ_HQA_P_40` | 1 |
| `x11_hq5_r` | `0x401208` | `OBJ_HQB_P_40` | 1 |
| `x11_hq5_r` | `0x402231` | `OBJ_HQB_P_10` | 1 |
| `x11_hq5_r` | `0x40259a` | `OBJ_HQB_P_20` | 1 |
| `x11_hq5_r` | `0x402ac4` | `OBJ_HQB_P_30` | 1 |
| `x11_hq5_r` | `0x402bd3` | `KeepBombs` | 2 |
| `x11_hq5_r` | `0x402c67` | `OBJ_HQB_P_31` | 1 |
| `x11_hq5_r` | `0x402d76` | `KeepBombs` | 2 |
| `x11_hq5_r` | `0x402e99` | `OBJ_HQB_P_30` | 2 |
| `x11_hq5_r` | `0x402f32` | `OBJ_HQB_P_31` | 2 |
| `x11_hq5_r` | `0x403384` | `OBJ_HQB_P_40` | 1 |
| `x11_hq5_r` | `0x40aaae` | `KeepBombs` | 1 |
| `x11_hq5_r` | `0x40aca7` | `KeepBombs` | 1 |
| `x11_hq5_r` | `0x40edb5` | `KeepBombs` | 1 |
| `x11_hq5_r` | `0x40ee5e` | `KeepBombs` | 1 |
| `x11_hq5_r` | `0x40ef3c` | `KeepBombs` | 1 |
| `x11_hq5_r` | `0x414f1b` | `OBJ_HQA_O_20` | 1 |
| `x11_hq5_r` | `0x415f7f` | `YeagerLambert` | 2 |
| `x11_hq5_r` | `0x415f93` | `YeagerEmile` | 1 |
| `x11_hq5_r` | `0x416026` | `YeagerLambert` | 1 |
| `x11_hq5_r` | `0x41603a` | `YeagerEmile` | 2 |
| `x11_hq5_r` | `0x4162c4` | `P_LambertCover` | 2 |
| `x11_hq5_r` | `0x4162d9` | `P_LambertNOTCover` | 1 |
| `x11_hq5_r` | `0x416431` | `P_LambertCover` | 1 |
| `x11_hq5_r` | `0x416446` | `P_LambertNOTCover` | 2 |
| `x11_hq5_r` | `0x417795` | `TrustNow` | 2 |
| `x11_hq5_r` | `0x4178ae` | `TrustNow` | 1 |
| `x11_hq5_r` | `0x417c18` | `TrustNow` | 2 |
| `x11_hq5_r` | `0x417d35` | `TrustNow` | 1 |
| `x11_hq5_r` | `0x418215` | `P_TrustNSA` | 2 |
| `x11_hq5_r` | `0x41836b` | `P_TrustNSA` | 1 |
| `x11_hq5_r` | `0x41865b` | `P_TrustNSAHq2` | 2 |
| `x11_hq5_r` | `0x418719` | `P_TrustNSAHq2` | 1 |
| `x11_hq5_r` | `0x418fab` | `FindBunker` | 1 |
| `x12_ny_a` | `0x3d594c` | `bomba` | 1 |
| `x12_ny_a` | `0x3d5a4f` | `hikilla` | 1 |
| `x12_ny_a` | `0x3d5b11` | `killalla` | 1 |
| `x12_ny_b` | `0x3027f1` | `ReachB` | 1 |
| `x12_ny_b` | `0x30281d` | `killalla` | 2 |
| `x12_ny_b` | `0x302863` | `bombb` | 1 |
| `x12_ny_b` | `0x30292a` | `hikillb` | 1 |
| `x12_ny_b` | `0x3029e8` | `killallb` | 1 |
| `x12_ny_c` | `0x409f7b` | `ReachC` | 1 |
| `x12_ny_c` | `0x409fff` | `killallb` | 2 |
| `x12_ny_c` | `0x40a056` | `bombc` | 1 |
| `x12_ny_c` | `0x40a10c` | `hikillc` | 1 |
| `x12_ny_c` | `0x40a277` | `killallc` | 1 |
| `x12_ny_c` | `0x40a3a9` | `killallc` | 2 |

## Outside the solo campaign

All 44 recognized co-op declarations have zero trust amounts, including omitted optional amounts (native default 0). No direct constant `ModifyTrust` call was retained in these maps.

## Direct constant call

`x04_moneytrain_a`, offset `0x3579cc`: amount 15, JBA boolean true, hence delta −15. Branch name: `Primary_Obj_Kill_Done_After_Alive`; reverses the station chief's earlier NSA credit.

## Coverage

The scanner recognizes the documented constant native calls. It does not replace a control-flow decompiler or prove the absence of dynamic references. False patterns in graphics data are not counted as scripts.
