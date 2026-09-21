CREATE TABLE IF NOT EXISTS `version_db_fury` (
  `sql_rev` varchar(100) NOT NULL,
  `required_rev` varchar(100) DEFAULT NULL,
  `date` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`sql_rev`),
  KEY `required_rev_idx` (`required_rev`),
  CONSTRAINT `fk_version_db_fury_required`
    FOREIGN KEY (`required_rev`) REFERENCES `version_db_fury` (`sql_rev`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC
  COMMENT='FURY database revision chain';

INSERT IGNORE INTO `version_db_fury` (`sql_rev`, `required_rev`, `date`)
VALUES ('2026_09_22_00_fury_base', NULL, '2026-09-22');
