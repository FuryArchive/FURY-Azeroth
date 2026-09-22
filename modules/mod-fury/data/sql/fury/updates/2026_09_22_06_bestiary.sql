CREATE TABLE IF NOT EXISTS `fury_bestiary_entry` (
  `entry_key` varchar(128) NOT NULL,
  `display_name` varchar(160) NOT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`entry_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_bestiary_creature_map` (
  `creature_entry` int unsigned NOT NULL,
  `entry_key` varchar(128) NOT NULL,
  `discovery_level` tinyint unsigned NOT NULL DEFAULT 1,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`creature_entry`, `entry_key`),
  KEY `ix_fury_bestiary_creature_map_lookup`
    (`creature_entry`, `enabled`, `entry_key`),
  CONSTRAINT `chk_fury_bestiary_creature_map_level`
    CHECK (`discovery_level` BETWEEN 1 AND 3),
  CONSTRAINT `fk_fury_bestiary_creature_map_entry`
    FOREIGN KEY (`entry_key`)
    REFERENCES `fury_bestiary_entry` (`entry_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_bestiary_state` (
  `account_id` int unsigned NOT NULL,
  `entry_key` varchar(128) NOT NULL,
  `discovery_level` tinyint unsigned NOT NULL DEFAULT 0,
  `kill_count` int unsigned NOT NULL DEFAULT 0,
  `first_event_id` bigint unsigned NOT NULL,
  `last_event_id` bigint unsigned NOT NULL,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`account_id`, `entry_key`),
  KEY `ix_fury_bestiary_state_level`
    (`account_id`, `discovery_level`, `entry_key`),
  CONSTRAINT `chk_fury_bestiary_state_level`
    CHECK (`discovery_level` BETWEEN 0 AND 3),
  CONSTRAINT `fk_fury_bestiary_state_entry`
    FOREIGN KEY (`entry_key`)
    REFERENCES `fury_bestiary_entry` (`entry_key`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_bestiary_state_first_event`
    FOREIGN KEY (`first_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_bestiary_state_last_event`
    FOREIGN KEY (`last_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
