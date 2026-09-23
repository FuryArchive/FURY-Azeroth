#include "core/FuryApp.h"

#include "content/defias/DefiasGraph.h"
#include "content/defias/DefiasContracts.h"
#include "content/defias/DefiasFieldRelief.h"
#include "contracts/ContractTypes.h"
#include "events/FuryEvent.h"

#include "GameObject.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
inline constexpr char BoardScriptName[] = "fury_westfall_contract_board";
inline constexpr uint32 BoardActionBase = GOSSIP_ACTION_INFO_DEF + 100;

Fury::Defias::FieldReliefChoice FieldReliefChoiceFor(Player* player)
{
    if (!player)
        return {};

    return Fury::Defias::SelectFieldReliefChoice(
        player->GetSkillValue(Fury::Defias::FieldReliefAlchemySkill),
        player->GetSkillValue(Fury::Defias::FieldReliefFirstAidSkill),
        player->GetSkillValue(Fury::Defias::FieldReliefCookingSkill));
}

std::optional<Fury::DirectorRun> FindDefiasRun(
    Fury::App& app,
    Fury::HouseholdId householdId)
{
    for (Fury::DirectorRun const& run : app.Director().ActiveRuns())
    {
        if (run.householdId == householdId &&
            run.graphKey == Fury::Defias::GraphKey)
        {
            return run;
        }
    }

    return std::nullopt;
}

Fury::ContractBoardContext BuildContext(
    Fury::App& app,
    Fury::ActorContext const& actor)
{
    Fury::ContractBoardContext context;
    context.householdId = actor.householdId.value_or(0);
    context.campaignNodeKey = Fury::Defias::CampaignKey;

    std::optional<Fury::DirectorRun> run =
        actor.householdId
            ? FindDefiasRun(app, *actor.householdId)
            : std::nullopt;

    context.allowNewContracts = run.has_value();

    if (run)
    {
        context.directorRunId = run->id;
        context.directorPhase = run->phaseKey;
    }

    return context;
}

char const* StatusPrefix(Fury::ContractStatus status)
{
    switch (status)
    {
        case Fury::ContractStatus::Available:
            return "[Available] ";
        case Fury::ContractStatus::Active:
            return "[Active] ";
        case Fury::ContractStatus::Complete:
            return "[Complete] ";
        case Fury::ContractStatus::Failed:
            return "[Failed] ";
        case Fury::ContractStatus::Expired:
            return "[Expired] ";
    }

    return "";
}

class FuryWestfallContractBoardScript final : public GameObjectScript
{
public:
    FuryWestfallContractBoardScript()
        : GameObjectScript(BoardScriptName)
    {
    }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        Render(player, go);
        return true;
    }

    bool OnGossipSelect(
        Player* player,
        GameObject* go,
        uint32 sender,
        uint32 action) override
    {
        if (sender != GOSSIP_SENDER_MAIN)
        {
            Render(player, go);
            return true;
        }

        Fury::App& app = Fury::App::Instance();
        if (!app.IsInitialized() || !app.IsEnabled())
        {
            Render(player, go);
            return true;
        }

        Fury::ActorContext actor = app.Actors().Resolve(player);
        Fury::ContractBoardContext context = BuildContext(app, actor);
        std::vector<Fury::ContractBoardEntry> entries =
            app.Contracts().ListBoard(Fury::Defias::ContractBoardKey, context);

        if (action > BoardActionBase)
        {
            uint32 const index = action - BoardActionBase - 1;
            if (index < entries.size() &&
                entries[index].status == Fury::ContractStatus::Available &&
                actor.kind == Fury::ActorKind::Human &&
                actor.householdId &&
                player->GetMapId() == Fury::Defias::MapId &&
                player->GetZoneId() == Fury::Defias::ZoneId)
            {
                Fury::FuryEvent source;
                source.type = "contract.board.accept.requested";
                source.actor = actor;
                source.mapId = player->GetMapId();
                source.zoneId = player->GetZoneId();
                source.areaId = player->GetAreaId();
                source.subjectType = "contract";
                source.sourceSystem = "fury.contract_board";
                source.correlationKey =
                    entries[index].definition.contractKey;
                source.dedupeIdentity = Acore::StringFormat(
                    "contract-board:accept:v1:{}:{}:{}",
                    *actor.householdId,
                    context.directorRunId.value_or(0),
                    entries[index].definition.contractKey);
                source.payloadJson =
                    "{\"board_key\":\"classic.westfall.contracts\"}";

                bool const fieldRelief =
                    entries[index].definition.contractKey ==
                        Fury::Defias::FieldReliefContract;
                Fury::Defias::FieldReliefChoice const reliefChoice =
                    fieldRelief
                        ? FieldReliefChoiceFor(player)
                        : Fury::Defias::FieldReliefChoice{};

                if (!fieldRelief || reliefChoice)
                {
                    if (std::optional<Fury::EventId> eventId =
                            app.Events().Append(source))
                    {
                        source.id = *eventId;
                        Fury::ContractAcceptResult accepted =
                            app.Contracts().Accept(
                                source,
                                entries[index].definition.contractKey,
                                context.directorRunId);

                        if (fieldRelief &&
                            accepted.AcceptedOrExisting())
                        {
                            (void)app.ProfessionOrders().Start(
                                source,
                                Fury::Defias::FieldReliefOrderKey,
                                reliefChoice.optionOrdinal);
                        }
                    }
                }
            }
        }

        Render(player, go);
        return true;
    }

private:
    static void Render(Player* player, GameObject* go)
    {
        ClearGossipMenuFor(player);

        Fury::App& app = Fury::App::Instance();
        if (!app.IsInitialized() || !app.IsEnabled())
        {
            AddGossipItemFor(
                player,
                0,
                "FURY contracts are unavailable.",
                GOSSIP_SENDER_MAIN,
                BoardActionBase);
            SendGossipMenuFor(player, 1, go->GetGUID());
            return;
        }

        Fury::ActorContext actor = app.Actors().Resolve(player);
        if (actor.kind != Fury::ActorKind::Human || !actor.householdId)
        {
            AddGossipItemFor(
                player,
                0,
                "Join a FURY household before using this board.",
                GOSSIP_SENDER_MAIN,
                BoardActionBase);
            SendGossipMenuFor(player, 1, go->GetGUID());
            return;
        }

        if (player->GetMapId() != Fury::Defias::MapId ||
            player->GetZoneId() != Fury::Defias::ZoneId)
        {
            AddGossipItemFor(
                player,
                0,
                "This board only serves the Westfall campaign.",
                GOSSIP_SENDER_MAIN,
                BoardActionBase);
            SendGossipMenuFor(player, 1, go->GetGUID());
            return;
        }

        Fury::ContractBoardContext context = BuildContext(app, actor);
        std::vector<Fury::ContractBoardEntry> entries =
            app.Contracts().ListBoard(Fury::Defias::ContractBoardKey, context);

        if (entries.empty())
        {
            AddGossipItemFor(
                player,
                0,
                "No Westfall contracts are available right now.",
                GOSSIP_SENDER_MAIN,
                BoardActionBase);
        }
        else
        {
            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                Fury::ContractBoardEntry const& entry = entries[index];

                bool const unavailableFieldRelief =
                    entry.status == Fury::ContractStatus::Available &&
                    entry.definition.contractKey ==
                        Fury::Defias::FieldReliefContract &&
                    !FieldReliefChoiceFor(player);

                std::string label;
                uint32 action = BoardActionBase +
                    1 +
                    static_cast<uint32>(index);

                if (unavailableFieldRelief)
                {
                    label =
                        "[Optional - learn Alchemy, First Aid, or Cooking] " +
                        entry.definition.title;
                    action = BoardActionBase;
                }
                else
                {
                    label =
                        std::string(StatusPrefix(entry.status)) +
                        entry.definition.title;
                }

                AddGossipItemFor(
                    player,
                    0,
                    label,
                    GOSSIP_SENDER_MAIN,
                    action);
            }
        }

        SendGossipMenuFor(player, 1, go->GetGUID());
    }
};
}

void AddFuryGameObjectScripts()
{
    new FuryWestfallContractBoardScript();
}
