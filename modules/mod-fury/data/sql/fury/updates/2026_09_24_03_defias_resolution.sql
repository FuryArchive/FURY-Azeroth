-- T32: Defias persistent resolution and repeatable attempts.
--
-- T25 originally made the first vertical slice one-shot. T32 intentionally
-- permits a later attempt after Partial/Ignored while the generic active-scope
-- uniqueness still forbids concurrent Westfall Director runs.

SET @fury_has_defias_once_index := (
  SELECT COUNT(*)
  FROM information_schema.statistics
  WHERE table_schema = DATABASE()
    AND table_name = 'fury_director_run'
    AND index_name = 'uq_fury_defias_once'
);
SET @fury_drop_defias_once_index := IF(
  @fury_has_defias_once_index > 0,
  'ALTER TABLE fury_director_run DROP INDEX uq_fury_defias_once',
  'SELECT 1'
);
PREPARE fury_stmt FROM @fury_drop_defias_once_index;
EXECUTE fury_stmt;
DEALLOCATE PREPARE fury_stmt;

SET @fury_has_defias_once_column := (
  SELECT COUNT(*)
  FROM information_schema.columns
  WHERE table_schema = DATABASE()
    AND table_name = 'fury_director_run'
    AND column_name = 'defias_once'
);
SET @fury_drop_defias_once_column := IF(
  @fury_has_defias_once_column > 0,
  'ALTER TABLE fury_director_run DROP COLUMN defias_once',
  'SELECT 1'
);
PREPARE fury_stmt FROM @fury_drop_defias_once_column;
EXECUTE fury_stmt;
DEALLOCATE PREPARE fury_stmt;

-- A success records a receipt-backed FURY claim, but has no physical reward
-- entries yet. Delivery remains Pending until a safe materializer exists.
INSERT IGNORE INTO `fury_reward_bundle`
  (`reward_key`, `minimum_power_band`, `maximum_power_band`)
VALUES
  ('classic.westfall.defias.success', 0, NULL);
