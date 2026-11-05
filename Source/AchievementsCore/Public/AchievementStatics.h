// One-line gameplay API. Call from host-side gameplay code (GameMode, damage, objectives).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "AchievementStatics.generated.h"

class APlayerState;
class UAchievementComponent;

UCLASS()
class ACHIEVEMENTSCORE_API UAchievementStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Achievements", meta = (AutoCreateRefTerm = "ContextTags", AdvancedDisplay = "ContextTags"))
	static void ReportAchievementEvent(APlayerState* Player, UPARAM(meta = (Categories = "Event")) FGameplayTag EventTag, int32 Amount, const FGameplayTagContainer& ContextTags);

	/** Reports the same event for every player in the match (e.g. "match won" for a team). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Achievements", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ContextTags", AdvancedDisplay = "ContextTags"))
	static void ReportAchievementEventForAll(const UObject* WorldContextObject, UPARAM(meta = (Categories = "Event")) FGameplayTag EventTag, int32 Amount, const FGameplayTagContainer& ContextTags);

	UFUNCTION(BlueprintPure, Category = "Achievements")
	static UAchievementComponent* GetAchievementComponent(const APlayerState* Player);
};
