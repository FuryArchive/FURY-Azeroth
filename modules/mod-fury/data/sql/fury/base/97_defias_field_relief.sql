-- T31: adaptive Field Relief profession order.
-- The three targets are baseline WotLK recipes available from the first rank
-- of their respective profession paths.

INSERT IGNORE INTO `fury_profession_order`
  (`order_key`, `title`, `repeat_policy`, `enabled`)
VALUES
  ('classic.westfall.defias.field_relief', 'Field Relief Supplies', 2, 1);

INSERT IGNORE INTO `fury_profession_order_option`
  (`order_key`, `ordinal`, `skill_id`, `item_id`, `required_count`)
VALUES
  ('classic.westfall.defias.field_relief', 1, 171, 118, 3),
  ('classic.westfall.defias.field_relief', 2, 129, 1251, 6),
  ('classic.westfall.defias.field_relief', 3, 185, 2681, 6);
