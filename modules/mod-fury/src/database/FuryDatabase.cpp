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
    PrepareStatement(FURY_SEL_REWARD_CLAIM_TAIL,
        "SELECT id, source_event_id, reward_key, beneficiary_kind, beneficiary_id, status "
        "FROM fury_reward_claim ORDER BY id DESC LIMIT ?",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_INS_CHRONICLE_ENTRY,
        "INSERT IGNORE INTO fury_chronicle_entry "
        "(household_id, entry_key, category, title, body, source_event_id, occurred_at, metadata) "
        "SELECT ?, ?, ?, ?, ?, id, occurred_at, ? FROM fury_event WHERE id = ?",
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
        "SELECT era, ordinal, display_name, required_power_band, grants_power_band, enabled "
        "FROM fury_campaign_node WHERE node_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_CAMPAIGN_STATE,
        "SELECT status, source_event_id, revision "
        "FROM fury_campaign_state WHERE household_id = ? AND node_key = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_SEL_HOUSEHOLD_POWER_BAND,
        "SELECT current_power_band FROM fury_household WHERE id = ?",
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
        "completed_at = CASE WHEN ? = 4 THEN CURRENT_TIMESTAMP(6) ELSE completed_at END, "
        "source_event_id = ?, "
        "revision = revision + 1 "
        "WHERE household_id = ? AND node_key = ? AND revision = ?",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_RECALC_HOUSEHOLD_POWER_BAND,
        "UPDATE fury_household h SET "
        "current_power_band = COALESCE(("
        "SELECT MAX(n.grants_power_band) "
        "FROM fury_campaign_state s "
        "JOIN fury_campaign_node n ON n.node_key = s.node_key "
        "WHERE s.household_id = h.id AND s.status = 4"
        "), 0), "
        "revision = revision + 1 "
        "WHERE h.id = ?",
        CONNECTION_SYNCH);

    PrepareStatement(FURY_SEL_PROOF,
        "SELECT source_event_id FROM fury_proof "
        "WHERE household_id = ? AND proof_key = ?",
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
    PrepareStatement(FURY_SEL_CONTRACT_ACTIVE_INSTANCE,
        "SELECT id, status, accepted_event_id, completed_event_id, revision "
        "FROM fury_contract_instance "
        "WHERE household_id = ? AND contract_key = ? AND status = 2 "
        "ORDER BY id DESC LIMIT 1",
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
        "FROM fury_contract_instance i "
        "JOIN fury_contract_objective o ON o.contract_key = i.contract_key "
        "JOIN fury_contract_progress p ON p.instance_id = i.id "
        "AND p.objective_ordinal = o.ordinal "
        "WHERE i.household_id = ? AND i.status = 2 "
        "AND o.event_type = ? "
        "AND (o.subject_type IS NULL OR o.subject_type = ?) "
        "AND (o.subject_id IS NULL OR o.subject_id = ?)",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_CONTRACT_PROGRESS,
        "UPDATE fury_contract_progress p "
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
        "ON o.contract_key = i.contract_key "
        "AND o.ordinal = p.objective_ordinal "
        "WHERE p.instance_id = ? AND p.progress_count < o.required_count",
        CONNECTION_SYNCH);
    PrepareStatement(FURY_UPD_CONTRACT_COMPLETE,
        "UPDATE fury_contract_instance SET "
        "status = 3, completed_event_id = ?, "
        "completed_at = CURRENT_TIMESTAMP(6), revision = revision + 1 "
        "WHERE id = ? AND status = 2",
        CONNECTION_SYNCH);
}
}
