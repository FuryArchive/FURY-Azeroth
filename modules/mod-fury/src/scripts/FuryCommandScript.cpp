#include "core/FuryApp.h"

#include "Chat.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <cstdlib>
#include <string>

using namespace Acore::ChatCommands;

namespace
{
uint32 ParseLimit(char const* args, uint32 defaultValue, uint32 maximum)
{
    if (!args || !*args)
        return defaultValue;

    unsigned long parsed = std::strtoul(args, nullptr, 10);
    if (!parsed)
        return defaultValue;

    return std::min<uint32>(static_cast<uint32>(parsed), maximum);
}

char const* ActorKindName(Fury::ActorKind kind)
{
    switch (kind)
    {
        case Fury::ActorKind::Human:
            return "Human";
        case Fury::ActorKind::HouseholdAltBot:
            return "HouseholdAltBot";
        case Fury::ActorKind::RandomPlayerBot:
            return "RandomPlayerBot";
        case Fury::ActorKind::NpcAssistant:
            return "NpcAssistant";
        case Fury::ActorKind::System:
            return "System";
    }

    return "Unknown";
}

char const* RewardStatusName(Fury::RewardClaimStatus status)
{
    switch (status)
    {
        case Fury::RewardClaimStatus::Pending:
            return "Pending";
        case Fury::RewardClaimStatus::Delivered:
            return "Delivered";
        case Fury::RewardClaimStatus::Failed:
            return "Failed";
    }

    return "Unknown";
}

char const* BeneficiaryKindName(Fury::BeneficiaryKind kind)
{
    switch (kind)
    {
        case Fury::BeneficiaryKind::Character:
            return "Character";
        case Fury::BeneficiaryKind::Account:
            return "Account";
        case Fury::BeneficiaryKind::Household:
            return "Household";
    }

    return "Unknown";
}

char const* HouseholdResultName(Fury::HouseholdMemberResult result)
{
    switch (result)
    {
        case Fury::HouseholdMemberResult::Added:
            return "added";
        case Fury::HouseholdMemberResult::AlreadyMember:
            return "already-member";
        case Fury::HouseholdMemberResult::AccountInOtherHousehold:
            return "account-in-other-household";
        case Fury::HouseholdMemberResult::HouseholdFull:
            return "household-full";
        case Fury::HouseholdMemberResult::HouseholdNotFound:
            return "household-not-found";
    }

    return "unknown";
}

char const* SeverityName(Fury::ValidationSeverity severity)
{
    switch (severity)
    {
        case Fury::ValidationSeverity::Info:
            return "INFO";
        case Fury::ValidationSeverity::Warning:
            return "WARN";
        case Fury::ValidationSeverity::Error:
            return "ERROR";
        case Fury::ValidationSeverity::Fatal:
            return "FATAL";
    }

    return "UNKNOWN";
}

class FuryCommandScript final : public CommandScript
{
public:
    FuryCommandScript()
        : CommandScript("FuryCommandScript")
    {
    }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable householdTable = {
            {"status", HandleHouseholdStatus, SEC_GAMEMASTER, Console::No},
            {"create", HandleHouseholdCreate, SEC_GAMEMASTER, Console::No},
            {"add", HandleHouseholdAdd, SEC_GAMEMASTER, Console::No},
        };

        static ChatCommandTable eventTable = {
            {"tail", HandleEventTail, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable rewardTable = {
            {"claims", HandleRewardClaims, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable furyTable = {
            {"status", HandleStatus, SEC_GAMEMASTER, Console::Yes},
            {"actor", HandleActor, SEC_GAMEMASTER, Console::No},
            {"household", householdTable},
            {"event", eventTable},
            {"reward", rewardTable},
            {"validate", HandleValidate, SEC_GAMEMASTER, Console::Yes},
        };

        static ChatCommandTable commandTable = {
            {"fury", furyTable},
        };

        return commandTable;
    }

private:
    static bool HandleStatus(ChatHandler* handler, char const* /*args*/)
    {
        Fury::App& app = Fury::App::Instance();

        handler->PSendSysMessage(
            "FURY: initialized={} enabled={}",
            app.IsInitialized() ? "yes" : "no",
            app.IsEnabled() ? "yes" : "no");

        if (!app.IsEnabled())
        {
            handler->PSendSysMessage("FURY database is not opened while the module is disabled.");
            return true;
        }

        std::optional<Fury::KernelSnapshot> snapshot =
            app.Diagnostics().Snapshot();
        if (!snapshot)
        {
            handler->SendErrorMessage(
                "FURY database snapshot unavailable. Run .fury validate.");
            return false;
        }

        handler->PSendSysMessage(
            "DB: households={} members={} events={} consumers={} reward_claims={} chronicle={}",
            snapshot->households,
            snapshot->householdMembers,
            snapshot->events,
            snapshot->consumers,
            snapshot->rewardClaims,
            snapshot->chronicleEntries);

        return true;
    }

    static bool HandleActor(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendErrorMessage("This command requires an in-game player.");
            return false;
        }

        Fury::ActorContext actor =
            Fury::App::Instance().Actors().Resolve(player);

        handler->PSendSysMessage(
            "Actor: kind={} guid={} account={} household={} persistent={}",
            ActorKindName(actor.kind),
            actor.characterGuid.GetRawValue(),
            actor.accountId,
            actor.householdId ? std::to_string(*actor.householdId) : "none",
            actor.isEligibleForPersistentProgression ? "yes" : "no");

        return true;
    }

    static bool HandleHouseholdCreate(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendErrorMessage("This command requires an in-game player.");
            return false;
        }

        Fury::ActorContext actor =
            Fury::App::Instance().Actors().Resolve(player);

        if (actor.householdId)
        {
            handler->SendErrorMessage("This account already belongs to a FURY household.");
            return false;
        }

        if (!args || !*args)
        {
            handler->SendErrorMessage("Usage: .fury household create <slug>");
            return false;
        }

        std::string slug(args);
        if (slug.find_first_of(" \t\r\n") != std::string::npos)
        {
            handler->SendErrorMessage("Household slug must not contain whitespace.");
            return false;
        }

        Fury::HouseholdCreateResult created =
            Fury::App::Instance().Households().CreateOrGet(slug, slug);
        if (!created.Succeeded())
        {
            handler->SendErrorMessage("Could not create or resolve FURY household.");
            return false;
        }

        Fury::HouseholdMemberResult added =
            Fury::App::Instance().Households().AddMember(
                *created.householdId,
                actor.accountId);

        if (added != Fury::HouseholdMemberResult::Added &&
            added != Fury::HouseholdMemberResult::AlreadyMember)
        {
            handler->SendErrorMessage(
                "Household created but account attach failed: {}",
                HouseholdResultName(added));
            return false;
        }

        handler->PSendSysMessage(
            "FURY household created: id={} slug={} account={}",
            *created.householdId,
            slug,
            actor.accountId);

        return true;
    }

    static bool HandleHouseholdAdd(ChatHandler* handler, char const* /*args*/)
    {
        Player* owner = handler->GetPlayer();
        if (!owner)
        {
            handler->SendErrorMessage("This command requires an in-game player.");
            return false;
        }

        Fury::ActorContext ownerActor =
            Fury::App::Instance().Actors().Resolve(owner);
        if (!ownerActor.householdId)
        {
            handler->SendErrorMessage(
                "Create a household first with .fury household create <slug>.");
            return false;
        }

        Player* target = handler->getSelectedPlayerOrSelf();
        if (!target)
        {
            handler->SendErrorMessage("Select an online player to add.");
            return false;
        }

        Fury::ActorContext targetActor =
            Fury::App::Instance().Actors().Resolve(target);
        if (!targetActor.accountId)
        {
            handler->SendErrorMessage("Selected player has no account id.");
            return false;
        }

        Fury::HouseholdMemberResult result =
            Fury::App::Instance().Households().AddMember(
                *ownerActor.householdId,
                targetActor.accountId);

        handler->PSendSysMessage(
            "FURY household add: account={} result={}",
            targetActor.accountId,
            HouseholdResultName(result));

        return result == Fury::HouseholdMemberResult::Added ||
            result == Fury::HouseholdMemberResult::AlreadyMember;
    }

    static bool HandleHouseholdStatus(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetPlayer();
        if (!player)
        {
            handler->SendErrorMessage("This command requires an in-game player.");
            return false;
        }

        Fury::ActorContext actor =
            Fury::App::Instance().Actors().Resolve(player);

        if (!actor.householdId)
        {
            handler->PSendSysMessage("FURY household: none");
            return true;
        }

        uint32 const members =
            Fury::App::Instance().Households().CountMembers(*actor.householdId);

        handler->PSendSysMessage(
            "FURY household: id={} members={}/2",
            *actor.householdId,
            members);

        return true;
    }

    static bool HandleEventTail(ChatHandler* handler, char const* args)
    {
        uint32 const limit = ParseLimit(args, 10, 50);
        std::vector<Fury::FuryEvent> events =
            Fury::App::Instance().Events().Tail(limit);

        handler->PSendSysMessage("FURY event tail: {} row(s)", events.size());

        for (Fury::FuryEvent const& event : events)
        {
            handler->PSendSysMessage(
                "#{} {} actor={} account={} household={} subject={}:{} source={}",
                event.id,
                event.type,
                ActorKindName(event.actor.kind),
                event.actor.accountId,
                event.actor.householdId
                    ? std::to_string(*event.actor.householdId)
                    : "none",
                event.subjectType.empty() ? "-" : event.subjectType,
                event.subjectId
                    ? std::to_string(*event.subjectId)
                    : "-",
                event.sourceSystem);
        }

        return true;
    }

    static bool HandleRewardClaims(ChatHandler* handler, char const* args)
    {
        uint32 const limit = ParseLimit(args, 10, 50);
        std::vector<Fury::RewardClaimView> claims =
            Fury::App::Instance().Rewards().TailClaims(limit);

        handler->PSendSysMessage(
            "FURY reward claims: {} row(s)",
            claims.size());

        for (Fury::RewardClaimView const& claim : claims)
        {
            handler->PSendSysMessage(
                "#{} event={} reward={} beneficiary={}:{} status={}",
                claim.id,
                claim.sourceEventId,
                claim.rewardKey,
                BeneficiaryKindName(claim.beneficiaryKind),
                claim.beneficiaryId,
                RewardStatusName(claim.status));
        }

        return true;
    }

    static bool HandleValidate(ChatHandler* handler, char const* /*args*/)
    {
        Fury::ValidationReport report =
            Fury::App::Instance().Diagnostics().Validate();

        for (Fury::ValidationIssue const& issue : report.issues)
        {
            handler->PSendSysMessage(
                "[{}] {}/{}: {}",
                SeverityName(issue.severity),
                issue.subsystem,
                issue.key,
                issue.message);
        }

        handler->PSendSysMessage(
            "FURY validation: {}",
            report.IsHealthy() ? "HEALTHY" : "FAILED");

        return report.IsHealthy();
    }
};
}

void AddFuryCommandScripts()
{
    new FuryCommandScript();
}
