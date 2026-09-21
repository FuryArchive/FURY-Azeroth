#include "ScriptMgr.h"

#include "core/FuryApp.h"

void AddFuryDatabaseScripts();

namespace
{
class FuryWorldScript final : public WorldScript
{
public:
    FuryWorldScript()
        : WorldScript(
              "FuryWorldScript",
              {
                  WORLDHOOK_ON_STARTUP,
                  WORLDHOOK_ON_UPDATE,
                  WORLDHOOK_ON_SHUTDOWN
              })
    {
    }

    void OnStartup() override
    {
        Fury::App::Instance().Initialize();
    }

    void OnUpdate(uint32 diff) override
    {
        Fury::App::Instance().Update(diff);
    }

    void OnShutdown() override
    {
        Fury::App::Instance().Shutdown();
    }
};
}

void AddFuryScripts()
{
    AddFuryDatabaseScripts();
    new FuryWorldScript();
}
