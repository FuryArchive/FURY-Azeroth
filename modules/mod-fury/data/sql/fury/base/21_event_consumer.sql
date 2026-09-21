CREATE TABLE IF NOT EXISTS `fury_event_consumer` (
  `consumer_key` varchar(96) NOT NULL,
  `last_event_id` bigint unsigned NOT NULL DEFAULT 0,
  `updated_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
    ON UPDATE CURRENT_TIMESTAMP(6),
  PRIMARY KEY (`consumer_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
