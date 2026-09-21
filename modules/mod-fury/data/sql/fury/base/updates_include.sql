CREATE TABLE IF NOT EXISTS `updates_include` (
  `path` varchar(200) NOT NULL,
  `state` enum('RELEASED','ARCHIVED','CUSTOM') NOT NULL DEFAULT 'RELEASED',
  PRIMARY KEY (`path`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC
  COMMENT='Directories containing FURY SQL updates';

DELETE FROM `updates_include`;
INSERT INTO `updates_include` (`path`, `state`) VALUES
  ('$/data/sql/fury/updates', 'RELEASED'),
  ('$/data/sql/fury/custom', 'CUSTOM'),
  ('$/data/sql/fury/archive', 'ARCHIVED');
