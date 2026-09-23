-- T26: server-side Westfall contract board.
-- Entry/guid 9000100 are reserved to FURY. The display id is an existing
-- 3.3.5 client asset, so this requires no client patch or addon.
INSERT INTO `gameobject_template`
  (`entry`, `type`, `displayId`, `name`, `IconName`,
   `castBarCaption`, `unk1`, `size`,
   `data0`, `data1`, `data2`, `data3`, `data4`, `data5`,
   `data6`, `data7`, `data8`, `data9`, `data10`, `data11`,
   `data12`, `data13`, `data14`, `data15`, `data16`, `data17`,
   `data18`, `data19`, `data20`, `data21`, `data22`, `data23`,
   `AIName`, `ScriptName`, `VerifiedBuild`)
VALUES
  (9000100, 10, 17, 'FURY Westfall Contract Board', 'Talk',
   '', '', 1.0,
   0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0,
   '', 'fury_westfall_contract_board', 0)
ON DUPLICATE KEY UPDATE
  `type` = VALUES(`type`),
  `displayId` = VALUES(`displayId`),
  `name` = VALUES(`name`),
  `IconName` = VALUES(`IconName`),
  `size` = VALUES(`size`),
  `ScriptName` = VALUES(`ScriptName`),
  `VerifiedBuild` = VALUES(`VerifiedBuild`);

INSERT INTO `gameobject`
  (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnMask`,
   `phaseMask`, `position_x`, `position_y`, `position_z`,
   `orientation`, `rotation0`, `rotation1`, `rotation2`,
   `rotation3`, `spawntimesecs`, `animprogress`, `state`,
   `ScriptName`, `VerifiedBuild`, `Comment`)
VALUES
  (9000100, 9000100, 0, 40, 108, 1,
   1, -10651.789063, 1039.241821, 33.536880,
   1.384337, 0, 0, 0.638208, 0.769864,
   0, 0, 1,
   'fury_westfall_contract_board', 0,
   'FURY T26 Westfall contract board')
ON DUPLICATE KEY UPDATE
  `id` = VALUES(`id`),
  `map` = VALUES(`map`),
  `zoneId` = VALUES(`zoneId`),
  `areaId` = VALUES(`areaId`),
  `spawnMask` = VALUES(`spawnMask`),
  `phaseMask` = VALUES(`phaseMask`),
  `position_x` = VALUES(`position_x`),
  `position_y` = VALUES(`position_y`),
  `position_z` = VALUES(`position_z`),
  `orientation` = VALUES(`orientation`),
  `rotation0` = VALUES(`rotation0`),
  `rotation1` = VALUES(`rotation1`),
  `rotation2` = VALUES(`rotation2`),
  `rotation3` = VALUES(`rotation3`),
  `spawntimesecs` = VALUES(`spawntimesecs`),
  `animprogress` = VALUES(`animprogress`),
  `state` = VALUES(`state`),
  `ScriptName` = VALUES(`ScriptName`),
  `VerifiedBuild` = VALUES(`VerifiedBuild`),
  `Comment` = VALUES(`Comment`);
