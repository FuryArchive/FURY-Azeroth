CREATE TABLE IF NOT EXISTS `fury_bestiary_event_map` (
  `event_type` varchar(96) NOT NULL,
  `subject_type` varchar(48) NOT NULL,
  `subject_id` bigint unsigned NOT NULL,
  `entry_key` varchar(128) NOT NULL,
  `discovery_level` tinyint unsigned NOT NULL DEFAULT 1,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`event_type`, `subject_type`, `subject_id`, `entry_key`),
  KEY `ix_fury_bestiary_event_map_lookup`
    (`event_type`, `subject_type`, `subject_id`, `enabled`, `entry_key`),
  CONSTRAINT `chk_fury_bestiary_event_map_level`
    CHECK (`discovery_level` BETWEEN 1 AND 3),
  CONSTRAINT `fk_fury_bestiary_event_map_entry`
    FOREIGN KEY (`entry_key`)
    REFERENCES `fury_bestiary_entry` (`entry_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
