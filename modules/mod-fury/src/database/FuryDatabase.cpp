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
        "(SELECT COUNT(*) FROM fury_chronicle_entry)",
        CONNECTION_SYNCH);
}
}
