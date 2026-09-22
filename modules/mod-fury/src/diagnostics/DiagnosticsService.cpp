#include "DiagnosticsService.h"

#include "core/FuryApp.h"

namespace Fury
{
DiagnosticsService::DiagnosticsService(
    App const& app,
    DiagnosticsRepository const& repository)
    : _app(app),
      _repository(repository)
{
}

std::optional<KernelSnapshot> DiagnosticsService::Snapshot() const
{
    return _repository.Snapshot();
}

ValidationReport DiagnosticsService::Validate() const
{
    ValidationReport report;

    if (!_app.IsInitialized())
    {
        report.issues.push_back({
            ValidationSeverity::Fatal,
            "core",
            "app.initialized",
            "FuryApp is not initialized."
        });
        return report;
    }

    if (!_app.IsEnabled())
    {
        report.issues.push_back({
            ValidationSeverity::Warning,
            "core",
            "app.enabled",
            "FURY is disabled by configuration."
        });
        return report;
    }

    std::optional<KernelSnapshot> snapshot = _repository.Snapshot();
    if (!snapshot)
    {
        report.issues.push_back({
            ValidationSeverity::Fatal,
            "database",
            "kernel.tables",
            "Could not query required FURY kernel tables."
        });
        return report;
    }

    if (snapshot->overfullHouseholds != 0)
    {
        report.issues.push_back({
            ValidationSeverity::Error,
            "household",
            "member.limit",
            "Household member count exceeds the M1 two-account policy."
        });
    }

    report.issues.push_back({
        ValidationSeverity::Info,
        "core",
        "kernel",
        "FURY kernel tables are queryable."
    });

    return report;
}
}
