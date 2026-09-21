#ifndef MOD_FURY_APP_H
#define MOD_FURY_APP_H

#include "Define.h"

namespace Fury
{
class App final
{
public:
    static App& Instance();

    void Initialize();
    void Update(uint32 diff);
    void Shutdown();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsInitialized() const { return _initialized; }

private:
    App() = default;

    bool _enabled = false;
    bool _initialized = false;
};
}

#endif
