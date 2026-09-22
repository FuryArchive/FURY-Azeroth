CREATE TABLE IF NOT EXISTS `fury_director_graph` (
  `graph_key` varchar(128) NOT NULL,
  `scope_key` varchar(128) NOT NULL,
  `display_name` varchar(160) NOT NULL,
  `campaign_node_key` varchar(128) DEFAULT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`graph_key`),
  KEY `ix_fury_director_graph_scope` (`scope_key`, `enabled`),
  CONSTRAINT `fk_fury_director_graph_campaign_node`
    FOREIGN KEY (`campaign_node_key`)
    REFERENCES `fury_campaign_node` (`node_key`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_director_run` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `household_id` bigint unsigned NOT NULL,
  `graph_key` varchar(128) NOT NULL,
  `scope_key` varchar(128) NOT NULL,
  `status` tinyint unsigned NOT NULL,
  `phase_key` varchar(128) NOT NULL,
  `external_runtime_id` bigint unsigned DEFAULT NULL,
  `started_event_id` bigint unsigned NOT NULL,
  `last_event_id` bigint unsigned NOT NULL,
  `resolved_event_id` bigint unsigned DEFAULT NULL,
  `outcome_key` varchar(128) DEFAULT NULL,
  `started_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `updated_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
    ON UPDATE CURRENT_TIMESTAMP(6),
  `completed_at` timestamp(6) NULL DEFAULT NULL,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  `active_scope_key` varchar(128)
    GENERATED ALWAYS AS (
      CASE WHEN `status` IN (1,2,3) THEN `scope_key` ELSE NULL END
    ) STORED,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_director_active_scope`
    (`household_id`, `active_scope_key`),
  KEY `ix_fury_director_external_runtime`
    (`external_runtime_id`),
  KEY `ix_fury_director_run_graph`
    (`household_id`, `graph_key`, `status`),
  CONSTRAINT `fk_fury_director_run_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_director_run_graph`
    FOREIGN KEY (`graph_key`)
    REFERENCES `fury_director_graph` (`graph_key`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_director_run_started_event`
    FOREIGN KEY (`started_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_director_run_last_event`
    FOREIGN KEY (`last_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_director_run_resolved_event`
    FOREIGN KEY (`resolved_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
