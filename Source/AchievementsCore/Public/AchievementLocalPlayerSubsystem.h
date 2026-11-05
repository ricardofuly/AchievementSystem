// Per local player persistence and platform sync. One instance per split-screen player.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "AchievementSaveGame.h"
#include "AchievementLocalPlayerSubsystem.generated.h"

class UAchievementSubsystem;

/**
 * Owns one player's local save and pending platform operations.
 *
 * Split-screen: every local player gets their own save slot. Only the primary local player
 * (the one signed in to Steam) syncs with the platform; the others are local-only guests.
 */
UCLASS()
class ACHIEVEMENTSCORE_API UAchievementLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	TArray<FAchievementSnapshotEntry> GetLocalSnapshot() const { return SaveData.Entries; }

	/** Merges the entry into the save (never lowers progress) and queues platform operations. */
	void HandleLocalProgress(const FAchievementProgressEntry& Entry);

	/** Forces pending platform operations out now (checkpoints, match end). No-op for guests. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void FlushPlatform();

	/** True for the local player that owns the platform account (Steam). */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool IsPrimaryPlayer() const;

#if !UE_BUILD_SHIPPING
	/** Debug only: wipes this player's save and, for the primary player, the platform achievements. */
	void ResetLocalData();
#endif

private:
	FString MakeSlotName() const;
	UAchievementSubsystem* GetAchievementSubsystem() const;
	FAchievementSnapshotEntry& FindOrAddSaved(const FGameplayTag& AchievementId);
	void QueueSavedUnlocksForPlatform();
	void ReconcileWithPlatform();
	bool TickCommit(float DeltaTime);

	TUniquePtr<IAchievementStorage> Storage;
	FAchievementSaveData SaveData;
	FTSTicker::FDelegateHandle CommitTickerHandle;

	bool bSaveDirty = false;
	bool bPlatformDirty = false;
	bool bReconciled = false;
};
