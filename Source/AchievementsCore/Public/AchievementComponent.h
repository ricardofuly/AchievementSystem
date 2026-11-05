// Per-player, host-authoritative achievement state. Add it to your PlayerState.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AchievementTypes.h"
#include "AchievementComponent.generated.h"

class UAchievementDefinition;
class UAchievementLocalPlayerSubsystem;
class UAchievementSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAchievementProgressChanged, const FAchievementProgressEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAchievementUnlocked, const UAchievementDefinition*, Definition);

/**
 * Lives on APlayerState so it survives respawns and seamless travel.
 *
 * Authority model (P2P / listen server):
 *  - The host evaluates events, updates progress and grants rewards.
 *  - Progress replicates to the owning client only (FastArray, deltas).
 *  - The owning client persists it locally and pushes it to Steam,
 *    since Steam only accepts achievements for the signed-in user.
 */
UCLASS(ClassGroup = (Achievements), meta = (BlueprintSpawnableComponent))
class ACHIEVEMENTSCORE_API UAchievementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAchievementComponent();

	/** Host only. Feeds an event to every achievement listening to it. */
	void ReportEvent(const FAchievementEventContext& Context);

	/** Asks the host to grant pending rewards (manual-claim flow). Safe to call from the owning client. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void RequestClaimRewards(FGameplayTag AchievementId);

	/** Retries every unlocked achievement whose rewards are still pending (manual claim or deferred grants). */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void RequestClaimAllRewards();

	/**
	 * Host only. Retries Deferred rewards without player input; call it when the blocker clears
	 * (e.g. inventory space freed). Rewards that returned Failed are skipped.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Achievements")
	void RetryPendingRewards();

	/** Unlocked achievements whose rewards have not been fully granted yet. */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	TArray<FGameplayTag> GetClaimableAchievements() const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool GetProgress(FGameplayTag AchievementId, FAchievementProgressEntry& OutEntry) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	bool IsUnlocked(FGameplayTag AchievementId) const;

	const TArray<FAchievementProgressEntry>& GetAllProgress() const { return Progress.Entries; }

	/** Called by FastArray callbacks on the owning client, and directly on the host for its own player. */
	void HandleEntryChanged(const FAchievementProgressEntry& Entry);

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementProgressChanged OnProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnAchievementUnlocked;

	/** Local only: an unlocked achievement has rewards waiting to be claimed. */
	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnRewardsClaimable;

	/** Local only: every reward of the achievement was granted. */
	UPROPERTY(BlueprintAssignable, Category = "Achievements")
	FOnAchievementUnlocked OnRewardsClaimed;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSubmitSnapshot(const TArray<FAchievementSnapshotEntry>& Snapshot);

	UFUNCTION(Server, Reliable)
	void ServerClaimRewards(FGameplayTag AchievementId);

	UFUNCTION(Server, Reliable)
	void ServerClaimAllRewards();

	void ApplySnapshot(const TArray<FAchievementSnapshotEntry>& Snapshot);
	bool ArePrerequisitesMet(const UAchievementDefinition& Definition) const;
	bool AreConditionsMet(const UAchievementDefinition& Definition, const FAchievementEventContext& Context) const;
	void Unlock(FAchievementProgressEntry& Entry, const UAchievementDefinition& Definition);
	/** bRetryFailed = false skips rewards that already returned Failed (automatic retries). */
	void ClaimRewards(FAchievementProgressEntry& Entry, const UAchievementDefinition& Definition, bool bRetryFailed = true);
	void CommitEntry(FAchievementProgressEntry& Entry);

	/** Sends the local save to the host. False while this PlayerState is not yet linked to a local controller. */
	bool TryStartHandshake();
	void RetryHandshake();

	bool IsLocallyOwned() const;
	APlayerState* GetOwningPlayerState() const;
	UAchievementSubsystem* GetAchievementSubsystem() const;
	UAchievementLocalPlayerSubsystem* GetLocalPlayerSubsystem() const;

	UPROPERTY(Replicated)
	FAchievementProgressArray Progress;

	/** Ids already announced locally; prevents re-toasting on load and on repeated deltas. */
	TSet<FGameplayTag> AnnouncedUnlocks;
	TSet<FGameplayTag> AnnouncedClaims;

	/** Local player subsystem forwarding this component's events; set once the handshake ran. */
	TWeakObjectPtr<UAchievementLocalPlayerSubsystem> BoundLocalSubsystem;

	FTimerHandle HandshakeRetryHandle;
	int32 HandshakeAttempts = 0;
	bool bHandshakeSent = false;
	bool bSnapshotApplied = false;

	static constexpr float HandshakeRetrySeconds = 0.2f;
	static constexpr int32 MaxHandshakeAttempts = 50;
};
