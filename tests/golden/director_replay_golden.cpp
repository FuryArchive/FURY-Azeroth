#include "director/DirectorService.h"
#include "events/EventStore.h"
#include <cassert>
#include <iostream>
#include <map>

namespace
{
std::optional<Fury::DirectorRun> saved;
bool failAppend = false;
std::map<std::string, unsigned> emissions;
}
namespace Fury
{
std::optional<DirectorGraphDefinition> DirectorRepository::FindGraph(std::string_view key) const
{ return DirectorGraphDefinition{std::string(key), "scope", "Graph", {}, true}; }
std::optional<DirectorRun> DirectorRepository::FindRun(DirectorRunId) const { return saved; }
std::optional<DirectorRun> DirectorRepository::FindActiveScope(HouseholdId, std::string_view) const { return saved; }
void DirectorRepository::InsertRun(HouseholdId household, DirectorGraphDefinition const& g, std::string_view phase, EventId event) const
{
    if(saved) return;
    saved = DirectorRun{}; saved->id = 1; saved->householdId = household;
    saved->graphKey = g.graphKey; saved->scopeKey = g.scopeKey;
    saved->phaseKey = phase; saved->startedEventId = event; saved->lastEventId = event;
}
void DirectorRepository::UpdatePhase(DirectorRunId, HouseholdId, uint64, std::string_view phase, EventId event) const
{ saved->phaseKey = phase; saved->lastEventId = event; ++saved->revision; }
void DirectorRepository::AttachRuntime(DirectorRunId, HouseholdId, uint64, uint64 runtime, EventId event) const
{ saved->externalRuntimeId = runtime; saved->lastEventId = event; ++saved->revision; }
void DirectorRepository::RecoverRuntime(DirectorRunId, HouseholdId, uint64, uint64, uint64 runtime, EventId event) const
{ saved->externalRuntimeId = runtime; saved->lastEventId = event; ++saved->revision; }
void DirectorRepository::Resolve(DirectorRunId, HouseholdId, uint64, std::string_view outcome, EventId event) const
{ saved->status = DirectorRunStatus::Complete; saved->outcomeKey = outcome; saved->lastEventId = event; ++saved->revision; }
void DirectorRepository::BindTerminalEvent(DirectorRunId, HouseholdId, EventId event) const
{ saved->resolvedEventId = event; saved->lastEventId = event; }
void DirectorRepository::Abort(DirectorRunId, HouseholdId, uint64, std::string_view outcome, EventId event) const
{ saved->status = DirectorRunStatus::Aborted; saved->outcomeKey = outcome; saved->lastEventId = event; ++saved->revision; }
std::optional<EventId> EventStore::Append(FuryEvent const& e) const
{ if(failAppend) return {}; ++emissions[e.type]; return 100; }
}
int main()
{
    Fury::DirectorRepository repository; Fury::EventStore events;
    Fury::DirectorService service(repository, events);
    Fury::FuryEvent source; source.id=1; source.actor.kind=Fury::ActorKind::Human; source.actor.householdId=1;
    failAppend=true;
    assert(!service.Start(source,"graph","rumours").Accepted()); assert(saved);
    failAppend=false;
    assert(service.Start(source,"graph","rumours").Accepted()); assert(emissions["director.run.started"]==1);
    source.id=2; failAppend=true;
    assert(!service.AdvancePhase(source,1,saved->revision,"invasion").Accepted()); assert(saved->phaseKey=="invasion");
    failAppend=false;
    // Restart reloads the post-mutation revision, not the old in-memory one.
    assert(service.AdvancePhase(source,1,saved->revision,"invasion").Accepted());
    assert(emissions["director.phase.changed"]==1);
    failAppend=true;
    assert(!service.AttachRuntime(source,1,saved->revision,42).Accepted()); assert(saved->externalRuntimeId==42);
    failAppend=false;
    assert(service.AttachRuntime(source,1,saved->revision,42).Accepted()); assert(emissions["director.runtime.attached"]==1);
    source.id=3; auto beforeRecovery=saved->revision;
    assert(service.RecoverRuntime(source,1,beforeRecovery,42,99).Accepted());
    assert(saved->externalRuntimeId==99);
    assert(emissions["director.runtime.recovered"]==1);
    std::cout << "[FURY][PASS] production Director retries events after persisted mutations and runtime recovery\n";
}
