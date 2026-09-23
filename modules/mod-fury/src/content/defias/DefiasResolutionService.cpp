#include "DefiasResolutionService.h"

#include "DefiasGraph.h"
#include "DefiasScoreService.h"
#include "campaign/CampaignService.h"
#include "director/DirectorService.h"
#include "events/EventStore.h"
#include "proofs/ProofService.h"
#include "rewards/RewardService.h"

#include "Log.h"
#include "StringFormat.h"

namespace Fury::Defias
{
ResolutionService::ResolutionService(
    DirectorService& director,
    ScoreService const& score,
    CampaignService& campaign,
    ProofService& proofs,
    RewardService& rewards,
    EventStore const& events)
    : _director(director),
      _score(score),
      _campaign(campaign),
      _proofs(proofs),
      _rewards(rewards),
      _events(events)
{
}

bool ResolutionService::Handle(FuryEvent const& event)
{
    if (event.type == "director.phase.changed" &&
        event.sourceSystem == "fury.director" &&
        event.correlationKey == GraphKey &&
        event.subjectType == "director_run" &&
        event.subjectId)
    {
        return ResolveRun(event);
    }

    if (event.type == "director.run.resolved" &&
        event.sourceSystem == "fury.director" &&
        event.correlationKey == GraphKey &&
        event.subjectType == "director_run" &&
        event.subjectId)
    {
        return PersistOutcome(event);
    }

    return true;
}

bool ResolutionService::ResolveRun(FuryEvent const& event) const
{
    std::optional<DirectorRun> run =
        _director.FindRun(*event.subjectId);
    if (!run ||
        run->graphKey != GraphKey ||
        run->phaseKey != "resolution" ||
        !event.actor.householdId ||
        *event.actor.householdId != run->householdId)
    {
        return true;
    }

    if (!_score.Enabled())
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] cannot resolve Defias run {} while scoring is disabled.",
            run->id);
        return false;
    }

    ScoreOutcome const outcome = _score.Outcome(run->id);
    std::string_view const outcomeKey = ScoreOutcomeKey(outcome);

    if (run->status == DirectorRunStatus::Complete)
    {
        if (!run->outcomeKey || *run->outcomeKey != outcomeKey)
            return false;

        if (run->resolvedEventId)
            return true;
    }
    else if (run->status != DirectorRunStatus::Active)
    {
        return true;
    }

    DirectorResult result =
        _director.Resolve(
            event,
            run->id,
            run->revision,
            outcomeKey);

    return result.Accepted();
}

bool ResolutionService::EnsureCampaignActive(
    FuryEvent const& authority) const
{
    if (!authority.actor.householdId)
        return false;

    HouseholdId const householdId =
        *authority.actor.householdId;

    CampaignStatus status =
        _campaign.GetStatus(householdId, CampaignKey);

    if (status == CampaignStatus::Locked)
    {
        CampaignTransitionResult available =
            _campaign.MarkAvailable(authority, CampaignKey);
        if (!available.Accepted())
            return false;
        status = available.status;
    }

    if (status == CampaignStatus::Available)
    {
        CampaignTransitionResult active =
            _campaign.Activate(authority, CampaignKey);
        if (!active.Accepted())
            return false;
        status = active.status;
    }

    return status == CampaignStatus::Active ||
        status == CampaignStatus::Complete;
}

bool ResolutionService::PersistOutcome(
    FuryEvent const& event) const
{
    std::optional<DirectorRun> run =
        _director.FindRun(*event.subjectId);

    if (!run ||
        run->graphKey != GraphKey ||
        run->status != DirectorRunStatus::Complete ||
        !run->outcomeKey ||
        !run->resolvedEventId ||
        *run->resolvedEventId != event.id ||
        !event.actor.householdId ||
        *event.actor.householdId != run->householdId)
    {
        return true;
    }

    std::optional<FuryEvent> start =
        _events.Find(run->startedEventId);
    if (!start ||
        start->actor.kind != ActorKind::Human ||
        !start->actor.householdId ||
        *start->actor.householdId != run->householdId)
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Defias run {} has no canonical Human start authority.",
            run->id);
        return false;
    }

    FuryEvent authority = event;
    authority.actor = start->actor;

    if (!EnsureCampaignActive(authority))
        return false;

    uint32 const score =
        _score.Score(run->id).value_or(0);

    std::string const metadata = Acore::StringFormat(
        "{{\"run_id\":{},\"outcome_key\":\"{}\",\"score\":{}}}",
        run->id,
        *run->outcomeKey,
        score);

    if (*run->outcomeKey == "success")
    {
        if (!_proofs.Grant(
                authority,
                DefiasDefendedProof,
                metadata).Accepted())
        {
            return false;
        }

        CampaignTransitionResult completed =
            _campaign.Complete(authority, CampaignKey);
        if (!completed.Accepted())
            return false;

        RewardRequest reward;
        reward.sourceEventId = event.id;
        reward.rewardKey = DefiasSuccessReward;
        reward.beneficiaryKind = BeneficiaryKind::Household;
        reward.beneficiaryId = run->householdId;
        reward.currentPowerBand =
            _campaign.GetHouseholdPowerBand(run->householdId)
                .value_or(PowerBand::None);

        if (!_rewards.Claim(reward).Accepted())
            return false;
    }
    else if (*run->outcomeKey == "partial")
    {
        if (!_proofs.Grant(
                authority,
                DefiasBloodiedProof,
                metadata).Accepted())
        {
            return false;
        }
    }
    else if (*run->outcomeKey == "ignored")
    {
        if (!_proofs.Grant(
                authority,
                DefiasEmergencyProof,
                metadata).Accepted())
        {
            return false;
        }

        if (!EmitPressureIncrease(authority, run->id, score))
            return false;
    }
    else
    {
        LOG_ERROR(
            "server.loading",
            "[FURY] Defias run {} has unknown outcome '{}'.",
            run->id,
            *run->outcomeKey);
        return false;
    }

    return EmitOutcome(
        authority,
        run->id,
        *run->outcomeKey,
        score);
}

bool ResolutionService::EmitPressureIncrease(
    FuryEvent const& authority,
    DirectorRunId runId,
    uint32 score) const
{
    FuryEvent event;
    event.type = "defias.pressure.increased";
    event.actor = authority.actor;
    event.mapId = authority.mapId;
    event.zoneId = authority.zoneId;
    event.areaId = authority.areaId;
    event.subjectType = "director_run";
    event.subjectId = runId;
    event.sourceSystem = "fury.defias";
    event.correlationKey = ScopeKey;
    event.dedupeIdentity = Acore::StringFormat(
        "defias:pressure:increased:v1:{}",
        runId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"delta\":1,\"score\":{}}}",
        runId,
        score);

    return _events.Append(event).has_value();
}

bool ResolutionService::EmitOutcome(
    FuryEvent const& authority,
    DirectorRunId runId,
    std::string_view outcomeKey,
    uint32 score) const
{
    FuryEvent event;
    event.type = Acore::StringFormat(
        "defias.resolution.{}",
        outcomeKey);
    event.actor = authority.actor;
    event.mapId = authority.mapId;
    event.zoneId = authority.zoneId;
    event.areaId = authority.areaId;
    event.subjectType = "director_run";
    event.subjectId = runId;
    event.sourceSystem = "fury.defias";
    event.correlationKey = GraphKey;
    event.dedupeIdentity = Acore::StringFormat(
        "defias:resolution:v1:{}",
        runId);
    event.payloadJson = Acore::StringFormat(
        "{{\"run_id\":{},\"outcome_key\":\"{}\",\"score\":{}}}",
        runId,
        outcomeKey,
        score);

    return _events.Append(event).has_value();
}
}
