#ifndef MOD_FURY_CAMPAIGN_POLICY_H
#define MOD_FURY_CAMPAIGN_POLICY_H

#include "CampaignTypes.h"
#include "actors/ActorPolicy.h"

namespace Fury
{
enum class CampaignTransitionDisposition : uint8
{
    Apply = 1,
    AlreadyApplied = 2,
    Reject = 3
};

[[nodiscard]] constexpr CampaignTransitionDisposition EvaluateCampaignTransition(
    CampaignStatus current,
    CampaignStatus target)
{
    if (current == target)
        return CampaignTransitionDisposition::AlreadyApplied;

    if (current == CampaignStatus::Locked && target == CampaignStatus::Available)
        return CampaignTransitionDisposition::Apply;

    if (current == CampaignStatus::Available && target == CampaignStatus::Active)
        return CampaignTransitionDisposition::Apply;

    if (current == CampaignStatus::Active && target == CampaignStatus::Complete)
        return CampaignTransitionDisposition::Apply;

    return CampaignTransitionDisposition::Reject;
}

[[nodiscard]] constexpr bool CanAuthorCampaignTransition(ActorKind kind)
{
    return IsPersistentProgressionAuthority(kind);
}

static_assert(EvaluateCampaignTransition(CampaignStatus::Locked, CampaignStatus::Available) == CampaignTransitionDisposition::Apply);
static_assert(EvaluateCampaignTransition(CampaignStatus::Available, CampaignStatus::Active) == CampaignTransitionDisposition::Apply);
static_assert(EvaluateCampaignTransition(CampaignStatus::Active, CampaignStatus::Complete) == CampaignTransitionDisposition::Apply);
static_assert(EvaluateCampaignTransition(CampaignStatus::Complete, CampaignStatus::Complete) == CampaignTransitionDisposition::AlreadyApplied);
static_assert(EvaluateCampaignTransition(CampaignStatus::Complete, CampaignStatus::Available) == CampaignTransitionDisposition::Reject);
static_assert(EvaluateCampaignTransition(CampaignStatus::Active, CampaignStatus::Available) == CampaignTransitionDisposition::Reject);
static_assert(EvaluateCampaignTransition(CampaignStatus::Locked, CampaignStatus::Complete) == CampaignTransitionDisposition::Reject);

static_assert(CanAuthorCampaignTransition(ActorKind::Human));
static_assert(!CanAuthorCampaignTransition(ActorKind::System));
static_assert(!CanAuthorCampaignTransition(ActorKind::HouseholdAltBot));
static_assert(!CanAuthorCampaignTransition(ActorKind::RandomPlayerBot));
}

#endif
