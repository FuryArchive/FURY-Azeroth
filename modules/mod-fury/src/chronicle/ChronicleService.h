#ifndef MOD_FURY_CHRONICLE_SERVICE_H
#define MOD_FURY_CHRONICLE_SERVICE_H

#include "ChronicleRepository.h"
#include "events/EventConsumer.h"

namespace Fury
{
class ChronicleService final : public EventConsumer
{
public:
    explicit ChronicleService(ChronicleRepository const& repository);

    [[nodiscard]] std::string_view Key() const override
    {
        return "chronicle";
    }

    bool Handle(FuryEvent const& event) override;

    void Record(
        FuryEvent const& event,
        std::string_view entryKey,
        std::string_view category,
        std::string_view title,
        std::string_view body = {},
        std::string_view metadataJson = {}) const;

    [[nodiscard]] std::vector<ChronicleEntry> Timeline(
        HouseholdId householdId,
        uint32 limit = 100) const;

private:
    ChronicleRepository const& _repository;
};
}

#endif
