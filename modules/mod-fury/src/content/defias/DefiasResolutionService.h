#ifndef MOD_FURY_DEFIAS_RESOLUTION_SERVICE_H
#define MOD_FURY_DEFIAS_RESOLUTION_SERVICE_H

#include "events/EventConsumer.h"
#include "director/DirectorTypes.h"

#include <string_view>

namespace Fury
{
class CampaignService;
class DirectorService;
class EventStore;
class ProofService;
class RewardService;
}

namespace Fury::Defias
{
class ScoreService;

inline constexpr char DefiasDefendedProof[] = "world.westfall.defended";
inline constexpr char DefiasBloodiedProof[] = "world.westfall.bloodied";
inline constexpr char DefiasEmergencyProof[] = "world.westfall.emergency";
inline constexpr char DefiasSuccessReward[] = "classic.westfall.defias.success";

class ResolutionService final : public EventConsumer
{
public:
    ResolutionService(
        DirectorService& director,
        ScoreService const& score,
        CampaignService& campaign,
        ProofService& proofs,
        RewardService& rewards,
        EventStore const& events);

    [[nodiscard]] std::string_view Key() const override
    {
        return "defias.resolution.v1";
    }

    bool Handle(FuryEvent const& event) override;

private:
    bool ResolveRun(FuryEvent const& event) const;
    bool PersistOutcome(FuryEvent const& event) const;
    bool EnsureCampaignActive(FuryEvent const& authority) const;
    bool EmitPressureIncrease(
        FuryEvent const& authority,
        DirectorRunId runId,
        uint32 score) const;
    bool EmitOutcome(
        FuryEvent const& authority,
        DirectorRunId runId,
        std::string_view outcomeKey,
        uint32 score) const;

    DirectorService& _director;
    ScoreService const& _score;
    CampaignService& _campaign;
    ProofService& _proofs;
    RewardService& _rewards;
    EventStore const& _events;
};
}

#endif
