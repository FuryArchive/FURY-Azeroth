INSERT INTO `fury_director_graph`
  (`graph_key`, `scope_key`, `display_name`, `campaign_node_key`, `enabled`)
VALUES
  ('defias.resurgence', 'world.westfall.defias', 'Defias Resurgence', NULL, 1)
ON DUPLICATE KEY UPDATE
  `scope_key` = VALUES(`scope_key`),
  `display_name` = VALUES(`display_name`),
  `enabled` = VALUES(`enabled`);
