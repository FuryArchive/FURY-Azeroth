#include "ChronicleService.h"

#include "StringFormat.h"

namespace Fury
{
ChronicleService::ChronicleService(ChronicleRepository const& repository)
    : _repository(repository)
{
}

bool ChronicleService::Handle(FuryEvent const& event)
{
    if (!event.actor.householdId)
        return true;

    std::string entryKey;
    std::string category;
    std::string title;

    if (event.type == "campaign.node.completed")
    {
        entryKey = event.correlationKey.empty()
            ? "campaign.completed"
            : event.correlationKey;
        category = "campaign";
        title = "Campaign milestone completed";
    }
    else if (event.type == "contract.completed")
    {
        entryKey = event.correlationKey.empty()
            ? "contract.completed"
            : event.correlationKey;
        category = "contract";
        title = "Contract completed";
    }
    else if (event.type == "defias.resolution.success")
    {
        entryKey = "classic.westfall.defias.resolution.success";
        category = "world";
        title = "Westfall defended";
    }
    else if (event.type == "defias.resolution.partial")
    {
        entryKey = "classic.westfall.defias.resolution.partial";
        category = "world";
        title = "Westfall bloodied";
    }
    else if (event.type == "defias.resolution.ignored")
    {
        entryKey = "classic.westfall.defias.resolution.ignored";
        category = "world";
        title = "Westfall crisis ignored";
    }
    else if (event.type == "director.run.resolved")
    {
        // Defias emits a richer player-facing outcome event in T32. Avoid
        // recording both the generic Director entry and the specific result.
        if (event.correlationKey ==
            "classic.westfall.defias_resurgence.v1")
        {
            return true;
        }

        entryKey = event.correlationKey.empty()
            ? "director.resolved"
            : event.correlationKey;
        category = "world";
        title = "World crisis resolved";
    }
    else if (event.type == "proof.granted")
    {
        entryKey = event.correlationKey.empty()
            ? "proof.granted"
            : event.correlationKey;
        category = "milestone";
        title = "New milestone recorded";
    }
    else
    {
        // Chronicle is intentionally sparse. Most machine events are not
        // player-facing history entries.
        return true;
    }

    return Record(
        event,
        entryKey,
        category,
        title,
        {},
        event.payloadJson);
}

bool ChronicleService::Record(
    FuryEvent const& event,
    std::string_view entryKey,
    std::string_view category,
    std::string_view title,
    std::string_view body,
    std::string_view metadataJson) const
{
    if (!event.id ||
        !event.actor.householdId ||
        entryKey.empty() ||
        category.empty() ||
        title.empty())
    {
        return false;
    }

    return _repository.Insert(
        *event.actor.householdId,
        event.id,
        entryKey,
        category,
        title,
        body,
        metadataJson.empty() ? event.payloadJson : metadataJson);
}

std::vector<ChronicleEntry> ChronicleService::Timeline(
    HouseholdId householdId,
    uint32 limit) const
{
    return _repository.Timeline(householdId, limit);
}
}
