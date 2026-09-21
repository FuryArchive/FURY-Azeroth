CREATE TABLE IF NOT EXISTS `updates` (
  `name` varchar(200) NOT NULL COMMENT 'filename with extension of the update',
  `hash` char(40) DEFAULT '' COMMENT 'sha1 hash of the sql file',
  `state` enum('RELEASED','ARCHIVED','CUSTOM') NOT NULL DEFAULT 'RELEASED',
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `speed` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC
  COMMENT='List of all applied updates in this database';
