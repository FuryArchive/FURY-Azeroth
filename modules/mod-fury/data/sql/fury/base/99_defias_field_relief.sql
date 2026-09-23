-- T31: adaptive Field Relief profession order.
-- All options are starter-tier crafts so selection can be based on a learned
-- profession rather than on a high skill threshold.

INSERT IGNORE INTO `fury_profession_order`
  (`order_key`, `title`, `repeat_policy`, `enabled`)
VALUES
  ('classic.westfall.defias.field_relief.supplies',
   'Field Relief Supplies', 1, 1);

INSERT IGNORE INTO `fury_profession_order_option`
  (`order_key`, `ordinal`, `skill_id`, `item_id`, `required_count`)
VALUES
  ('classic.westfall.defias.field_relief.supplies', 1, 129, 1251, 8),
  ('classic.westfall.defias.field_relief.supplies', 2, 171, 118, 5),
  ('classic.westfall.defias.field_relief.supplies', 3, 185, 2679, 8),
  ('classic.westfall.defias.field_relief.supplies', 4, 165, 2304, 4),
  ('classic.westfall.defias.field_relief.supplies', 5, 164, 2862, 6),
  ('classic.westfall.defias.field_relief.supplies', 6, 197, 2996, 6),
  ('classic.westfall.defias.field_relief.supplies', 7, 202, 4357, 8);
