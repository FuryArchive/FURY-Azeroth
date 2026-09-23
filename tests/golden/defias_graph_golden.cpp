#include "content/defias/DefiasGraph.h"
#include <cassert>
#include <iostream>
using namespace Fury;
using namespace Fury::Defias;

struct Port final : GraphPort
{
    std::optional<DirectorRun> saved;
    bool valid = true, complete = false, busy = false, failAttach = false;
    unsigned starts = 0, externalStarts = 0, startEmits = 0, phaseEmits = 0, attachEmits = 0;
    bool disabled = false, failStartEmit = false, failPhaseEmit = false, failAttachEmit = false;
    std::optional<DirectorRun> Find(HouseholdId) override { return saved; }
    bool ContentValid() const override { return valid; }
    bool CampaignComplete(HouseholdId) override { return complete; }
    DirectorResult Start(FuryEvent const& e) override
    {
        if (disabled) return {DirectorOutcome::GraphDisabled, {}};
        if (busy) return {DirectorOutcome::ScopeBusy, {}};
        if (!saved) {
            ++starts;
            saved = DirectorRun{};
            saved->id = 1; saved->householdId = *e.actor.householdId;
            saved->graphKey = GraphKey; saved->phaseKey = "rumours";
            saved->startedEventId = e.id;
        }
        if (failStartEmit) return {DirectorOutcome::PersistenceFailed, saved};
        ++startEmits; return {DirectorOutcome::Started, saved};
    }
    bool Advance(FuryEvent const&, DirectorRun const&, std::string_view phase) override
    { if(saved->phaseKey != phase) { saved->phaseKey = phase; ++saved->revision; }
        if (failPhaseEmit) return false;
        ++phaseEmits; return true; }
    std::optional<uint64> StartExternal(FuryEvent const&) override
    { if (!externalStarts) ++externalStarts; return 42; }
    bool Attach(FuryEvent const&, DirectorRun const&, uint64 id) override
    { if (failAttach) return false; saved->externalRuntimeId = id; ++saved->revision;
        if (failAttachEmit) return false;
        ++attachEmits; return true; }
};
FuryEvent Human()
{
    FuryEvent e; e.id = 1; e.actor.kind = ActorKind::Human;
    e.actor.householdId = 1; e.mapId = 0; e.zoneId = 40; return e;
}
int main()
{
    auto e = Human();
    Port p; Graph g(p);
    for (auto kind : {ActorKind::System, ActorKind::HouseholdAltBot, ActorKind::RandomPlayerBot, ActorKind::NpcAssistant}) {
        e.actor.kind = kind; assert(g.Enter(e, 80)); assert(!p.saved);
    }
    e = Human(); assert(g.Enter(e, 9)); assert(!p.saved);
    e.zoneId = 1; assert(g.Enter(e, 10)); assert(!p.saved); e.zoneId = 40;
    p.valid = false; assert(g.Enter(e, 10)); assert(!p.saved); p.valid = true;
    p.complete = true; assert(g.Enter(e, 10)); assert(!p.saved); p.complete = false;
    p.busy = true; assert(g.Enter(e, 10)); assert(!p.saved); p.busy = false;
    assert(g.Enter(e, 10)); assert(p.saved->phaseKey == "rumours");
    assert(g.Enter(e, 10)); assert(p.starts == 1); assert(p.externalStarts == 0);
    assert(g.Activate(e, 999)); assert(p.externalStarts == 0);
    e.actor.kind = ActorKind::HouseholdAltBot;
    assert(g.Activate(e, 1)); assert(p.externalStarts == 0); e = Human();
    p.failAttach = true;
    assert(!g.Activate(e, 1)); assert(p.saved->phaseKey == "invasion");
    Graph restarted(p); p.failAttach = false;
    assert(restarted.Activate(e, 1)); assert(p.externalStarts == 1);
    assert(p.saved->externalRuntimeId == 42);
    assert(restarted.Activate(e, 1)); assert(p.externalStarts == 1);
    assert(restarted.Observe(e, 99, 1006, false)); assert(p.saved->phaseKey == "invasion");
    assert(restarted.Observe(e, 42, 1006, false)); assert(p.saved->phaseKey == "final_battle");
    assert(restarted.Observe(e, 42, 1001, false)); assert(p.saved->phaseKey == "final_battle");
    assert(restarted.Observe(e, 42, 1006, true)); assert(p.saved->phaseKey == "resolution");
    assert(restarted.Activate(e, 1)); assert(p.externalStarts == 1);
    p.saved->status = DirectorRunStatus::Complete; p.saved->outcomeKey = "partial";
    assert(restarted.Enter(e, 10)); assert(p.starts == 1);
    assert(OutcomeForScore(0) == "ignored"); assert(OutcomeForScore(29) == "ignored");
    assert(OutcomeForScore(30) == "partial"); assert(OutcomeForScore(69) == "partial");
    assert(OutcomeForScore(70) == "success"); assert(OutcomeForScore(100) == "success");
    Port disabled; disabled.disabled = true; Graph disabledGraph(disabled);
    assert(disabledGraph.Enter(e, 10));
    Port recovery; Graph repair(recovery); recovery.failStartEmit = true;
    assert(!repair.Enter(e, 10)); recovery.failStartEmit = false;
    assert(repair.Enter(e, 10)); assert(recovery.startEmits == 1);
    recovery.disabled = true;
    assert(repair.Enter(e, 10)); recovery.disabled = false;
    recovery.failPhaseEmit = true;
    assert(!repair.Activate(e, 1)); recovery.failPhaseEmit = false;
    assert(repair.Activate(e, 1)); assert(recovery.phaseEmits == 1);
    recovery.saved->externalRuntimeId.reset(); recovery.failAttachEmit = true;
    assert(!repair.Activate(e, 1)); recovery.failAttachEmit = false;
    auto emitted = recovery.attachEmits;
    assert(repair.Activate(e, 1)); assert(recovery.attachEmits == emitted + 1);
    recovery.failPhaseEmit = true;
    assert(!repair.Observe(e, 42, 1006, false)); recovery.failPhaseEmit = false;
    emitted = recovery.phaseEmits;
    assert(repair.Observe(e, 42, 1006, false)); assert(recovery.phaseEmits == emitted + 1);
    recovery.failPhaseEmit = true;
    assert(!repair.Observe(e, 42, 1006, true)); recovery.failPhaseEmit = false;
    emitted = recovery.phaseEmits;
    assert(repair.Observe(e, 42, 1006, true)); assert(recovery.phaseEmits == emitted + 1);
    std::cout << "[FURY][PASS] T25 eligibility, one-shot, activation retry/restart, runtime isolation, phases, outcome boundaries\n";
}
