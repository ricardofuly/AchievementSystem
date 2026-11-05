// Extensible gate evaluated before an event counts toward an achievement.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AchievementTypes.h"
#include "AchievementCondition.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ACHIEVEMENTSCORE_API UAchievementCondition : public UObject
{
	GENERATED_BODY()

public:
	/** Runs on the host only. Return false to ignore the event for this achievement. */
	UFUNCTION(BlueprintNativeEvent, Category = "Achievements")
	bool IsSatisfied(const FAchievementEventContext& Context) const;

protected:
	virtual bool IsSatisfied_Implementation(const FAchievementEventContext& Context) const { return true; }
};

/** Passes only when the event carries all of the required context tags. */
UCLASS(meta = (DisplayName = "Requires Context Tags"))
class ACHIEVEMENTSCORE_API UAchievementCondition_RequiresTags : public UAchievementCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Achievements")
	FGameplayTagContainer RequiredTags;

protected:
	virtual bool IsSatisfied_Implementation(const FAchievementEventContext& Context) const override
	{
		return Context.ContextTags.HasAll(RequiredTags);
	}
};
