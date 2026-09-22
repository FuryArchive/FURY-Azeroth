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

    switch (_app.IndividualProgression().Availability())
    {
        case IndividualProgressionAvailability::Unavailable:
            report.issues.push_back({
                ValidationSeverity::Error,
                "individual_progression",
                "module.available",
                "Individual Progression adapter cannot resolve the pinned module API."
            });
            break;
        case IndividualProgressionAvailability::Disabled:
            report.issues.push_back({
                ValidationSeverity::Error,
                "individual_progression",
                "module.enabled",
                "IndividualProgression.Enable must be 1 for FURY character gates."
            });
            break;
        case IndividualProgressionAvailability::PlayerSettingsDisabled:
            report.issues.push_back({
                ValidationSeverity::Error,
                "individual_progression",
                "player_settings.enabled",
                "EnablePlayerSettings must be 1 for Individual Progression persistence."
            });
            break;
        case IndividualProgressionAvailability::Available:
            report.issues.push_back({
                ValidationSeverity::Info,
                "individual_progression",
                "adapter",
                "Individual Progression read-only adapter is available."
            });
            break;
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
