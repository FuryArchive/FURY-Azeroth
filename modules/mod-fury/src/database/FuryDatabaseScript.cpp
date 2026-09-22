#include "FuryDatabase.h"

#include "BuiltInConfig.h"
#include "Config.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ScriptMgr.h"

#include <mysqld_error.h>

namespace
{
class FuryDatabaseScript final : public DatabaseScript
{
public:
    FuryDatabaseScript()
        : DatabaseScript("FuryDatabaseScript")
    {
    }

    bool OnModuleDatabasesLoading() override
    {
        if (!sConfigMgr->GetOption<bool>("Fury.Enable", true))
        {
            LOG_INFO("server.loading", "[FURY] module disabled; skipping FURY database startup.");
            return true;
        }

        std::string const dbString = sConfigMgr->GetOption<std::string>("FuryDatabaseInfo", "");
        if (dbString.empty())
        {
            LOG_ERROR("server.loading", "[FURY] FuryDatabaseInfo is empty.");
            return false;
        }

        uint8 const synchThreads = sConfigMgr->GetOption<uint8>("FuryDatabase.SynchThreads", 1);
        Fury::FuryDatabase.SetConnectionInfo(dbString, synchThreads);

        bool const updatesEnabled = sConfigMgr->GetOption<bool>("Fury.Updates.EnableDatabases", true);
        if (updatesEnabled && !DBUpdaterUtil::CheckExecutable())
            return false;

        uint32 error = Fury::FuryDatabase.Open();
        if (error == ER_BAD_DB_ERROR && updatesEnabled)
        {
            if (!ModuleDBUpdater::Create(Fury::FuryDatabase))
                return false;

            error = Fury::FuryDatabase.Open();
        }

        if (error)
        {
            LOG_ERROR("server.loading", "[FURY] cannot connect to FURY database, error {}.", error);
            return false;
        }

        if (updatesEnabled)
        {
            std::string moduleRoot =
                sConfigMgr->GetOption<std::string>("Fury.Database.SourceDirectory", "");

            if (moduleRoot.empty())
                moduleRoot = BuiltInConfig::GetSourceDirectory() + "/modules/mod-fury";
            DBUpdaterInfo const info = {
                "FURY",
                moduleRoot,
                moduleRoot + "/data/sql/fury/base/",
                "fury"
            };

            if (!ModuleDBUpdater::Populate(Fury::FuryDatabase, info))
            {
                LOG_ERROR("server.loading", "[FURY] could not populate FURY database.");
                return false;
            }

            if (!ModuleDBUpdater::Update(Fury::FuryDatabase, info))
            {
                LOG_ERROR("server.loading", "[FURY] could not update FURY database.");
                return false;
            }
        }

        if (!Fury::FuryDatabase.PrepareStatements())
        {
            LOG_ERROR("server.loading", "[FURY] could not prepare FURY database statements.");
            return false;
        }

        _opened = true;
        LOG_INFO("server.loading", "[FURY] FURY database ready.");
        return true;
    }

    void OnModuleDatabasesKeepAlive() override
    {
        if (_opened)
            Fury::FuryDatabase.KeepAlive();
    }

    void OnModuleDatabasesClosing() override
    {
        if (!_opened)
            return;

        Fury::FuryDatabase.Close();
        _opened = false;
    }

    void OnDatabaseWarnAboutSyncQueries(bool apply) override
    {
        Fury::FuryDatabase.WarnAboutSyncQueries(apply);
    }

    void OnDatabaseGetDBRevision(std::map<std::string, std::string>& revisions) override
    {
        if (!_opened)
        {
            revisions["FURY"] = sConfigMgr->GetOption<bool>("Fury.Enable", true)
                ? "FURY database unavailable"
                : "FURY disabled";
            return;
        }

        std::string revision;
        if (QueryResult result = Fury::FuryDatabase.Query(
                "SELECT date FROM version_db_fury ORDER BY date DESC, sql_rev DESC LIMIT 1"))
        {
            Field* fields = result->Fetch();
            revision = fields[0].Get<std::string>();
        }

        if (revision.empty())
            revision = "Unknown FURY Database Revision";

        revisions["FURY"] = revision;
    }

private:
    bool _opened = false;
};
}

void AddFuryDatabaseScripts()
{
    new FuryDatabaseScript();
}
