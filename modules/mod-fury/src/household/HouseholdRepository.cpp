#include "HouseholdRepository.h"

#include "database/FuryDatabase.h"
#include "DatabaseEnv.h"

namespace Fury
{
namespace
{
std::optional<HouseholdId> ReadHouseholdId(PreparedQueryResult const& result)
{
    if (!result)
        return std::nullopt;

    Field* fields = result->Fetch();
    return fields[0].Get<HouseholdId>();
}
}

bool HouseholdRepository::Exists(HouseholdId householdId) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_BY_ID);
    stmt->SetData(0, householdId);
    return static_cast<bool>(FuryDatabase.Query(stmt));
}

std::optional<HouseholdId> HouseholdRepository::FindBySlug(std::string_view slug) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_BY_SLUG);
    stmt->SetData(0, std::string(slug));
    return ReadHouseholdId(FuryDatabase.Query(stmt));
}

std::optional<HouseholdId> HouseholdRepository::FindByAccount(uint32 accountId) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_BY_ACCOUNT);
    stmt->SetData(0, accountId);
    return ReadHouseholdId(FuryDatabase.Query(stmt));
}

uint32 HouseholdRepository::CountMembers(HouseholdId householdId) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_MEMBER_COUNT);
    stmt->SetData(0, householdId);

    PreparedQueryResult result = FuryDatabase.Query(stmt);
    if (!result)
        return 0;

    Field* fields = result->Fetch();
    return fields[0].Get<uint32>();
}

std::vector<std::pair<uint32, HouseholdId>> HouseholdRepository::LoadMemberships() const
{
    std::vector<std::pair<uint32, HouseholdId>> memberships;

    PreparedQueryResult result =
        FuryDatabase.Query(FuryDatabase.GetPreparedStatement(FURY_SEL_HOUSEHOLD_MEMBERS));
    if (!result)
        return memberships;

    do
    {
        Field* fields = result->Fetch();
        memberships.emplace_back(fields[0].Get<uint32>(), fields[1].Get<HouseholdId>());
    } while (result->NextRow());

    return memberships;
}

std::optional<HouseholdId> HouseholdRepository::CreateOrGet(
    std::string_view slug,
    std::string_view displayName) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_INS_HOUSEHOLD);
    stmt->SetData(0, std::string(slug));
    stmt->SetData(1, std::string(displayName));
    FuryDatabase.Execute(stmt);

    return FindBySlug(slug);
}

void HouseholdRepository::AddMember(HouseholdId householdId, uint32 accountId, uint8 role) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_INS_HOUSEHOLD_MEMBER);
    stmt->SetData(0, householdId);
    stmt->SetData(1, accountId);
    stmt->SetData(2, role);
    FuryDatabase.Execute(stmt);
}

void HouseholdRepository::RemoveMember(HouseholdId householdId, uint32 accountId) const
{
    DatabasePreparedStatement* stmt = FuryDatabase.GetPreparedStatement(FURY_DEL_HOUSEHOLD_MEMBER);
    stmt->SetData(0, householdId);
    stmt->SetData(1, accountId);
    FuryDatabase.Execute(stmt);
}
}
