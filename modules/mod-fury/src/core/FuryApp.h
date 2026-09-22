#ifndef MOD_FURY_APP_H
#define MOD_FURY_APP_H

#include "Define.h"
#include "actors/ActorResolver.h"
#include "household/HouseholdRepository.h"
#include "household/HouseholdService.h"
#include "events/EventStore.h"
#include "events/ConsumerCheckpointRepository.h"
#include "events/EventBus.h"
#include "rewards/RewardRepository.h"
#include "rewards/RewardPolicy.h"
#include "rewards/RewardService.h"
#include "chronicle/ChronicleRepository.h"
#include "chronicle/ChronicleService.h"
#include "diagnostics/DiagnosticsRepository.h"
#include "diagnostics/DiagnosticsService.h"
#include "campaign/CampaignRepository.h"
#include "campaign/CampaignService.h"
#include "proofs/ProofRepository.h"
#include "proofs/ProofService.h"
#include "contracts/ContractRepository.h"
#include "contracts/ContractService.h"
#include "contracts/ContractRewardConsumer.h"
#include "director/DirectorRepository.h"
#include "director/DirectorService.h"
#include "director/DirectorReconciliation.h"

namespace Fury
{
class App final
{
public:
    static App& Instance();

    void Initialize();
    void Update(uint32 diff);
    void Shutdown();

    [[nodiscard]] bool IsEnabled() const { return _enabled; }
    [[nodiscard]] bool IsInitialized() const { return _initialized; }

    ActorResolver& Actors() { return _actors; }
    HouseholdService& Households() { return _households; }
    EventStore& Events() { return _events; }
    EventBus& EventStream() { return _eventBus; }
    RewardService& Rewards() { return _rewards; }
    ChronicleService& Chronicle() { return _chronicle; }
    DiagnosticsService& Diagnostics() { return _diagnostics; }
    CampaignService& Campaign() { return _campaign; }
    ProofService& Proofs() { return _proofs; }
    ContractService& Contracts() { return _contracts; }
    DirectorService& Director() { return _director; }
    DirectorReconciliationService& DirectorReconciliation()
    {
        return _directorReconciliation;
    }

private:
    struct TickConfig
    {
        uint32 fastMs = 250;
        uint32 serviceMs = 1000;
        uint32 directorMs = 5000;
        uint32 reconcileMs = 30000;
    };

    App();

    static bool AdvanceTimer(uint64& accumulator, uint32 diff, uint32 interval);

    void ResetTimers();
    void RunFastTick();
    void RunServiceTick();
    void RunDirectorTick();
    void RunReconcileTick();

    HouseholdRepository _householdRepository;
    HouseholdService _households;
    ActorResolver _actors;
    EventStore _events;
    ConsumerCheckpointRepository _consumerCheckpoints;
    EventBus _eventBus;

    RewardRepository _rewardRepository;
    RewardPolicy _rewardPolicy;
    RewardService _rewards;

    ChronicleRepository _chronicleRepository;
    ChronicleService _chronicle;

    DiagnosticsRepository _diagnosticsRepository;
    DiagnosticsService _diagnostics;

    CampaignRepository _campaignRepository;
    CampaignService _campaign;

    ProofRepository _proofRepository;
    ProofService _proofs;

    ContractRepository _contractRepository;
    ContractService _contracts;
    ContractRewardConsumer _contractRewards;

    DirectorRepository _directorRepository;
    DirectorService _director;
    DirectorReconciliationService _directorReconciliation;

    TickConfig _ticks;

    uint64 _fastAccumulator = 0;
    uint64 _serviceAccumulator = 0;
    uint64 _directorAccumulator = 0;
    uint64 _reconcileAccumulator = 0;

    bool _enabled = false;
    bool _initialized = false;
};
}

#endif
