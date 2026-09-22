CREATE TABLE IF NOT EXISTS `fury_contract` (
  `contract_key` varchar(128) NOT NULL,
  `board_key` varchar(128) NOT NULL,
  `title` varchar(160) NOT NULL,
  `campaign_node_key` varchar(128) DEFAULT NULL,
  `director_phase` varchar(128) DEFAULT NULL,
  `repeat_policy` tinyint unsigned NOT NULL DEFAULT 1,
  `reward_key` varchar(128) DEFAULT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`contract_key`),
  KEY `ix_fury_contract_board` (`board_key`, `enabled`),
  KEY `ix_fury_contract_campaign` (`campaign_node_key`, `enabled`),
  CONSTRAINT `fk_fury_contract_campaign_node`
    FOREIGN KEY (`campaign_node_key`)
    REFERENCES `fury_campaign_node` (`node_key`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_contract_objective` (
  `contract_key` varchar(128) NOT NULL,
  `ordinal` smallint unsigned NOT NULL,
  `objective_type` tinyint unsigned NOT NULL,
  `event_type` varchar(96) NOT NULL,
  `subject_type` varchar(48) DEFAULT NULL,
  `subject_id` bigint unsigned DEFAULT NULL,
  `required_count` int unsigned NOT NULL DEFAULT 1,
  `criteria` json DEFAULT NULL,
  PRIMARY KEY (`contract_key`, `ordinal`),
  KEY `ix_fury_contract_objective_match`
    (`event_type`, `subject_type`, `subject_id`, `contract_key`, `ordinal`),
  CONSTRAINT `fk_fury_contract_objective_contract`
    FOREIGN KEY (`contract_key`)
    REFERENCES `fury_contract` (`contract_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_contract_instance` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `household_id` bigint unsigned NOT NULL,
  `contract_key` varchar(128) NOT NULL,
  `director_run_id` bigint unsigned DEFAULT NULL,
  `status` tinyint unsigned NOT NULL,
  `accepted_event_id` bigint unsigned NOT NULL,
  `completed_event_id` bigint unsigned DEFAULT NULL,
  `accepted_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `completed_at` timestamp(6) NULL DEFAULT NULL,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  `active_contract_key` varchar(128)
    GENERATED ALWAYS AS (
      CASE WHEN `status` = 2 THEN `contract_key` ELSE NULL END
    ) STORED,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_contract_instance_active`
    (`household_id`, `active_contract_key`),
  KEY `ix_fury_contract_instance_contract`
    (`household_id`, `contract_key`, `status`),
  CONSTRAINT `fk_fury_contract_instance_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_contract_instance_contract`
    FOREIGN KEY (`contract_key`)
    REFERENCES `fury_contract` (`contract_key`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_contract_instance_accepted_event`
    FOREIGN KEY (`accepted_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_contract_instance_completed_event`
    FOREIGN KEY (`completed_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_contract_progress` (
  `instance_id` bigint unsigned NOT NULL,
  `objective_ordinal` smallint unsigned NOT NULL,
  `progress_count` int unsigned NOT NULL DEFAULT 0,
  `completed_at` timestamp(6) NULL DEFAULT NULL,
  `last_event_id` bigint unsigned NOT NULL DEFAULT 0,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`instance_id`, `objective_ordinal`),
  KEY `ix_fury_contract_progress_event` (`last_event_id`),
  CONSTRAINT `fk_fury_contract_progress_instance`
    FOREIGN KEY (`instance_id`)
    REFERENCES `fury_contract_instance` (`id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
