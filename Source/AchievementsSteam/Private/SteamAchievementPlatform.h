// Steamworks implementation of IAchievementPlatform. Runs on each player's own machine.

#pragma once

#include "CoreMinimal.h"
#include "AchievementPlatform.h"

class FSteamAchievementPlatform final : public IAchievementPlatform
{
public:
	virtual FName GetPlatformName() const override { return TEXT("Steam"); }
	virtual bool IsReady() const override;
	virtual bool UnlockAchievement(FName ApiName) override;
	virtual bool SetStat(FName StatName, int32 Value) override;
	virtual void IndicateProgress(FName ApiName, int32 Current, int32 Max) override;
	virtual bool Commit() override;
	virtual bool IsAchievementUnlocked(FName ApiName, bool& bOutUnlocked) const override;
	virtual void ResetAll() override;
};

class FSteamAchievementPlatformFactory final : public IAchievementPlatformFactory
{
public:
	virtual FName GetPlatformName() const override { return TEXT("Steam"); }
	virtual TSharedPtr<IAchievementPlatform> CreatePlatform() override { return MakeShared<FSteamAchievementPlatform>(); }
};
