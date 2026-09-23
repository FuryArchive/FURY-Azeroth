#ifndef MOD_FURY_DEFIAS_SERVICE_H
#define MOD_FURY_DEFIAS_SERVICE_H

#include "DefiasGraph.h"
#include "DefiasPresence.h"
#include "DefiasContent.h"
#include "events/EventConsumer.h"
#include <chrono>
#include <unordered_map>

namespace Fury
{
class DirectorService;
class DirectorRepository;
class CampaignService;
class ActorResolver;
class EventStore;
}

namespace Fury::Defias
{
class Service final : public EventConsumer, private GraphPort
{
public:
    Service(DirectorService& director, DirectorRepository const& repository,
        CampaignService& campaign, LivingWorldAdapter& livingWorld,
        ContentService const& content, EventStore const& events, ActorResolver& actors);
    void Initialize();
    void Reset();
    void Tick();
    std::string_view Key() const override { return "defias.graph.v1"; }
    bool Handle(FuryEvent const& event) override;
    bool Enabled() const { return _enabled && _content.IsValid(); }

private:
    bool ContentValid() const override { return Enabled(); }
    bool CampaignComplete(HouseholdId household) override;
    std::optional<DirectorRun> Find(HouseholdId household) override;
    DirectorResult Start(FuryEvent const& source) override;
    bool Advance(FuryEvent const& source, DirectorRun const& run, std::string_view phase) override;
    std::optional<uint64> StartExternal(FuryEvent const& source) override;
    bool Attach(FuryEvent const& source, DirectorRun const& run, uint64 runtime) override;
    bool RequestActivation(FuryEvent source, DirectorRun const& run);

    DirectorService& _director;
    DirectorRepository const& _repository;
    CampaignService& _campaign;
    LivingWorldAdapter& _livingWorld;
    ContentService const& _content;
    EventStore const& _events;
    ActorResolver& _actors;
    Graph _graph;
    PresenceTracker _presence;
    bool _enabled = false;
    bool _contractActivation = true;
    bool _presenceActivation = true;
    uint32 _minimumLevel = 10;
    uint32 _presenceSeconds = 120;
    std::string _activationContract;
};
}
#endif
