#include "SteamAchievementPlatform.h"
#include "AchievementTypes.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

// Since Steamworks SDK 1.61 the user's stats are fetched automatically at SteamAPI_Init,
// so there is no RequestCurrentStats round-trip. SteamAPI_RunCallbacks is pumped by OnlineSubsystemSteam.

namespace SteamAchievementPlatform
{
	static ISteamUserStats* GetUserStats()
	{
		return SteamAPI_IsSteamRunning() ? SteamUserStats() : nullptr;
	}
}

bool FSteamAchievementPlatform::IsReady() const
{
	return SteamAchievementPlatform::GetUserStats() != nullptr;
}

bool FSteamAchievementPlatform::UnlockAchievement(FName ApiName)
{
	ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats();
	if (!Stats)
	{
		return false;
	}

	const bool bOk = Stats->SetAchievement(TCHAR_TO_UTF8(*ApiName.ToString()));
	UE_CLOG(!bOk, LogAchievements, Warning, TEXT("Steam rejected achievement '%s' (check the API name on the partner site)"), *ApiName.ToString());
	return bOk;
}

bool FSteamAchievementPlatform::SetStat(FName StatName, int32 Value)
{
	ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats();
	if (!Stats)
	{
		return false;
	}

	const bool bOk = Stats->SetStat(TCHAR_TO_UTF8(*StatName.ToString()), Value);
	UE_CLOG(!bOk, LogAchievements, Warning, TEXT("Steam rejected stat '%s'"), *StatName.ToString());
	return bOk;
}

void FSteamAchievementPlatform::IndicateProgress(FName ApiName, int32 Current, int32 Max)
{
	if (ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats())
	{
		Stats->IndicateAchievementProgress(TCHAR_TO_UTF8(*ApiName.ToString()), static_cast<uint32>(Current), static_cast<uint32>(Max));
	}
}

bool FSteamAchievementPlatform::Commit()
{
	ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats();
	return Stats && Stats->StoreStats();
}

bool FSteamAchievementPlatform::IsAchievementUnlocked(FName ApiName, bool& bOutUnlocked) const
{
	ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats();
	return Stats && Stats->GetAchievement(TCHAR_TO_UTF8(*ApiName.ToString()), &bOutUnlocked);
}

void FSteamAchievementPlatform::ResetAll()
{
#if !UE_BUILD_SHIPPING
	if (ISteamUserStats* Stats = SteamAchievementPlatform::GetUserStats())
	{
		Stats->ResetAllStats(/*bAchievementsToo*/ true);
		Stats->StoreStats();
	}
#endif
}
