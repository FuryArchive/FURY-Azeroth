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
    FURY_SEL_REWARD_CLAIM_BY_ID,
    FURY_SEL_REWARD_CLAIM_TAIL,
    FURY_SEL_PENDING_REWARD_CLAIMS,
    FURY_UPD_REWARD_CLAIM_STATUS,

    FURY_INS_CHRONICLE_ENTRY,
    FURY_SEL_CHRONICLE_ENTRY_BY_SOURCE,
    FURY_SEL_CHRONICLE_TIMELINE,

    FURY_SEL_DIAGNOSTIC_COUNTS,

    FURY_SEL_CAMPAIGN_NODE,
    FURY_SEL_CAMPAIGN_STATE,
    FURY_SEL_HOUSEHOLD_POWER_BAND,
    FURY_SEL_DERIVED_CAMPAIGN_POWER_BAND,
    FURY_INS_CAMPAIGN_STATE,
    FURY_UPD_CAMPAIGN_STATE,
    FURY_UPD_HOUSEHOLD_POWER_BAND,

    FURY_SEL_PROOF,
    FURY_INS_PROOF,

    FURY_SEL_CONTRACT_DEFINITION,
    FURY_SEL_CONTRACT_ACTIVE_INSTANCE,
    FURY_SEL_CONTRACT_INSTANCE_BY_ID,
    FURY_SEL_CONTRACT_COMPLETED_INSTANCE,
    FURY_SEL_CONTRACT_OBJECTIVE_COUNT,
    FURY_INS_CONTRACT_INSTANCE,
    FURY_INS_CONTRACT_PROGRESS_ROWS,
    FURY_SEL_CONTRACT_MATCHING_OBJECTIVES,
    FURY_SEL_CONTRACT_PROGRESS_ROW,
    FURY_UPD_CONTRACT_PROGRESS,
    FURY_SEL_CONTRACT_INCOMPLETE_COUNT,
    FURY_UPD_CONTRACT_COMPLETE,

    FURY_SEL_DIRECTOR_LATEST_GRAPH,
    FURY_SEL_DIRECTOR_GRAPH,
    FURY_SEL_DIRECTOR_RUN_BY_ID,
    FURY_SEL_DIRECTOR_ACTIVE_SCOPE,
    FURY_SEL_DIRECTOR_ACTIVE_RUNS,
    FURY_INS_DIRECTOR_RUN,
    FURY_UPD_DIRECTOR_PHASE,
    FURY_UPD_DIRECTOR_RUNTIME,
    FURY_UPD_DIRECTOR_RESOLVE,
    FURY_UPD_DIRECTOR_ABORT,

    FURY_SEL_PROFESSION_ORDER,
    FURY_SEL_PROFESSION_ORDER_OPTION,
    FURY_SEL_PROFESSION_ORDER_ACTIVE,
    FURY_SEL_PROFESSION_ORDER_INSTANCE,
    FURY_SEL_PROFESSION_ORDER_COMPLETED,
    FURY_INS_PROFESSION_ORDER_INSTANCE,
    FURY_SEL_PROFESSION_ORDER_MATCHES,
    FURY_UPD_PROFESSION_ORDER_PROGRESS,
    FURY_UPD_PROFESSION_ORDER_COMPLETE,

    FURY_SEL_BESTIARY_MAPPINGS,
    FURY_SEL_BESTIARY_STATE,
    FURY_UPSERT_BESTIARY_KILL,
    FURY_UPSERT_BESTIARY_LEVEL,

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
