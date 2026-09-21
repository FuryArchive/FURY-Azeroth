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
    PrepareStatement(FURY_INS_EVENT,
        "INSERT IGNORE INTO fury_event "
        "(event_type, actor_kind, actor_guid, account_id, household_id, map_id, zone_id, area_id, "
        "subject_type, subject_id, source_system, correlation_key, dedupe_key, payload) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        CONNECTION_SYNCH);
}
}
