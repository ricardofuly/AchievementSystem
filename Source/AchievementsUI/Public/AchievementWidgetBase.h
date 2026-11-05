// Base for the reference achievement widgets. Uses only UAchievementLocalPlayerSubsystem,
// the same public API any custom UI can use without these classes.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AchievementWidgetBase.generated.h"

class UAchievementComponent;
class UAchievementLocalPlayerSubsystem;

UCLASS(Abstract)
class ACHIEVEMENTSUI_API UAchievementWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The owning player's achievement API: events, queries and claims. */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	UAchievementLocalPlayerSubsystem* GetAchievements() const { return BoundAchievements.Get(); }

	/** The match component, or null outside a match. Prefer GetAchievements(). */
	UFUNCTION(BlueprintPure, Category = "Achievements")
	UAchievementComponent* GetAchievementComponent() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual void BindToAchievements(UAchievementLocalPlayerSubsystem& InAchievements) {}
	virtual void UnbindFromAchievements(UAchievementLocalPlayerSubsystem& InAchievements) {}

	/** Fired when a match's achievement data is ready (also after map travel). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnAchievementComponentReady();

	UFUNCTION()
	virtual void HandleAchievementsReady();

private:
	TWeakObjectPtr<UAchievementLocalPlayerSubsystem> BoundAchievements;
};
