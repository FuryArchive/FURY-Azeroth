#ifndef MOD_FURY_DATABASE_H
#define MOD_FURY_DATABASE_H

#include "DatabaseEnvFwd.h"
#include "ModuleDatabasePool.h"
#include "MySQLConnection.h"
#include "PreparedStatement.h"
#include "Transaction.h"

#include <memory>

namespace Fury
{
enum DatabaseStatements : uint32
{
    FURY_SEL_HOUSEHOLD_BY_ID,
    FURY_SEL_HOUSEHOLD_BY_SLUG,
    FURY_SEL_HOUSEHOLD_BY_ACCOUNT,
    FURY_SEL_HOUSEHOLD_MEMBERS,
    FURY_SEL_HOUSEHOLD_MEMBER_COUNT,
    FURY_INS_HOUSEHOLD,
    FURY_INS_HOUSEHOLD_MEMBER,
    FURY_DEL_HOUSEHOLD_MEMBER,

    FURY_SEL_EVENT_ID_BY_DEDUPE,
    FURY_SEL_EVENTS_AFTER_ID,
    FURY_SEL_EVENT_TAIL,
    FURY_INS_EVENT,

    FURY_SEL_CONSUMER_CHECKPOINT,
    FURY_UPSERT_CONSUMER_CHECKPOINT,

    FURY_SEL_REWARD_POLICY,
    FURY_SEL_REWARD_CLAIM,
    FURY_INS_REWARD_CLAIM,
    FURY_SEL_REWARD_CLAIM_TAIL,

    FURY_INS_CHRONICLE_ENTRY,
    FURY_SEL_CHRONICLE_TIMELINE,

    FURY_SEL_DIAGNOSTIC_COUNTS,

    MAX_FURY_DATABASE_STATEMENTS
};

class DatabaseConnection final : public MySQLConnection
{
public:
    using Statements = DatabaseStatements;

    explicit DatabaseConnection(MySQLConnectionInfo& connInfo);
    ~DatabaseConnection() override;

    void DoPrepareStatements() override;
};

using DatabasePreparedStatement = PreparedStatement<DatabaseConnection>;
using DatabaseTransaction = std::shared_ptr<Transaction<DatabaseConnection>>;

class DatabasePool final : public ModuleDatabasePool
{
public:
    DatabasePreparedStatement* GetPreparedStatement(DatabaseStatements index)
    {
        return new DatabasePreparedStatement(index, GetPreparedStatementParamCount(index));
    }

    using ModuleDatabasePool::Execute;
    using ModuleDatabasePool::Query;

    DatabaseTransaction BeginTransaction()
    {
        return std::make_shared<Transaction<DatabaseConnection>>();
    }

    void CommitTransaction(DatabaseTransaction transaction)
    {
        DirectCommitTransaction(transaction);
    }

    void WarnAboutSyncQueries([[maybe_unused]] bool apply) {}

    [[nodiscard]] std::size_t QueueSize() const { return 0; }

protected:
    MySQLConnection* CreateConnection(MySQLConnectionInfo& connInfo) override
    {
        return new DatabaseConnection(connInfo);
    }
};

extern DatabasePool FuryDatabase;
}

#endif
