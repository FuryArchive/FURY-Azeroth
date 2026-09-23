-- Forward-only, repeatable T24 -> T25 upgrade. The unique key applies only
-- to the authored one-shot Defias graph; other Director graphs can repeat.
SET @fury_defias_ddl = IF(
  EXISTS(SELECT 1 FROM information_schema.columns
    WHERE table_schema = DATABASE() AND table_name = 'fury_director_run'
      AND column_name = 'defias_once'),
  'SELECT 1',
  'ALTER TABLE fury_director_run ADD COLUMN defias_once TINYINT UNSIGNED GENERATED ALWAYS AS (CASE WHEN graph_key = ''classic.westfall.defias_resurgence.v1'' THEN 1 ELSE NULL END) STORED, ADD UNIQUE KEY uq_fury_defias_once (household_id, defias_once)'
);
PREPARE fury_defias_upgrade FROM @fury_defias_ddl;
EXECUTE fury_defias_upgrade;
DEALLOCATE PREPARE fury_defias_upgrade;

-- First-party Defias graph; never overwrite operator content settings.
INSERT IGNORE INTO `fury_campaign_node`
  (`node_key`, `era`, `ordinal`, `display_name`, `required_power_band`, `grants_power_band`, `enabled`)
VALUES ('campaign.classic.westfall', 1, 20, 'Westfall', 0, 0, 1);

INSERT IGNORE INTO `fury_director_graph`
  (`graph_key`, `scope_key`, `display_name`, `campaign_node_key`, `enabled`)
VALUES ('classic.westfall.defias_resurgence.v1', 'classic.westfall',
  'Defias Resurgence', 'campaign.classic.westfall', 1);
