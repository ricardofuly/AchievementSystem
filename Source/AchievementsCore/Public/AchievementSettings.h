// Project Settings > Plugins > Achievements

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AchievementReward.h"
#include "AchievementSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Achievements"))
class ACHIEVEMENTSCORE_API UAchievementSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Platform factory to use; falls back to Null when it is not available. */
	UPROPERTY(Config, EditAnywhere, Category = "Platform")
	FName PreferredPlatform = TEXT("Steam");

	/** Seconds between platform commits (Steam StoreStats is rate limited). */
	UPROPERTY(Config, EditAnywhere, Category = "Platform", meta = (ClampMin = "1.0"))
	float CommitIntervalSeconds = 5.f;

	/** Grant rewards as soon as an achievement unlocks; otherwise the player claims them. */
	UPROPERTY(Config, EditAnywhere, Category = "Rewards")
	bool bAutoClaimRewards = true;

	/**
	 * Granter class instantiated on startup for each reward class (no code needed).
	 * Granters must implement IAchievementRewardGranter. Code can still add more via RegisterRewardGranter.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Rewards")
	TMap<TSoftClassPtr<UAchievementReward>, TSoftClassPtr<UObject>> DefaultGranters;

	UPROPERTY(Config, EditAnywhere, Category = "Persistence")
	FString SaveSlotName = TEXT("Achievements");

	/** Upper bound on the snapshot a client may submit to the host. */
	UPROPERTY(Config, EditAnywhere, Category = "Networking", meta = (ClampMin = "1"))
	int32 MaxSnapshotEntries = 1024;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
