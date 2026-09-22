CREATE TABLE IF NOT EXISTS `fury_chronicle_entry` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `household_id` bigint unsigned NOT NULL,
  `entry_key` varchar(128) NOT NULL,
  `category` varchar(48) NOT NULL,
  `title` varchar(160) NOT NULL,
  `body` text DEFAULT NULL,
  `source_event_id` bigint unsigned NOT NULL,
  `occurred_at` timestamp(6) NOT NULL,
  `metadata` json DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_chronicle_source`
    (`household_id`, `entry_key`, `source_event_id`),
  KEY `ix_fury_chronicle_household`
    (`household_id`, `occurred_at`, `id`),
  CONSTRAINT `fk_fury_chronicle_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_chronicle_event`
    FOREIGN KEY (`source_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
