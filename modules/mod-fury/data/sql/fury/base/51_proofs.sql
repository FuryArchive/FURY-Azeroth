CREATE TABLE IF NOT EXISTS `fury_proof` (
  `household_id` bigint unsigned NOT NULL,
  `proof_key` varchar(128) NOT NULL,
  `source_event_id` bigint unsigned NOT NULL,
  `granted_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `metadata` json DEFAULT NULL,
  PRIMARY KEY (`household_id`, `proof_key`),
  KEY `ix_fury_proof_event` (`source_event_id`),
  CONSTRAINT `fk_fury_proof_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_proof_event`
    FOREIGN KEY (`source_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
