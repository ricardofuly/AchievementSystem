// Base for achievement widgets: finds the local player's UAchievementComponent and binds to it.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AchievementWidgetBase.generated.h"

class UAchievementComponent;

/**
 * On clients the PlayerState may replicate after the HUD is created, so binding retries
 * until the component exists. Subclasses override BindToComponent / UnbindFromComponent.
 */
UCLASS(Abstract)
class ACHIEVEMENTSUI_API UAchievementWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Achievements")
	UAchievementComponent* GetAchievementComponent() const { return BoundComponent.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	virtual void BindToComponent(UAchievementComponent& Component) {}
	virtual void UnbindFromComponent(UAchievementComponent& Component) {}

	/** Fired once the local player's component is found and bound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnAchievementComponentReady();

private:
	void TryBind();

	TWeakObjectPtr<UAchievementComponent> BoundComponent;
	FTimerHandle RetryHandle;

	static constexpr float RetryIntervalSeconds = 0.25f;
};
