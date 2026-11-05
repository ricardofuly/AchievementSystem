// Platform abstraction (Steam, Epic, consoles...). The core never includes a platform SDK.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class ACHIEVEMENTSCORE_API IAchievementPlatform
{
public:
	virtual ~IAchievementPlatform() = default;

	virtual FName GetPlatformName() const = 0;

	/** False while the SDK is unavailable (offline, not initialized); calls are then queued by the caller. */
	virtual bool IsReady() const = 0;

	/** Idempotent. Returns false if the operation must be retried later. */
	virtual bool UnlockAchievement(FName ApiName) = 0;
	virtual bool SetStat(FName StatName, int32 Value) = 0;

	/** Shows the platform's native progress popup, when supported. */
	virtual void IndicateProgress(FName ApiName, int32 Current, int32 Max) {}

	/** Writes pending changes to the backend (Steam: StoreStats). */
	virtual bool Commit() = 0;

	virtual bool IsAchievementUnlocked(FName ApiName, bool& bOutUnlocked) const = 0;

	virtual void ResetAll() {}
};

/** Platform modules register one of these as a modular feature on startup. */
class ACHIEVEMENTSCORE_API IAchievementPlatformFactory : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("AchievementPlatformFactory"));
		return FeatureName;
	}

	virtual FName GetPlatformName() const = 0;
	virtual TSharedPtr<IAchievementPlatform> CreatePlatform() = 0;
};

/** Fallback used in editor, tests and builds without a platform. Everything succeeds locally. */
class ACHIEVEMENTSCORE_API FNullAchievementPlatform final : public IAchievementPlatform
{
public:
	virtual FName GetPlatformName() const override { return TEXT("Null"); }
	virtual bool IsReady() const override { return true; }
	virtual bool UnlockAchievement(FName ApiName) override { Unlocked.Add(ApiName); return true; }
	virtual bool SetStat(FName StatName, int32 Value) override { return true; }
	virtual bool Commit() override { return true; }
	virtual bool IsAchievementUnlocked(FName ApiName, bool& bOutUnlocked) const override
	{
		bOutUnlocked = Unlocked.Contains(ApiName);
		return true;
	}
	virtual void ResetAll() override { Unlocked.Reset(); }

private:
	TSet<FName> Unlocked;
};
