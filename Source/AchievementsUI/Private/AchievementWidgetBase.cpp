#include "AchievementWidgetBase.h"
#include "AchievementComponent.h"
#include "AchievementStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementWidgetBase)

void UAchievementWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UAchievementWidgetBase::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryHandle);
	}

	if (UAchievementComponent* Component = BoundComponent.Get())
	{
		UnbindFromComponent(*Component);
	}
	BoundComponent.Reset();

	Super::NativeDestruct();
}

void UAchievementWidgetBase::TryBind()
{
	const APlayerController* Controller = GetOwningPlayer();
	UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(Controller ? Controller->PlayerState.Get() : nullptr);

	if (!Component)
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RetryHandle, FTimerDelegate::CreateUObject(this, &ThisClass::TryBind), RetryIntervalSeconds, false);
		}
		return;
	}

	BoundComponent = Component;
	BindToComponent(*Component);
	OnAchievementComponentReady();
}
