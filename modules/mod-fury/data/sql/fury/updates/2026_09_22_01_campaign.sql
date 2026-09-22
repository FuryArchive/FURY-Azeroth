CREATE TABLE IF NOT EXISTS `fury_campaign_node` (
  `node_key` varchar(128) NOT NULL,
  `era` tinyint unsigned NOT NULL,
  `ordinal` smallint unsigned NOT NULL,
  `display_name` varchar(160) NOT NULL,
  `required_power_band` smallint unsigned NOT NULL DEFAULT 0,
  `grants_power_band` smallint unsigned NOT NULL DEFAULT 0,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`node_key`),
  KEY `ix_fury_campaign_node_order` (`era`, `ordinal`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_campaign_state` (
  `household_id` bigint unsigned NOT NULL,
  `node_key` varchar(128) NOT NULL,
  `status` tinyint unsigned NOT NULL,
  `activated_at` timestamp(6) NULL DEFAULT NULL,
  `completed_at` timestamp(6) NULL DEFAULT NULL,
  `source_event_id` bigint unsigned DEFAULT NULL,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`household_id`, `node_key`),
  KEY `ix_fury_campaign_state_status` (`household_id`, `status`),
  CONSTRAINT `fk_fury_campaign_state_household`
    FOREIGN KEY (`household_id`) REFERENCES `fury_household` (`id`) ON DELETE CASCADE,
  CONSTRAINT `fk_fury_campaign_state_node`
    FOREIGN KEY (`node_key`) REFERENCES `fury_campaign_node` (`node_key`) ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_campaign_state_event`
    FOREIGN KEY (`source_event_id`) REFERENCES `fury_event` (`id`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
