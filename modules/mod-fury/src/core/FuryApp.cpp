#include "FuryApp.h"

#include "Config.h"
#include "Log.h"

namespace Fury
{
App& App::Instance()
{
    static App instance;
    return instance;
}

void App::Initialize()
{
    if (_initialized)
        return;

    _enabled = sConfigMgr->GetOption<bool>("Fury.Enable", true);
    _initialized = true;

    LOG_INFO("server.loading", "[FURY] mod-fury initialized (enabled={}).", _enabled ? "true" : "false");
}

void App::Update(uint32 /*diff*/)
{
    if (!_initialized || !_enabled)
        return;
}

void App::Shutdown()
{
    if (!_initialized)
        return;

    LOG_INFO("server.loading", "[FURY] mod-fury shutdown.");

    _enabled = false;
    _initialized = false;
}
}
