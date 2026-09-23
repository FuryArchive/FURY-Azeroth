#include "FuryDatabase.h"

#include "MySQLPreparedStatement.h"

namespace Fury
{
DatabasePool FuryDatabase;

DatabaseConnection::DatabaseConnection(MySQLConnectionInfo& connInfo)
    : MySQLConnection(connInfo)
{
}

DatabaseConnection::~DatabaseConnection() = default;

void DatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
        m_stmts.resize(MAX_FURY_DATABASE_STATEMENTS);

    PrepareStatement(FURY_SEL_HOUSEHOLD_BY_ID,
        "SELECT id FROM fury_household WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_BY_SLUG,
        "SELECT id FROM fury_household WHERE slug = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_BY_ACCOUNT,
        "SELECT household_id FROM fury_household_member WHERE account_id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_MEMBERS,
        "SELECT account_id, household_id FROM fury_household_member",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_MEMBER_COUNT,
        "SELECT COUNT(*) FROM fury_household_member WHERE household_id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_HOUSEHOLD,
        "INSERT INTO fury_household (slug, display_name) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE slug = VALUES(slug)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_HOUSEHOLD_MEMBER,
        "INSERT IGNORE INTO fury_household_member (household_id, account_id, role) VALUES (?, ?, ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_DEL_HOUSEHOLD_MEMBER,
        "DELETE FROM fury_household_member WHERE household_id = ? AND account_id = ?",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_EVENT_ID_BY_DEDUPE,
        "SELECT id FROM fury_event WHERE dedupe_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_EVENTS_AFTER_ID,
        "SELECT id, event_type, actor_kind, actor_guid, account_id, household_id, "
        "map_id, zone_id, area_id, subject_type, subject_id, source_system, correlation_key, payload "
        "FROM fury_event WHERE id > ? ORDER BY id ASC LIMIT ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_EVENT_TAIL,
        "SELECT id, event_type, actor_kind, actor_guid, account_id, household_id, "
        "map_id, zone_id, area_id, subject_type, subject_id, source_system, correlation_key, payload "
        "FROM fury_event ORDER BY id DESC LIMIT ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_EVENT,
        "INSERT IGNORE INTO fury_event "
        "(event_type, actor_kind, actor_guid, account_id, household_id, map_id, zone_id, area_id, "
        "subject_type, subject_id, source_system, correlation_key, dedupe_key, payload) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_CONSUMER_CHECKPOINT,
        "SELECT last_event_id FROM fury_event_consumer WHERE consumer_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPSERT_CONSUMER_CHECKPOINT,
        "INSERT INTO fury_event_consumer (consumer_key, last_event_id) VALUES (?, ?) "
        "ON DUPLICATE KEY UPDATE last_event_id = GREATEST(last_event_id, VALUES(last_event_id))",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_REWARD_POLICY,
        "SELECT minimum_power_band, maximum_power_band FROM fury_reward_bundle WHERE reward_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_REWARD_CLAIM,
        "SELECT id, status FROM fury_reward_claim "
        "WHERE source_event_id = ? AND reward_key = ? AND beneficiary_kind = ? AND beneficiary_id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_REWARD_CLAIM,
        "INSERT IGNORE INTO fury_reward_claim "
        "(source_event_id, reward_key, beneficiary_kind, beneficiary_id, status) "
        "VALUES (?, ?, ?, ?, ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_REWARD_CLAIM_BY_ID,
        "SELECT id, source_event_id, reward_key, beneficiary_kind, beneficiary_id, status "
        "FROM fury_reward_claim WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_REWARD_CLAIM_TAIL,
        "SELECT id, source_event_id, reward_key, beneficiary_kind, beneficiary_id, status "
        "FROM fury_reward_claim ORDER BY id DESC LIMIT ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PENDING_REWARD_CLAIMS,
        "SELECT id, source_event_id, reward_key, beneficiary_kind, beneficiary_id, status "
        "FROM fury_reward_claim WHERE status = ? ORDER BY id ASC LIMIT ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_REWARD_CLAIM_STATUS,
        "UPDATE fury_reward_claim "
        "SET status = ?, delivered_at = CASE WHEN ? = 1 THEN CURRENT_TIMESTAMP(6) ELSE delivered_at END "
        "WHERE id = ? AND status = 0",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_INS_CHRONICLE_ENTRY,
        "INSERT IGNORE INTO fury_chronicle_entry "
        "(household_id, entry_key, category, title, body, source_event_id, occurred_at, metadata) "
        "SELECT ?, ?, ?, ?, ?, id, occurred_at, ? FROM fury_event WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CHRONICLE_ENTRY_BY_SOURCE,
        "SELECT id FROM fury_chronicle_entry "
        "WHERE household_id = ? AND entry_key = ? AND source_event_id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CHRONICLE_TIMELINE,
        "SELECT id, entry_key, category, title, body, source_event_id, occurred_at, metadata "
        "FROM fury_chronicle_entry WHERE household_id = ? "
        "ORDER BY occurred_at DESC, id DESC LIMIT ?",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_DIAGNOSTIC_COUNTS,
        "SELECT "
        "(SELECT COUNT(*) FROM fury_household), "
        "(SELECT COUNT(*) FROM fury_household_member), "
        "(SELECT COUNT(*) FROM fury_event), "
        "(SELECT COUNT(*) FROM fury_event_consumer), "
        "(SELECT COUNT(*) FROM fury_reward_claim), "
        "(SELECT COUNT(*) FROM fury_chronicle_entry), "
        "(SELECT COUNT(*) FROM ("
        "SELECT household_id FROM fury_household_member "
        "GROUP BY household_id HAVING COUNT(*) > 2"
        ") overfull)",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_CAMPAIGN_NODE,
        "SELECT era, ordinal, display_name, required_power_band, grants_power_band, "
        "ip_required_state, enabled "
        "FROM fury_campaign_node WHERE node_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CAMPAIGN_STATE,
        "SELECT status, source_event_id, revision "
        "FROM fury_campaign_state WHERE household_id = ? AND node_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_POWER_BAND,
        "SELECT current_power_band FROM fury_household WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DERIVED_CAMPAIGN_POWER_BAND,
        "SELECT COALESCE(MAX(n.grants_power_band), 0) "
        "FROM fury_campaign_state s "
        "JOIN fury_campaign_node n ON n.node_key = s.node_key "
        "WHERE s.household_id = ? AND s.status = 4",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_CAMPAIGN_STATE,
        "INSERT IGNORE INTO fury_campaign_state "
        "(household_id, node_key, status, source_event_id, revision) "
        "VALUES (?, ?, ?, ?, 0)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_CAMPAIGN_STATE,
        "UPDATE fury_campaign_state SET "
        "status = ?, "
        "activated_at = CASE WHEN ? = 3 AND activated_at IS NULL THEN CURRENT_TIMESTAMP(6) ELSE activated_at END, "
        "completed_at = CASE WHEN ? = 4 AND completed_at IS NULL THEN CURRENT_TIMESTAMP(6) ELSE completed_at END, "
        "source_event_id = ?, "
        "revision = revision + 1 "
        "WHERE household_id = ? AND node_key = ? AND revision = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_HOUSEHOLD_POWER_BAND,
        "UPDATE fury_household h "
        "JOIN ("
        "SELECT ? AS household_id, COALESCE(MAX(n.grants_power_band), 0) AS derived_band "
        "FROM fury_campaign_state s "
        "JOIN fury_campaign_node n ON n.node_key = s.node_key "
        "WHERE s.household_id = ? AND s.status = 4"
        ") d ON d.household_id = h.id "
        "SET h.current_power_band = d.derived_band, h.revision = h.revision + 1 "
        "WHERE h.current_power_band <> d.derived_band",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_PROOF,
        "SELECT source_event_id, COALESCE(CAST(metadata AS CHAR), '{}') "
        "FROM fury_proof WHERE household_id = ? AND proof_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_PROOF,
        "INSERT IGNORE INTO fury_proof "
        "(household_id, proof_key, source_event_id, metadata) "
        "VALUES (?, ?, ?, ?)",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_CONTRACT_DEFINITION,
        "SELECT board_key, title, campaign_node_key, director_phase, repeat_policy, reward_key, enabled "
        "FROM fury_contract WHERE contract_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_BOARD_DEFINITIONS,
        "SELECT contract_key, title, campaign_node_key, director_phase, repeat_policy, reward_key, enabled "
        "FROM fury_contract WHERE board_key = ? "
        "ORDER BY contract_key ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_ACTIVE_INSTANCE,
        "SELECT id, status, accepted_event_id, completed_event_id, revision "
        "FROM fury_contract_instance "
        "WHERE household_id = ? AND contract_key = ? AND status = 2 "
        "ORDER BY id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_INSTANCE_BY_ID,
        "SELECT household_id, contract_key, status, accepted_event_id, completed_event_id, revision "
        "FROM fury_contract_instance WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_COMPLETED_INSTANCE,
        "SELECT id FROM fury_contract_instance "
        "WHERE household_id = ? AND contract_key = ? AND status = 3 "
        "ORDER BY id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_OBJECTIVE_COUNT,
        "SELECT COUNT(*) FROM fury_contract_objective WHERE contract_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_CONTRACT_INSTANCE,
        "INSERT IGNORE INTO fury_contract_instance "
        "(household_id, contract_key, director_run_id, status, accepted_event_id) "
        "VALUES (?, ?, ?, 2, ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_CONTRACT_PROGRESS_ROWS,
        "INSERT IGNORE INTO fury_contract_progress "
        "(instance_id, objective_ordinal) "
        "SELECT ?, ordinal FROM fury_contract_objective WHERE contract_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_MATCHING_OBJECTIVES,
        "SELECT i.id, i.contract_key, o.ordinal, o.required_count, "
        "p.progress_count, p.last_event_id "
        "FROM fury_contract_objective o FORCE INDEX (ix_fury_contract_objective_match) "
        "JOIN fury_contract_instance i "
        "ON i.contract_key = o.contract_key AND i.household_id = ? AND i.status = 2 "
        "JOIN fury_contract_progress p "
        "ON p.instance_id = i.id AND p.objective_ordinal = o.ordinal "
        "LEFT JOIN fury_director_run d ON d.id = i.director_run_id "
        "WHERE o.event_type = ? "
        "AND (o.subject_type IS NULL OR o.subject_type = ?) "
        "AND (o.subject_id IS NULL OR o.subject_id = ?) "
        "AND i.accepted_event_id <= ? "
        "AND (i.director_run_id IS NULL "
        "OR o.event_type <> 'living_world.entity.killed' "
        "OR d.external_runtime_id = CAST("
        "JSON_UNQUOTE(JSON_EXTRACT(?, '$.runtime_id')) AS UNSIGNED))",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_PROGRESS_ROW,
        "SELECT progress_count, last_event_id "
        "FROM fury_contract_progress "
        "WHERE instance_id = ? AND objective_ordinal = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_CONTRACT_PROGRESS,
        "UPDATE fury_contract_progress p "
        "JOIN fury_contract_instance i "
        "ON i.id = p.instance_id AND i.status = 2 "
        "JOIN fury_contract_objective o "
        "ON o.contract_key = ? AND o.ordinal = p.objective_ordinal "
        "SET p.completed_at = CASE "
        "WHEN p.progress_count + 1 >= o.required_count "
        "THEN COALESCE(p.completed_at, CURRENT_TIMESTAMP(6)) "
        "ELSE p.completed_at END, "
        "p.progress_count = LEAST(o.required_count, p.progress_count + 1), "
        "p.last_event_id = ?, "
        "p.revision = p.revision + 1 "
        "WHERE p.instance_id = ? AND p.objective_ordinal = ? "
        "AND p.last_event_id < ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CONTRACT_INCOMPLETE_COUNT,
        "SELECT COUNT(*) "
        "FROM fury_contract_progress p "
        "JOIN fury_contract_instance i ON i.id = p.instance_id "
        "JOIN fury_contract_objective o "
        "ON o.contract_key = i.contract_key AND o.ordinal = p.objective_ordinal "
        "WHERE p.instance_id = ? AND p.progress_count < o.required_count",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_CONTRACT_COMPLETE,
        "UPDATE fury_contract_instance SET "
        "status = 3, completed_event_id = ?, "
        "completed_at = COALESCE(completed_at, CURRENT_TIMESTAMP(6)), revision = revision + 1 "
        "WHERE id = ? AND status = 2",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_DIRECTOR_LATEST_GRAPH,
        "SELECT id, household_id, graph_key, scope_key, status, phase_key, "
        "external_runtime_id, started_event_id, last_event_id, "
        "resolved_event_id, outcome_key, revision "
        "FROM fury_director_run WHERE household_id = ? AND graph_key = ? "
        "ORDER BY id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_GRAPH,
        "SELECT scope_key, display_name, campaign_node_key, enabled "
        "FROM fury_director_graph WHERE graph_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_RUN_BY_ID,
        "SELECT id, household_id, graph_key, scope_key, status, phase_key, "
        "external_runtime_id, started_event_id, last_event_id, "
        "resolved_event_id, outcome_key, revision "
        "FROM fury_director_run WHERE id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_ACTIVE_SCOPE,
        "SELECT id, household_id, graph_key, scope_key, status, phase_key, "
        "external_runtime_id, started_event_id, last_event_id, "
        "resolved_event_id, outcome_key, revision "
        "FROM fury_director_run "
        "WHERE household_id = ? AND scope_key = ? AND status IN (1, 2, 3) "
        "ORDER BY id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_ACTIVE_RUNS,
        "SELECT id, household_id, graph_key, scope_key, status, phase_key, "
        "external_runtime_id, started_event_id, last_event_id, "
        "resolved_event_id, outcome_key, revision "
        "FROM fury_director_run WHERE status IN (1, 2, 3) ORDER BY id ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_DIRECTOR_RUN,
        "INSERT IGNORE INTO fury_director_run "
        "(household_id, graph_key, scope_key, status, phase_key, "
        "started_event_id, last_event_id) "
        "VALUES (?, ?, ?, 2, ?, ?, ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_DIRECTOR_PHASE,
        "UPDATE fury_director_run SET "
        "status = 2, phase_key = ?, last_event_id = ?, revision = revision + 1 "
        "WHERE id = ? AND household_id = ? AND revision = ? "
        "AND status IN (1, 2)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_DIRECTOR_RUNTIME,
        "UPDATE fury_director_run SET "
        "external_runtime_id = ?, last_event_id = ?, revision = revision + 1 "
        "WHERE id = ? AND household_id = ? AND revision = ? "
        "AND status IN (1, 2, 3) AND external_runtime_id IS NULL",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_DIRECTOR_RESOLVE,
        "UPDATE fury_director_run SET "
        "status = 4, outcome_key = ?, resolved_event_id = ?, last_event_id = ?, "
        "completed_at = COALESCE(completed_at, CURRENT_TIMESTAMP(6)), revision = revision + 1 "
        "WHERE id = ? AND household_id = ? AND revision = ? "
        "AND status IN (1, 2, 3)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_DIRECTOR_ABORT,
        "UPDATE fury_director_run SET "
        "status = 6, outcome_key = ?, resolved_event_id = ?, last_event_id = ?, "
        "completed_at = COALESCE(completed_at, CURRENT_TIMESTAMP(6)), revision = revision + 1 "
        "WHERE id = ? AND household_id = ? AND revision = ? "
        "AND status IN (1, 2, 3)",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_COMPONENT_MATCHES,
        "SELECT component_key, score_value, source_correlation_key "
        "FROM fury_director_score_component "
        "WHERE graph_key = ? AND source_event_type = ? "
        "AND (source_correlation_key IS NULL OR source_correlation_key = ?) "
        "AND enabled = 1 ORDER BY component_key ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_DEFINITION_TOTAL,
        "SELECT COALESCE(SUM(score_value), 0) "
        "FROM fury_director_score_component "
        "WHERE graph_key = ? AND enabled = 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_RUN_FOR_CONTRACT,
        "SELECT i.director_run_id "
        "FROM fury_contract_instance i "
        "JOIN fury_director_run r ON r.id = i.director_run_id "
        "WHERE i.id = ? AND i.household_id = ? "
        "AND i.contract_key = ? AND r.graph_key = ? "
        "AND i.completed_event_id = ? "
        "LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_RUN_FOR_RUNTIME,
        "SELECT id FROM fury_director_run "
        "WHERE household_id = ? AND graph_key = ? "
        "AND external_runtime_id = ? "
        "LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_DIRECTOR_SCORE_AWARD,
        "INSERT IGNORE INTO fury_director_score_award "
        "(director_run_id, graph_key, component_key, score_value, source_event_id) "
        "SELECT ?, ?, ?, ?, ? "
        "FROM fury_director_run r "
        "WHERE r.id = ? AND r.graph_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_AWARD,
        "SELECT graph_key, score_value, source_event_id "
        "FROM fury_director_score_award "
        "WHERE director_run_id = ? AND component_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_DIRECTOR_SCORE_TOTAL,
        "SELECT COALESCE(SUM(score_value), 0) "
        "FROM fury_director_score_award WHERE director_run_id = ?",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_PROFESSION_ORDER,
        "SELECT title, repeat_policy, enabled "
        "FROM fury_profession_order WHERE order_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PROFESSION_ORDER_OPTION,
        "SELECT skill_id, item_id, required_count "
        "FROM fury_profession_order_option "
        "WHERE order_key = ? AND ordinal = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PROFESSION_ORDER_ACTIVE,
        "SELECT i.id, i.household_id, i.order_key, i.option_ordinal, "
        "i.status, i.progress_count, i.accepted_event_id, i.last_event_id, "
        "i.completed_event_id, i.revision, "
        "o.skill_id, o.item_id, o.required_count "
        "FROM fury_profession_order_instance i "
        "JOIN fury_profession_order_option o "
        "ON o.order_key = i.order_key AND o.ordinal = i.option_ordinal "
        "WHERE i.household_id = ? AND i.order_key = ? AND i.status = 2 "
        "ORDER BY i.id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PROFESSION_ORDER_INSTANCE,
        "SELECT i.id, i.household_id, i.order_key, i.option_ordinal, "
        "i.status, i.progress_count, i.accepted_event_id, i.last_event_id, "
        "i.completed_event_id, i.revision, "
        "o.skill_id, o.item_id, o.required_count "
        "FROM fury_profession_order_instance i "
        "JOIN fury_profession_order_option o "
        "ON o.order_key = i.order_key AND o.ordinal = i.option_ordinal "
        "WHERE i.id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PROFESSION_ORDER_COMPLETED,
        "SELECT id FROM fury_profession_order_instance "
        "WHERE household_id = ? AND order_key = ? AND status = 3 "
        "ORDER BY id DESC LIMIT 1",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_INS_PROFESSION_ORDER_INSTANCE,
        "INSERT IGNORE INTO fury_profession_order_instance "
        "(household_id, order_key, option_ordinal, status, "
        "accepted_event_id, last_event_id) "
        "VALUES (?, ?, ?, 2, ?, ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_PROFESSION_ORDER_MATCHES,
        "SELECT i.id, i.household_id, i.order_key, i.option_ordinal, "
        "i.status, i.progress_count, i.accepted_event_id, i.last_event_id, "
        "i.completed_event_id, i.revision, "
        "o.skill_id, o.item_id, o.required_count "
        "FROM fury_profession_order_option o "
        "FORCE INDEX (ix_fury_profession_order_option_target) "
        "JOIN fury_profession_order_instance i "
        "ON i.order_key = o.order_key AND i.option_ordinal = o.ordinal "
        "AND i.household_id = ? AND i.status = 2 "
        "WHERE o.skill_id = ? AND o.item_id = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_PROFESSION_ORDER_PROGRESS,
        "UPDATE fury_profession_order_instance i "
        "JOIN fury_profession_order_option o "
        "ON o.order_key = i.order_key AND o.ordinal = i.option_ordinal "
        "SET i.progress_count = LEAST(o.required_count, i.progress_count + 1), "
        "i.last_event_id = ?, i.revision = i.revision + 1 "
        "WHERE i.id = ? AND i.household_id = ? AND i.status = 2 "
        "AND i.last_event_id < ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_PROFESSION_ORDER_COMPLETE,
        "UPDATE fury_profession_order_instance SET "
        "status = 3, completed_event_id = ?, "
        "completed_at = COALESCE(completed_at, CURRENT_TIMESTAMP(6)), "
        "revision = revision + 1 "
        "WHERE id = ? AND household_id = ? AND status = 2",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_BESTIARY_MAPPINGS,
        "SELECT m.entry_key, m.discovery_level "
        "FROM fury_bestiary_creature_map m FORCE INDEX (ix_fury_bestiary_creature_map_lookup) "
        "JOIN fury_bestiary_entry e ON e.entry_key = m.entry_key AND e.enabled = 1 "
        "WHERE m.creature_entry = ? AND m.enabled = 1 "
        "ORDER BY m.entry_key ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_BESTIARY_EVENT_MAPPINGS,
        "SELECT m.entry_key, m.discovery_level "
        "FROM fury_bestiary_event_map m "
        "FORCE INDEX (ix_fury_bestiary_event_map_lookup) "
        "JOIN fury_bestiary_entry e "
        "ON e.entry_key = m.entry_key AND e.enabled = 1 "
        "WHERE m.event_type = ? AND m.subject_type = ? "
        "AND m.subject_id = ? AND m.enabled = 1 "
        "ORDER BY m.entry_key ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_BESTIARY_HOUSEHOLD_ACCOUNTS_AT_LEVEL,
        "SELECT s.account_id "
        "FROM fury_bestiary_state s "
        "JOIN fury_household_member h ON h.account_id = s.account_id "
        "WHERE h.household_id = ? AND s.entry_key = ? "
        "AND s.discovery_level >= ? "
        "ORDER BY s.account_id ASC",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_BESTIARY_STATE,
        "SELECT discovery_level, kill_count, first_event_id, last_event_id, revision "
        "FROM fury_bestiary_state WHERE account_id = ? AND entry_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPSERT_BESTIARY_KILL,
        "INSERT INTO fury_bestiary_state "
        "(account_id, entry_key, discovery_level, kill_count, first_event_id, last_event_id, revision) "
        "VALUES (?, ?, ?, 1, ?, ?, 0) "
        "ON DUPLICATE KEY UPDATE "
        "discovery_level = IF(last_event_id < VALUES(last_event_id), "
        "GREATEST(discovery_level, VALUES(discovery_level)), discovery_level), "
        "kill_count = IF(last_event_id < VALUES(last_event_id), kill_count + 1, kill_count), "
        "revision = IF(last_event_id < VALUES(last_event_id), revision + 1, revision), "
        "last_event_id = GREATEST(last_event_id, VALUES(last_event_id))",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPSERT_BESTIARY_LEVEL,
        "INSERT INTO fury_bestiary_state "
        "(account_id, entry_key, discovery_level, kill_count, first_event_id, last_event_id, revision) "
        "VALUES (?, ?, ?, 0, ?, ?, 0) "
        "ON DUPLICATE KEY UPDATE "
        "discovery_level = IF(last_event_id < VALUES(last_event_id), "
        "GREATEST(discovery_level, VALUES(discovery_level)), discovery_level), "
        "revision = IF(last_event_id < VALUES(last_event_id), revision + 1, revision), "
        "last_event_id = GREATEST(last_event_id, VALUES(last_event_id))",
        CONNECTION_SYNCH);
}
}
