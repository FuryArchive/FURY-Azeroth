-- First-party Defias graph; never overwrite operator content settings.
INSERT IGNORE INTO `fury_campaign_node`
  (`node_key`, `era`, `ordinal`, `display_name`, `required_power_band`, `grants_power_band`, `enabled`)
VALUES ('campaign.classic.westfall', 1, 20, 'Westfall', 0, 0, 1);

INSERT IGNORE INTO `fury_director_graph`
  (`graph_key`, `scope_key`, `display_name`, `campaign_node_key`, `enabled`)
VALUES ('classic.westfall.defias_resurgence.v1', 'classic.westfall',
  'Defias Resurgence', 'campaign.classic.westfall', 1);
