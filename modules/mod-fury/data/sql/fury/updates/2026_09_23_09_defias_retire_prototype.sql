-- Retain the earlier SQL prototype and any run history; only the canonical
-- versioned graph is eligible for new automatic starts. Existing prototype
-- runs block a second household scenario and require explicit reconciliation.
UPDATE `fury_director_graph` SET `enabled` = 0
WHERE `graph_key` = 'defias.resurgence';
