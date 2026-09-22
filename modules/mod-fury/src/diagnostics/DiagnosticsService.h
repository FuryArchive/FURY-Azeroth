#ifndef MOD_FURY_DIAGNOSTICS_SERVICE_H
#define MOD_FURY_DIAGNOSTICS_SERVICE_H

#include "DiagnosticsRepository.h"

namespace Fury
{
class App;

class DiagnosticsService final
{
public:
    DiagnosticsService(
        App const& app,
        DiagnosticsRepository const& repository);

    [[nodiscard]] std::optional<KernelSnapshot> Snapshot() const;
    [[nodiscard]] ValidationReport Validate() const;

private:
    App const& _app;
    DiagnosticsRepository const& _repository;
};
}

#endif
