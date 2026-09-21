CREATE TABLE IF NOT EXISTS `fury_reward_bundle` (
  `reward_key` varchar(128) NOT NULL,
  `minimum_power_band` smallint unsigned NOT NULL DEFAULT 0,
  `maximum_power_band` smallint unsigned DEFAULT NULL,
  PRIMARY KEY (`reward_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_reward_entry` (
  `reward_key` varchar(128) NOT NULL,
  `ordinal` smallint unsigned NOT NULL,
  `reward_type` tinyint unsigned NOT NULL,
  `target_id` bigint unsigned DEFAULT NULL,
  `amount` bigint NOT NULL DEFAULT 0,
  `payload` json DEFAULT NULL,
  PRIMARY KEY (`reward_key`, `ordinal`),
  CONSTRAINT `fk_fury_reward_entry_bundle`
    FOREIGN KEY (`reward_key`)
    REFERENCES `fury_reward_bundle` (`reward_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_reward_claim` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `source_event_id` bigint unsigned NOT NULL,
  `reward_key` varchar(128) NOT NULL,
  `beneficiary_kind` tinyint unsigned NOT NULL,
  `beneficiary_id` bigint unsigned NOT NULL,
  `status` tinyint unsigned NOT NULL DEFAULT 0,
  `created_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `delivered_at` timestamp(6) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_reward_claim`
    (`source_event_id`, `reward_key`, `beneficiary_kind`, `beneficiary_id`),
  KEY `ix_fury_reward_claim_beneficiary`
    (`beneficiary_kind`, `beneficiary_id`, `status`),
  CONSTRAINT `fk_fury_reward_claim_event`
    FOREIGN KEY (`source_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_reward_claim_bundle`
    FOREIGN KEY (`reward_key`)
    REFERENCES `fury_reward_bundle` (`reward_key`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
