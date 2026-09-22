-- FURY owns activation authority for the pinned Defias Westfall invasion.
-- AzerothCore's module DB updater applies and hash-tracks this overlay.
UPDATE `lw_invasion`
SET `allow_random_start` = 0
WHERE `id` = 1;
