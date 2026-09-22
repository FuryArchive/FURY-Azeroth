CREATE TABLE IF NOT EXISTS `fury_household` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `slug` varchar(64) NOT NULL,
  `display_name` varchar(96) NOT NULL,
  `current_power_band` smallint unsigned NOT NULL DEFAULT 0,
  `created_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  `updated_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
    ON UPDATE CURRENT_TIMESTAMP(6),
  `revision` bigint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uq_fury_household_slug` (`slug`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_household_member` (
  `household_id` bigint unsigned NOT NULL,
  `account_id` int unsigned NOT NULL,
  `role` tinyint unsigned NOT NULL DEFAULT 1,
  `joined_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (`household_id`, `account_id`),
  UNIQUE KEY `uq_fury_household_account` (`account_id`),
  CONSTRAINT `fk_fury_household_member_household`
    FOREIGN KEY (`household_id`)
    REFERENCES `fury_household` (`id`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
