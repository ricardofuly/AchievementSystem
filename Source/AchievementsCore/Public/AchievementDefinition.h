// Designer-authored description of one achievement.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AchievementDefinition.generated.h"

class UAchievementCondition;
class UAchievementReward;
class UTexture2D;

UCLASS(BlueprintType, Const)
class ACHIEVEMENTSCORE_API UAchievementDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType AssetType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Identity", meta = (Categories = "Achievement"))
	FGameplayTag Id;

	/** API name configured in the Steamworks partner site (e.g. ACH_KILL_100). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = "Platform")
	FName PlatformApiName;

	/** Optional Steam stat mirroring progress; enables Steam's native progress popup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Platform")
	FName PlatformStatName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	bool bHidden = false;

	/** Events that feed this achievement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (Categories = "Event"))
	FGameplayTagContainer ListenedEvents;

	/** 1 = one-shot; greater than 1 = incremental. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (ClampMin = "1"))
	int32 TargetValue = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progress", meta = (Categories = "Achievement"))
	FGameplayTagContainer Prerequisites;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Progress")
	TArray<TObjectPtr<UAchievementCondition>> Conditions;

	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Rewards")
	TArray<TObjectPtr<UAchievementReward>> Rewards;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
