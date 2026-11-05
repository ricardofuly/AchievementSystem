// Display-ready achievement data for any UI (UMG, CommonUI, MVVM, Slate, HUD).
// Built by UAchievementLocalPlayerSubsystem; the AchievementsUI module is only one consumer of it.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "AchievementViewData.generated.h"

class UAchievementDefinition;
class UTexture2D;

UENUM(BlueprintType)
enum class EAchievementListFilter : uint8
{
	All,
	Unlocked,
	Locked,
	/** Unlocked with rewards still pending. */
	Claimable
};

UENUM(BlueprintType)
enum class EAchievementListSort : uint8
{
	/** Claimable, unlocked, in progress (most advanced first), locked. */
	Status,
	/** Order in which definitions were registered. */
	DefinitionOrder
};

/** Display-ready data for one reward of an achievement. */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSCORE_API FAchievementRewardViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TSoftObjectPtr<UTexture2D> Icon;

	/** This specific reward was already granted (partial grants are possible). */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bGranted = false;
};

/** Display-ready data for one achievement; hidden achievements are already masked. */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSCORE_API FAchievementViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FGameplayTag Id;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	EAchievementState State = EAchievementState::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int32 TargetValue = 1;

	/** 0..1, ready for a progress bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bHasRewards = false;

	/** Same order as the definition's Rewards. Empty while the achievement is masked. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TArray<FAchievementRewardViewData> Rewards;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bClaimable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bMasked = false;

	bool IsUnlocked() const { return State >= EAchievementState::Unlocked; }

	static FAchievementViewData Make(const UAchievementDefinition& Definition, const FAchievementProgressEntry* Entry);
};
