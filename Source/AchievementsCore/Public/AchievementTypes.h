// Core value types shared by every layer of the achievement system.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AchievementTypes.generated.h"

ACHIEVEMENTSCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAchievements, Log, All);

class APlayerState;
class UAchievementComponent;
struct FAchievementProgressArray;

UENUM(BlueprintType)
enum class EAchievementState : uint8
{
	Locked,
	InProgress,
	Unlocked,
	RewardClaimed
};

/** One achievement's progress for one player. Replicated as a FastArray item (deltas only). */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSCORE_API FAchievementProgressEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FGameplayTag AchievementId;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	EAchievementState State = EAchievementState::Locked;

	/** UTC ticks of the unlock moment, 0 while locked. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int64 UnlockedAtUtcTicks = 0;

	/** Bit N set = Rewards[N] already granted. Makes partial grants safe to retry. */
	UPROPERTY()
	uint32 ClaimedRewardMask = 0;

	/** Host only, not replicated or saved: Rewards[N] returned Failed this session. Skipped by automatic retries. */
	uint32 FailedRewardMask = 0;

	bool IsUnlocked() const { return State >= EAchievementState::Unlocked; }

	/** Host: folds a saved snapshot in without ever lowering progress. Returns true if anything changed. */
	bool MergeFrom(const FAchievementSnapshotEntry& Saved, int32 TargetValue);

	void PostReplicatedAdd(const FAchievementProgressArray& InArray);
	void PostReplicatedChange(const FAchievementProgressArray& InArray);
};

USTRUCT()
struct ACHIEVEMENTSCORE_API FAchievementProgressArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FAchievementProgressEntry> Entries;

	/** Set by the owning component so item callbacks can reach it. Never replicated. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UAchievementComponent> Owner = nullptr;

	FAchievementProgressEntry* Find(const FGameplayTag& Id);
	const FAchievementProgressEntry* Find(const FGameplayTag& Id) const;
	FAchievementProgressEntry& FindOrAdd(const FGameplayTag& Id);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAchievementProgressEntry, FAchievementProgressArray>(Entries, DeltaParams, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FAchievementProgressArray> : public TStructOpsTypeTraitsBase2<FAchievementProgressArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** Plain snapshot used for persistence and for the client -> host handshake (P2P). */
USTRUCT()
struct ACHIEVEMENTSCORE_API FAchievementSnapshotEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGameplayTag AchievementId;

	UPROPERTY(SaveGame)
	int32 Value = 0;

	UPROPERTY(SaveGame)
	EAchievementState State = EAchievementState::Locked;

	UPROPERTY(SaveGame)
	int64 UnlockedAtUtcTicks = 0;

	UPROPERTY(SaveGame)
	uint32 ClaimedRewardMask = 0;

	/** Local save: folds replicated progress in without ever lowering it. Returns true if anything changed. */
	bool MergeFrom(const FAchievementProgressEntry& Entry);
};

/** Context handed to conditions so they can gate progress without knowing the caller. */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSCORE_API FAchievementEventContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Achievements")
	FGameplayTag EventTag;

	UPROPERTY(BlueprintReadWrite, Category = "Achievements")
	int32 Amount = 1;

	/** Optional extra tags describing the event (difficulty, weapon, map...). */
	UPROPERTY(BlueprintReadWrite, Category = "Achievements")
	FGameplayTagContainer ContextTags;

	UPROPERTY(BlueprintReadWrite, Category = "Achievements")
	TObjectPtr<APlayerState> Player = nullptr;
};
