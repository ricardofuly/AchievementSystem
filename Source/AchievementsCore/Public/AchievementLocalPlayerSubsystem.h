// Per local player persistence and platform sync. One instance per split-screen player.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "AchievementComponent.h"
#include "AchievementSaveGame.h"
#include "AchievementViewData.h"
#include "AchievementLocalPlayerSubsystem.generated.h"

class UAchievementSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAchievementsReady);

/**
 * The single entry point for a local player's achievements, and the API any UI should use.
 *
 * - Events are always bindable, even before the PlayerState replicates; they are forwarded from
 *   the player's UAchievementComponent once it exists.
 * - Queries work anywhere: in a match they read live progress, elsewhere (main menu) the local save.
 * - Owns the player's local save and pending platform operations.
 *
 * Split-screen: every local player gets their own instance and save slot. Only the primary local
 * player (the one signed in to Steam) syncs with the platform; the others are local-only guests.
 */
UCLASS()
class ACHIEVEMENTSCORE_API UAchievementLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Events (for UI) ----------------------------------------------------

	/** A match's achievement component was bound (or replaced after travel): refresh any list. */
	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementsReady OnAchievementsReady;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementProgressChanged OnProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnAchievementUnlocked;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnRewardsClaimable;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnRewardsClaimed;

	// --- Queries (for UI) ---------------------------------------------------

	/** Display-ready achievements, filtered and sorted. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	TArray<FAchievementViewData> GetAchievements(EAchievementListFilter Filter = EAchievementListFilter::All,
		EAchievementListSort Sort = EAchievementListSort::Status, bool bIncludeHiddenLocked = true) const;

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	bool GetAchievement(FGameplayTag AchievementId, FAchievementViewData& OutView) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetUnlockedCount() const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetTotalCount() const;

	/** True while a match component is bound (live progress, claims possible). */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool IsInMatch() const { return BoundComponent.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Achievements")
	UAchievementComponent* GetAchievementComponent() const { return BoundComponent.Get(); }

	// --- Commands (for UI) --------------------------------------------------

	/** Asks the host for this achievement's pending rewards. Needs a match. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void ClaimRewards(FGameplayTag AchievementId);

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void ClaimAllRewards();

	// --- Internal -----------------------------------------------------------

	/** Called by the player's UAchievementComponent once it knows it is locally owned. */
	void BindAchievementComponent(UAchievementComponent& Component);
	void UnbindAchievementComponent(UAchievementComponent& Component);

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
	UFUNCTION()
	void ForwardProgressChanged(const FAchievementProgressEntry& Entry);

	UFUNCTION()
	void ForwardUnlocked(const UAchievementDefinition* Definition);

	UFUNCTION()
	void ForwardRewardsClaimable(const UAchievementDefinition* Definition);

	UFUNCTION()
	void ForwardRewardsClaimed(const UAchievementDefinition* Definition);

	/** Live progress in a match, the local save otherwise. */
	bool FindProgress(const FGameplayTag& AchievementId, FAchievementProgressEntry& OutEntry) const;

	FString MakeSlotName() const;
	UAchievementSubsystem* GetAchievementSubsystem() const;
	FAchievementSnapshotEntry& FindOrAddSaved(const FGameplayTag& AchievementId);
	void QueueSavedUnlocksForPlatform();
	void ReconcileWithPlatform();
	bool TickCommit(float DeltaTime);

	TWeakObjectPtr<UAchievementComponent> BoundComponent;

	TUniquePtr<IAchievementStorage> Storage;
	FAchievementSaveData SaveData;
	FTSTicker::FDelegateHandle CommitTickerHandle;

	bool bSaveDirty = false;
	bool bPlatformDirty = false;
	bool bReconciled = false;
};
