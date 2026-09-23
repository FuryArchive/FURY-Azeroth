-- T30: Defias Bestiary content. Runtime groups, not creature entries,
-- are authoritative so ordinary Westfall Defias never advance these entries.

INSERT IGNORE INTO `fury_bestiary_entry`
  (`entry_key`, `display_name`, `enabled`)
VALUES
  ('classic.westfall.defias.scouts', 'Defias Scouts', 1),
  ('classic.westfall.defias.control_teams', 'Defias Control Teams', 1),
  ('classic.westfall.defias.commander', 'Captain Garrick Vane', 1);

INSERT IGNORE INTO `fury_bestiary_event_map`
  (`event_type`, `subject_type`, `subject_id`,
   `entry_key`, `discovery_level`, `enabled`)
VALUES
  ('living_world.entity.killed', 'living_world_spawn_group', 100,
   'classic.westfall.defias.scouts', 2, 1),
  ('living_world.entity.killed', 'living_world_spawn_group', 101,
   'classic.westfall.defias.scouts', 2, 1),
  ('living_world.entity.killed', 'living_world_spawn_group', 102,
   'classic.westfall.defias.control_teams', 2, 1),
  ('living_world.entity.killed', 'living_world_spawn_group', 103,
   'classic.westfall.defias.control_teams', 2, 1),
  ('living_world.entity.killed', 'living_world_spawn_group', 104,
   'classic.westfall.defias.control_teams', 2, 1),
  ('living_world.entity.killed', 'living_world_spawn_group', 105,
   'classic.westfall.defias.commander', 2, 1);
