#include "AchievementWidgetBase.h"
#include "AchievementLocalPlayerSubsystem.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementWidgetBase)

void UAchievementWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	UAchievementLocalPlayerSubsystem* Subsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}

	BoundAchievements = Subsystem;
	Subsystem->OnAchievementsReady.AddDynamic(this, &ThisClass::HandleAchievementsReady);
	BindToAchievements(*Subsystem);

	if (Subsystem->IsInMatch())
	{
		HandleAchievementsReady();
	}
}

void UAchievementWidgetBase::NativeDestruct()
{
	if (UAchievementLocalPlayerSubsystem* Subsystem = BoundAchievements.Get())
	{
		Subsystem->OnAchievementsReady.RemoveDynamic(this, &ThisClass::HandleAchievementsReady);
		UnbindFromAchievements(*Subsystem);
	}
	BoundAchievements.Reset();

	Super::NativeDestruct();
}

void UAchievementWidgetBase::HandleAchievementsReady()
{
	OnAchievementComponentReady();
}

UAchievementComponent* UAchievementWidgetBase::GetAchievementComponent() const
{
	const UAchievementLocalPlayerSubsystem* Subsystem = BoundAchievements.Get();
	return Subsystem ? Subsystem->GetAchievementComponent() : nullptr;
}
