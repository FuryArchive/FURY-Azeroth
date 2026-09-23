#include "DefiasService.h"
#include "actors/ActorResolver.h"
#include "campaign/CampaignService.h"
#include "director/DirectorService.h"
#include "events/EventStore.h"
#include "Config.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringFormat.h"
#include <boost/bind/placeholders.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>
#include <algorithm>

namespace Fury::Defias
{
namespace
{
std::optional<uint32> Number(std::string const& json, char const* field)
{
    try
    {
        boost::property_tree::ptree tree;
        std::istringstream stream(json);
        boost::property_tree::read_json(stream, tree);
        auto value = tree.get_optional<uint32>(field);
        if (value) return *value;
    }
    catch (boost::property_tree::ptree_error const&) { }
    return std::nullopt;
}
}

Service::Service(DirectorService& director, DirectorRepository const& repository,
    CampaignService& campaign, LivingWorldAdapter& livingWorld,
    ContentService const& content, EventStore const& events, ActorResolver& actors)
    : _director(director), _repository(repository), _campaign(campaign),
      _livingWorld(livingWorld), _content(content), _events(events),
      _actors(actors), _graph(*this) { }

void Service::Initialize()
{
    Reset();
    _enabled = sConfigMgr->GetOption<bool>("Fury.Defias.Enable", true);
    _minimumLevel = std::clamp(sConfigMgr->GetOption<uint32>("Fury.Defias.MinimumLevel", 10), 10u, 80u);
    _presenceSeconds = std::max(1u, sConfigMgr->GetOption<uint32>("Fury.Defias.PresenceSeconds", 120));
    std::string mode = sConfigMgr->GetOption<std::string>("Fury.Defias.Activation", "contract_or_presence");
    _contractActivation = mode == "contract" || mode == "contract_or_presence";
    _presenceActivation = mode == "presence" || mode == "contract_or_presence";
    _activationContract = sConfigMgr->GetOption<std::string>(
        "Fury.Defias.ActivationContract", "classic.westfall.defias.scout_report");
    if ((!_contractActivation && !_presenceActivation) ||
        !_livingWorld.ManageGraph(GraphKey, InvasionId))
    {
        _enabled = false;
        LOG_ERROR("server.loading", "[FURY] Defias graph disabled: invalid activation mode or graph mapping.");
    }
    LOG_INFO("server.loading", "[FURY] Defias graph enabled={}, minimum level={}, activation={}, dwell={}s.",
        Enabled(), _minimumLevel, mode, _presenceSeconds);
}

void Service::Reset()
{
    _presence.Reset();
    _enabled = false;
}

bool Service::Handle(FuryEvent const& event)
{
    // Keep a queued durable activation behind the checkpoint while content is
    // unavailable; otherwise an already persisted invasion intent can be lost.
    if (!Enabled()) return event.type != "defias.activation.requested";
    if (event.type == "player.zone.changed" || event.type == "player.login" || event.type == "player.level.changed" || event.type == "player.logout")
    {
        if (event.sourceSystem != "azerothcore") return true;
        uint32 level = Number(event.payloadJson, "actor_level").value_or(0);
        if (event.type != "player.logout" && !_graph.Enter(event, level, _minimumLevel)) return false;
        uint64 guid = event.actor.characterGuid.GetRawValue();
        if (!guid) return true;
        bool eligible = _presenceActivation && Graph::Eligible(event, level, _minimumLevel);
        if (eligible)
        {
            auto run = Find(*event.actor.householdId);
            eligible = run && run->phaseKey == "rumours" && run->status == DirectorRunStatus::Active;
        }
        _presence.Observe(guid, event, eligible, std::chrono::steady_clock::now());
        return true;
    }
    if (event.type == "contract.accepted" && _contractActivation &&
        event.sourceSystem == "fury.contracts" && event.correlationKey == _activationContract &&
        event.actor.kind == ActorKind::Human && event.actor.householdId && event.id)
    {
        auto run = Find(*event.actor.householdId);
        if (run && run->phaseKey == "rumours" && run->status == DirectorRunStatus::Active &&
            event.id > run->startedEventId)
            return RequestActivation(event, *run);
        return true;
    }
    if (event.type == "defias.activation.requested" && event.sourceSystem == "fury.defias" &&
        event.subjectType == "director_run" && event.subjectId && event.correlationKey == GraphKey)
        return _graph.Activate(event, *event.subjectId);

    if ((event.type == "living_world.runtime.observed" || event.type == "living_world.stage.observed") &&
        event.sourceSystem == "living_world" && event.subjectType == "living_world_runtime" &&
        event.subjectId && event.correlationKey == "invasion:1")
    {
        auto state = Number(event.payloadJson, "state");
        auto stage = Number(event.payloadJson, "stage_id");
        if (!state || !stage) return true;
        bool terminal = *state == static_cast<uint32>(LivingWorldRuntimeState::Complete) ||
            *state == static_cast<uint32>(LivingWorldRuntimeState::Failed);
        for (auto const& run : _director.ActiveRuns())
        {
            if (run.graphKey != GraphKey || !run.externalRuntimeId ||
                *run.externalRuntimeId != *event.subjectId) continue;
            FuryEvent source = event;
            source.actor.kind = ActorKind::System;
            source.actor.householdId = run.householdId;
            if (!_graph.Observe(source, *event.subjectId, *stage, terminal)) return false;
        }
    }
    return true;
}

void Service::Tick()
{
    if (!Enabled()) return;
    _livingWorld.PollManagedRuntimes();
    auto now = std::chrono::steady_clock::now();
    for (auto it = _presence.Entries().begin(); it != _presence.Entries().end();)
    {
        auto const& presence = it->second;
        Player* player = ObjectAccessor::FindPlayer(presence.source.actor.characterGuid);
        if (!player || !player->IsInWorld()) { it = _presence.Entries().erase(it); continue; }
        FuryEvent current = presence.source;
        current.actor = _actors.Resolve(player);
        current.mapId = player->GetMapId(); current.zoneId = player->GetZoneId();
        if (current.actor.householdId != presence.source.actor.householdId ||
            !Graph::Eligible(current, player->GetLevel(), _minimumLevel))
        { it = _presence.Entries().erase(it); continue; }
        auto run = Find(*current.actor.householdId);
        if (!run || run->phaseKey != "rumours" || run->status != DirectorRunStatus::Active)
        { it = _presence.Entries().erase(it); continue; }
        if (now - presence.since >= std::chrono::seconds(_presenceSeconds) && RequestActivation(current, *run))
        { it = _presence.Entries().erase(it); continue; }
        ++it;
    }
}

bool Service::RequestActivation(FuryEvent source, DirectorRun const& run)
{
    source.id = 0;
    source.type = "defias.activation.requested";
    source.sourceSystem = "fury.defias";
    source.subjectType = "director_run";
    source.subjectId = run.id;
    source.correlationKey = GraphKey;
    source.payloadJson = "{}";
    source.dedupeIdentity = Acore::StringFormat("defias:activate:v1:{}", run.id);
    return _events.Append(source).has_value();
}

bool Service::CampaignComplete(HouseholdId household)
{
    return _campaign.GetStatus(household, CampaignKey) == CampaignStatus::Complete;
}
std::optional<DirectorRun> Service::Find(HouseholdId household)
{
    // Preserve any run made against the earlier, unpublished SQL prototype.
    // Do not silently start a second scenario or reinterpret its state.
    if (auto legacy = _repository.FindLatestGraph(household, "defias.resurgence"))
        return legacy;
    return _repository.FindLatestGraph(household, GraphKey);
}
DirectorResult Service::Start(FuryEvent const& source)
{
    return _director.Start(source, GraphKey, "rumours");
}
bool Service::Advance(FuryEvent const& source, DirectorRun const& run, std::string_view phase)
{
    return _director.AdvancePhase(source, run.id, run.revision, phase).Accepted();
}
std::optional<uint64> Service::StartExternal(FuryEvent const& source)
{
    auto result = _livingWorld.StartInvasion(source, InvasionId);
    if (result.Accepted() && result.runtime) return result.runtime->runtimeId;
    return std::nullopt;
}
bool Service::Attach(FuryEvent const& source, DirectorRun const& run, uint64 runtime)
{
    return _director.AttachRuntime(source, run.id, run.revision, runtime).Accepted();
}
}
