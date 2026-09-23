-- T32: Defias is no longer one-shot.
--
-- T25 originally protected the first vertical slice with a generated
-- household/graph uniqueness key. T32 defines Partial and Ignored as
-- non-terminal campaign outcomes that may be attempted again, so upgraded
-- databases must retire that historical one-shot constraint.

SET @fury_drop_defias_once_index = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.statistics
    WHERE table_schema = DATABASE()
      AND table_name = 'fury_director_run'
      AND index_name = 'uq_fury_defias_once'
  ),
  'ALTER TABLE fury_director_run DROP INDEX uq_fury_defias_once',
  'SELECT 1'
);
PREPARE fury_drop_defias_once_index_stmt
  FROM @fury_drop_defias_once_index;
EXECUTE fury_drop_defias_once_index_stmt;
DEALLOCATE PREPARE fury_drop_defias_once_index_stmt;

SET @fury_drop_defias_once_column = IF(
  EXISTS(
    SELECT 1
    FROM information_schema.columns
    WHERE table_schema = DATABASE()
      AND table_name = 'fury_director_run'
      AND column_name = 'defias_once'
  ),
  'ALTER TABLE fury_director_run DROP COLUMN defias_once',
  'SELECT 1'
);
PREPARE fury_drop_defias_once_column_stmt
  FROM @fury_drop_defias_once_column;
EXECUTE fury_drop_defias_once_column_stmt;
DEALLOCATE PREPARE fury_drop_defias_once_column_stmt;
