CREATE TABLE IF NOT EXISTS `fury_profession_order` (
  `order_key` varchar(128) NOT NULL,
  `title` varchar(160) NOT NULL,
  `repeat_policy` tinyint unsigned NOT NULL DEFAULT 1,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`order_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_profession_order_option` (
  `order_key` varchar(128) NOT NULL,
  `ordinal` smallint unsigned NOT NULL,
  `skill_id` int unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `required_count` int unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`order_key`, `ordinal`),
  KEY `ix_fury_profession_order_option_target`
    (`skill_id`, `item_id`),
  CONSTRAINT `fk_fury_profession_order_option_order`
    FOREIGN KEY (`order_key`)
    REFERENCES `fury_profession_order` (`order_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_profession_order_instance` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `household_id` bigint unsigned NOT NULL,
  `order_key` varchar(128) NOT NULL,
  `option_ordinal` smallint unsigned NOT NULL,
  `status` tinyint unsigned NOT NULL,
  `progress_count` int unsigned NOT NULL DEFAULT 0,
  `accepted_event_id` bigint unsigned NOT NULL,
  `last_event_id` bigint unsigned NOT NULL,
  `completed_event_id` bigint unsigned DEFAULT NULL,
  `accepted_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `completed_at` timestamp(6) NULL DEFAULT NULL,
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  `active_order_key` varchar(128)
    GENERATED ALWAYS AS (
      CASE WHEN `status` = 2 THEN `order_key` ELSE NULL END
    ) STORED,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_profession_order_active`
    (`household_id`, `active_order_key`),
  KEY `ix_fury_profession_order_instance_state`
    (`household_id`, `order_key`, `status`),
  CONSTRAINT `fk_fury_profession_order_instance_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_profession_order_instance_option`
    FOREIGN KEY (`order_key`, `option_ordinal`)
    REFERENCES `fury_profession_order_option` (`order_key`, `ordinal`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_profession_order_instance_accepted_event`
    FOREIGN KEY (`accepted_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_profession_order_instance_last_event`
    FOREIGN KEY (`last_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_profession_order_instance_completed_event`
    FOREIGN KEY (`completed_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
