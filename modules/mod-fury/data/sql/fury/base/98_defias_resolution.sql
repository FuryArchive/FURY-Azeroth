-- T32: Defias persistent resolution configuration.
--
-- The success bundle intentionally has no physical reward entries yet.
-- A successful run creates a unique Pending FURY claim. Delivery remains
-- withheld until a receipt-backed materializer exists, so replay cannot
-- duplicate valuable physical items.

INSERT IGNORE INTO `fury_reward_bundle`
  (`reward_key`, `minimum_power_band`, `maximum_power_band`)
VALUES
  ('classic.westfall.defias.success', 0, NULL);
