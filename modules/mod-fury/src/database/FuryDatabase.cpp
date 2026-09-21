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
}
}
