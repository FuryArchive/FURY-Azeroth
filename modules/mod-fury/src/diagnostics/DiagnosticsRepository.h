#ifndef MOD_FURY_DIAGNOSTICS_REPOSITORY_H
#define MOD_FURY_DIAGNOSTICS_REPOSITORY_H

#include "DiagnosticsTypes.h"

#include <optional>

namespace Fury
{
class DiagnosticsRepository final
{
public:
    [[nodiscard]] std::optional<KernelSnapshot> Snapshot() const;
};
}

#endif
