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
}
}
