// Queued, one-at-a-time notifications for unlocks and claimed rewards.

#pragma once

#include "CoreMinimal.h"
#include "AchievementWidgetBase.h"
#include "AchievementToastWidget.generated.h"

class UAchievementDefinition;

UENUM(BlueprintType)
enum class EAchievementToastType : uint8
{
	Unlocked,
	RewardsClaimed
};

USTRUCT(BlueprintType)
struct ACHIEVEMENTSUI_API FAchievementToastData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TObjectPtr<const UAchievementDefinition> Definition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	EAchievementToastType Type = EAchievementToastType::Unlocked;
};

/**
 * Blueprint implements ShowToast (play the intro animation) and, when AutoFinishSeconds is 0,
 * calls FinishToast at the end of the outro. The next queued toast is shown right after.
 */
UCLASS(Abstract)
class ACHIEVEMENTSUI_API UAchievementToastWidget : public UAchievementWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void FinishToast();

protected:
	virtual void BindToComponent(UAchievementComponent& Component) override;
	virtual void UnbindFromComponent(UAchievementComponent& Component) override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void ShowToast(const FAchievementToastData& Toast);

	/** Fired when the last queued toast finished; hide the widget here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnQueueEmpty();

	/** 0 = Blueprint calls FinishToast itself (e.g. at the end of an animation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements", meta = (ClampMin = "0.0"))
	float AutoFinishSeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Achievements")
	bool bShowRewardToasts = true;

private:
	UFUNCTION()
	void HandleUnlocked(const UAchievementDefinition* Definition);

	UFUNCTION()
	void HandleRewardsClaimed(const UAchievementDefinition* Definition);

	void Enqueue(const UAchievementDefinition* Definition, EAchievementToastType Type);
	void ShowNext();

	UPROPERTY(Transient)
	TArray<FAchievementToastData> Queue;

	FTimerHandle AutoFinishHandle;
	bool bIsShowing = false;
};
