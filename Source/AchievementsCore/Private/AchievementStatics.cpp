#include "AchievementStatics.h"
#include "AchievementComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementStatics)

void UAchievementStatics::ReportAchievementEvent(APlayerState* Player, FGameplayTag EventTag, int32 Amount, const FGameplayTagContainer& ContextTags)
{
	UAchievementComponent* Component = GetAchievementComponent(Player);
	if (!Component)
	{
		UE_LOG(LogAchievements, Warning, TEXT("ReportAchievementEvent: %s has no UAchievementComponent"), *GetNameSafe(Player));
		return;
	}

	FAchievementEventContext Context;
	Context.EventTag = EventTag;
	Context.Amount = Amount;
	Context.ContextTags = ContextTags;
	Context.Player = Player;
	Component->ReportEvent(Context);
}

void UAchievementStatics::ReportAchievementEventForAll(const UObject* WorldContextObject, FGameplayTag EventTag, int32 Amount, const FGameplayTagContainer& ContextTags)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* Player : GameState->PlayerArray)
	{
		ReportAchievementEvent(Player, EventTag, Amount, ContextTags);
	}
}

UAchievementComponent* UAchievementStatics::GetAchievementComponent(const APlayerState* Player)
{
	return Player ? Player->FindComponentByClass<UAchievementComponent>() : nullptr;
}
