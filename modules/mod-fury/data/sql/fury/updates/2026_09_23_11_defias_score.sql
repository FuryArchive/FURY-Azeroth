-- Generic Director score definitions and immutable awarded snapshots.
CREATE TABLE IF NOT EXISTS `fury_director_score_component` (
  `graph_key` varchar(128) NOT NULL,
  `component_key` varchar(128) NOT NULL,
  `score_value` smallint unsigned NOT NULL,
  `source_event_type` varchar(96) NOT NULL,
  `source_correlation_key` varchar(128) DEFAULT NULL,
  `enabled` tinyint unsigned NOT NULL DEFAULT 1,
  PRIMARY KEY (`graph_key`, `component_key`),
  UNIQUE KEY `uq_fury_director_score_source`
    (`graph_key`, `source_event_type`, `source_correlation_key`),
  KEY `ix_fury_director_score_match`
    (`source_event_type`, `source_correlation_key`, `graph_key`, `enabled`),
  CONSTRAINT `fk_fury_director_score_graph`
    FOREIGN KEY (`graph_key`)
    REFERENCES `fury_director_graph` (`graph_key`)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `fury_director_score_award` (
  `director_run_id` bigint unsigned NOT NULL,
  `graph_key` varchar(128) NOT NULL,
  `component_key` varchar(128) NOT NULL,
  `score_value` smallint unsigned NOT NULL,
  `source_event_id` bigint unsigned NOT NULL,
  `awarded_at` timestamp(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
  PRIMARY KEY (`director_run_id`, `component_key`),
  KEY `ix_fury_director_score_award_event` (`source_event_id`),
  CONSTRAINT `fk_fury_director_score_award_run`
    FOREIGN KEY (`director_run_id`)
    REFERENCES `fury_director_run` (`id`)
    ON DELETE CASCADE,
  CONSTRAINT `fk_fury_director_score_award_component`
    FOREIGN KEY (`graph_key`, `component_key`)
    REFERENCES `fury_director_score_component` (`graph_key`, `component_key`)
    ON DELETE RESTRICT,
  CONSTRAINT `fk_fury_director_score_award_event`
    FOREIGN KEY (`source_event_id`)
    REFERENCES `fury_event` (`id`)
    ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- T29 Defias score v1. INSERT IGNORE preserves operator-owned tuning.
INSERT IGNORE INTO `fury_director_score_component`
  (`graph_key`, `component_key`, `score_value`,
   `source_event_type`, `source_correlation_key`, `enabled`)
VALUES
  ('classic.westfall.defias_resurgence.v1',
   'recon', 10,
   'contract.completed', 'classic.westfall.defias.scout_report', 1),
  ('classic.westfall.defias_resurgence.v1',
   'scouts', 10,
   'contract.completed', 'classic.westfall.defias.break_scouts', 1),
  ('classic.westfall.defias_resurgence.v1',
   'control', 15,
   'contract.completed', 'classic.westfall.defias.break_control', 1),
  ('classic.westfall.defias_resurgence.v1',
   'hold_sentinel', 15,
   'contract.completed', 'classic.westfall.defias.hold_sentinel', 1),
  ('classic.westfall.defias_resurgence.v1',
   'field_relief', 10,
   'contract.completed', 'classic.westfall.defias.field_relief', 1),
  ('classic.westfall.defias_resurgence.v1',
   'captain', 25,
   'contract.completed', 'classic.westfall.defias.defeat_commander', 1),
  ('classic.westfall.defias_resurgence.v1',
   'final_stage_presence', 15,
   'defias.final_stage.participated',
   'classic.westfall.defias_resurgence.v1', 1);
