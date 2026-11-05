// Rewards are pure data; the game grants them through IAchievementRewardGranter.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AchievementReward.generated.h"

class APlayerState;
class UTexture2D;

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ACHIEVEMENTSCORE_API UAchievementReward : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	FText DisplayName;

	/** Shown by the UI next to the achievement; streamed on demand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	TSoftObjectPtr<UTexture2D> Icon;
};

/** Generic item reward; the granter decides what ItemId means for the game. */
UCLASS(meta = (DisplayName = "Item Reward"))
class ACHIEVEMENTSCORE_API UAchievementReward_Item : public UAchievementReward
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	FPrimaryAssetId ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

UCLASS(meta = (DisplayName = "Currency Reward"))
class ACHIEVEMENTSCORE_API UAchievementReward_Currency : public UAchievementReward
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	FGameplayTag CurrencyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (ClampMin = "1"))
	int32 Amount = 1;
};

UENUM(BlueprintType)
enum class EAchievementGrantResult : uint8
{
	Granted,
	/** Temporary failure (e.g. inventory full); the reward stays claimable. */
	Deferred,
	Failed
};

UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class UAchievementRewardGranter : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by game code (inventory, wallet...). Always called on the host. */
class ACHIEVEMENTSCORE_API IAchievementRewardGranter
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Achievements")
	EAchievementGrantResult GrantReward(const UAchievementReward* Reward, APlayerState* Player);
};
