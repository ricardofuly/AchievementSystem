#include "AchievementToastWidget.h"
#include "AchievementLocalPlayerSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementToastWidget)

void UAchievementToastWidget::BindToAchievements(UAchievementLocalPlayerSubsystem& InAchievements)
{
	InAchievements.OnAchievementUnlocked.AddDynamic(this, &ThisClass::HandleUnlocked);
	InAchievements.OnRewardsClaimed.AddDynamic(this, &ThisClass::HandleRewardsClaimed);
}

void UAchievementToastWidget::UnbindFromAchievements(UAchievementLocalPlayerSubsystem& InAchievements)
{
	InAchievements.OnAchievementUnlocked.RemoveDynamic(this, &ThisClass::HandleUnlocked);
	InAchievements.OnRewardsClaimed.RemoveDynamic(this, &ThisClass::HandleRewardsClaimed);
}

void UAchievementToastWidget::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFinishHandle);
	}
	Queue.Reset();
	bIsShowing = false;

	Super::NativeDestruct();
}

void UAchievementToastWidget::HandleUnlocked(const UAchievementDefinition* Definition)
{
	Enqueue(Definition, EAchievementToastType::Unlocked);
}

void UAchievementToastWidget::HandleRewardsClaimed(const UAchievementDefinition* Definition)
{
	if (bShowRewardToasts)
	{
		Enqueue(Definition, EAchievementToastType::RewardsClaimed);
	}
}

void UAchievementToastWidget::Enqueue(const UAchievementDefinition* Definition, EAchievementToastType Type)
{
	if (!Definition)
	{
		return;
	}

	FAchievementToastData& Toast = Queue.AddDefaulted_GetRef();
	Toast.Definition = Definition;
	Toast.Type = Type;

	if (!bIsShowing)
	{
		ShowNext();
	}
}

void UAchievementToastWidget::ShowNext()
{
	if (Queue.IsEmpty())
	{
		bIsShowing = false;
		OnQueueEmpty();
		return;
	}

	bIsShowing = true;
	const FAchievementToastData Toast = Queue[0];
	Queue.RemoveAt(0);
	ShowToast(Toast);

	if (AutoFinishSeconds > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(AutoFinishHandle, FTimerDelegate::CreateUObject(this, &ThisClass::FinishToast), AutoFinishSeconds, false);
	}
}

void UAchievementToastWidget::FinishToast()
{
	if (!bIsShowing)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoFinishHandle);
	ShowNext();
}
