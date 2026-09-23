-- T32: Defias persistent resolution reward policy.
INSERT IGNORE INTO `fury_reward_bundle`
  (`reward_key`, `minimum_power_band`, `maximum_power_band`)
VALUES
  ('classic.westfall.defias.success', 0, NULL);
