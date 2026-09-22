#ifndef MOD_FURY_DIAGNOSTICS_TYPES_H
#define MOD_FURY_DIAGNOSTICS_TYPES_H

#include "Define.h"

#include <string>
#include <vector>

namespace Fury
{
struct KernelSnapshot
{
    uint64 households = 0;
    uint64 householdMembers = 0;
    uint64 events = 0;
    uint64 consumers = 0;
    uint64 rewardClaims = 0;
    uint64 chronicleEntries = 0;
};

enum class ValidationSeverity : uint8
{
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4
};

struct ValidationIssue
{
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string subsystem;
    std::string key;
    std::string message;
};

struct ValidationReport
{
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool IsHealthy() const
    {
        for (ValidationIssue const& issue : issues)
        {
            if (issue.severity == ValidationSeverity::Error ||
                issue.severity == ValidationSeverity::Fatal)
            {
                return false;
            }
        }

        return true;
    }
};
}

#endif
