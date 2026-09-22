SET @fury_ip_column_exists := (
  SELECT COUNT(*)
  FROM information_schema.columns
  WHERE table_schema = DATABASE()
    AND table_name = 'fury_campaign_node'
    AND column_name = 'ip_required_state'
);

SET @fury_ip_column_sql := IF(
  @fury_ip_column_exists = 0,
  'ALTER TABLE fury_campaign_node ADD COLUMN ip_required_state tinyint unsigned NOT NULL DEFAULT 0 AFTER grants_power_band',
  'SELECT 1'
);

PREPARE fury_ip_column_stmt FROM @fury_ip_column_sql;
EXECUTE fury_ip_column_stmt;
DEALLOCATE PREPARE fury_ip_column_stmt;
