-- T27: first playable Defias contract set.
-- Definitions are first-party defaults. INSERT IGNORE preserves operator edits.

INSERT IGNORE INTO `fury_contract`
  (`contract_key`, `board_key`, `title`, `campaign_node_key`,
   `director_phase`, `repeat_policy`, `reward_key`, `enabled`)
VALUES
  ('classic.westfall.defias.scout_report',
   'classic.westfall.contracts', 'Recon Roads',
   'campaign.classic.westfall', 'rumours', 3, NULL, 1),
  ('classic.westfall.defias.break_scouts',
   'classic.westfall.contracts', 'Break Scouts',
   'campaign.classic.westfall', 'invasion', 3, NULL, 1),
  ('classic.westfall.defias.break_control',
   'classic.westfall.contracts', 'Break Control',
   'campaign.classic.westfall', 'invasion', 3, NULL, 1),
  ('classic.westfall.defias.hold_sentinel',
   'classic.westfall.contracts', 'Hold Sentinel',
   'campaign.classic.westfall', 'invasion', 3, NULL, 1),
  ('classic.westfall.defias.field_relief',
   'classic.westfall.contracts', 'Field Relief',
   'campaign.classic.westfall', 'invasion', 3, NULL, 1),
  ('classic.westfall.defias.defeat_commander',
   'classic.westfall.contracts', 'Defeat Commander',
   'campaign.classic.westfall', 'invasion', 3, NULL, 1);

-- Runtime-qualified kills use subject_type=living_world_spawn_group and the
-- authored Living World spawn-group id. Ordinary Westfall creature kills
-- therefore cannot satisfy any of these objectives.

INSERT IGNORE INTO `fury_contract_objective`
  (`contract_key`, `ordinal`, `objective_type`, `event_type`,
   `subject_type`, `subject_id`, `required_count`, `criteria`)
VALUES
  -- Recon Roads: make contact with both authored scout columns.
  ('classic.westfall.defias.scout_report', 1, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 100, 1,
   JSON_OBJECT('spawn_group', 100, 'role', 'scout')),
  ('classic.westfall.defias.scout_report', 2, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 101, 1,
   JSON_OBJECT('spawn_group', 101, 'role', 'scout')),

  -- Break Scouts: the pinned content authors exactly three entities per scout
  -- group, so this is a deterministic full clear if accepted before combat.
  ('classic.westfall.defias.break_scouts', 1, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 100, 3,
   JSON_OBJECT('spawn_group', 100, 'role', 'scout')),
  ('classic.westfall.defias.break_scouts', 2, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 101, 3,
   JSON_OBJECT('spawn_group', 101, 'role', 'scout')),

  -- Break Control: damage each of the three 30-unit control teams rather than
  -- farming one replenishing group.
  ('classic.westfall.defias.break_control', 1, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 102, 6,
   JSON_OBJECT('spawn_group', 102, 'role', 'control')),
  ('classic.westfall.defias.break_control', 2, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 103, 6,
   JSON_OBJECT('spawn_group', 103, 'role', 'control')),
  ('classic.westfall.defias.break_control', 3, 1,
   'living_world.entity.killed', 'living_world_spawn_group', 104, 6,
   JSON_OBJECT('spawn_group', 104, 'role', 'control')),

  -- Hold Sentinel: broad human contribution across every hostile formation.
  ('classic.westfall.defias.hold_sentinel', 1, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 100, 1,
   JSON_OBJECT('spawn_group', 100, 'role', 'defense')),
  ('classic.westfall.defias.hold_sentinel', 2, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 101, 1,
   JSON_OBJECT('spawn_group', 101, 'role', 'defense')),
  ('classic.westfall.defias.hold_sentinel', 3, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 102, 3,
   JSON_OBJECT('spawn_group', 102, 'role', 'defense')),
  ('classic.westfall.defias.hold_sentinel', 4, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 103, 3,
   JSON_OBJECT('spawn_group', 103, 'role', 'defense')),
  ('classic.westfall.defias.hold_sentinel', 5, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 104, 3,
   JSON_OBJECT('spawn_group', 104, 'role', 'defense')),
  ('classic.westfall.defias.hold_sentinel', 6, 8,
   'living_world.entity.killed', 'living_world_spawn_group', 105, 1,
   JSON_OBJECT('spawn_group', 105, 'role', 'defense')),

  -- T31 owns the adaptive profession-order implementation and emits this one
  -- stable completion signal. Keeping it explicit now makes Field Relief a
  -- real optional contract without inventing profession/item ids in T27.
  ('classic.westfall.defias.field_relief', 1, 11,
   'defias.field_relief.completed', 'defias_contract_signal', 1, 1,
   JSON_OBJECT('provider', 'profession_order_t31')),

  -- The pinned leadership group contains exactly one named field commander.
  ('classic.westfall.defias.defeat_commander', 1, 2,
   'living_world.entity.killed', 'living_world_spawn_group', 105, 1,
   JSON_OBJECT('spawn_group', 105, 'role', 'commander'));
