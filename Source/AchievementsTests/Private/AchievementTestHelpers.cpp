#include "AchievementTestHelpers.h"
#include "AchievementComponent.h"
#include "AchievementDefinition.h"
#include "AchievementSettings.h"
#include "AchievementStatics.h"
#include "AchievementSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementTestHelpers)

namespace AchievementTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(OneShot, "AchievementTests.Achievement.OneShot");
	UE_DEFINE_GAMEPLAY_TAG(Counter, "AchievementTests.Achievement.Counter");
	UE_DEFINE_GAMEPLAY_TAG(Gated, "AchievementTests.Achievement.Gated");
	UE_DEFINE_GAMEPLAY_TAG(Conditional, "AchievementTests.Achievement.Conditional");
	UE_DEFINE_GAMEPLAY_TAG(Rewarded, "AchievementTests.Achievement.Rewarded");
	UE_DEFINE_GAMEPLAY_TAG(EventPing, "AchievementTests.Event.Ping");
	UE_DEFINE_GAMEPLAY_TAG(EventOther, "AchievementTests.Event.Other");
	UE_DEFINE_GAMEPLAY_TAG(ContextHard, "AchievementTests.Context.Hard");
	UE_DEFINE_GAMEPLAY_TAG(Gold, "AchievementTests.Currency.Gold");
	UE_DEFINE_GAMEPLAY_TAG(Gems, "AchievementTests.Currency.Gems");
}

// ---------------------------------------------------------------------------
// Granter
// ---------------------------------------------------------------------------

EAchievementGrantResult UAchievementTestGranter::GrantReward_Implementation(const UAchievementReward* Reward, APlayerState* Player)
{
	++Calls;

	const UAchievementReward_Currency* Currency = Cast<UAchievementReward_Currency>(Reward);
	if (!Currency)
	{
		return EAchievementGrantResult::Failed;
	}

	const EAchievementGrantResult* Scripted = ResultByCurrency.Find(Currency->CurrencyTag);
	const EAchievementGrantResult Result = Scripted ? *Scripted : EAchievementGrantResult::Granted;
	if (Result == EAchievementGrantResult::Granted)
	{
		++GrantedByCurrency.FindOrAdd(Currency->CurrencyTag);
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------

void FAchievementTestEnvironment::Setup(bool bAutoClaimRewards)
{
	UAchievementSettings* Settings = GetMutableDefault<UAchievementSettings>();
	bPreviousAutoClaim = Settings->bAutoClaimRewards;
	Settings->bAutoClaimRewards = bAutoClaimRewards;

	GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone();

	World = GameInstance->GetWorld();
	Subsystem = GameInstance->GetSubsystem<UAchievementSubsystem>();

	// Overrides any project mapping for currency rewards with the scripted granter.
	Granter = NewObject<UAchievementTestGranter>(Subsystem);
	Subsystem->RegisterRewardGranter(UAchievementReward_Currency::StaticClass(), Granter);
}

void FAchievementTestEnvironment::Teardown()
{
	if (GameInstance)
	{
		GameInstance->Shutdown();
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		GameInstance->RemoveFromRoot();
	}

	GameInstance = nullptr;
	World = nullptr;
	Subsystem = nullptr;
	Granter = nullptr;

	GetMutableDefault<UAchievementSettings>()->bAutoClaimRewards = bPreviousAutoClaim;
}

UAchievementComponent* FAchievementTestEnvironment::SpawnPlayer()
{
	APlayerState* PlayerState = World->SpawnActor<APlayerState>();
	UAchievementComponent* Component = NewObject<UAchievementComponent>(PlayerState);
	Component->RegisterComponent();
	return Component;
}

UAchievementDefinition* FAchievementTestEnvironment::AddDefinition(const FGameplayTag& Id, const FGameplayTag& Event, int32 TargetValue)
{
	UAchievementDefinition* Definition = NewObject<UAchievementDefinition>(GetTransientPackage());
	Definition->Id = Id;
	Definition->ListenedEvents.AddTag(Event);
	Definition->TargetValue = TargetValue;
	Subsystem->RegisterDefinition(Definition);
	return Definition;
}

void FAchievementTestEnvironment::AddCurrencyReward(UAchievementDefinition& Definition, const FGameplayTag& Currency)
{
	UAchievementReward_Currency* Reward = NewObject<UAchievementReward_Currency>(&Definition);
	Reward->CurrencyTag = Currency;
	Reward->Amount = 1;
	Definition.Rewards.Add(Reward);
}

void FAchievementTestEnvironment::Report(UAchievementComponent& Component, const FGameplayTag& Event, int32 Amount, const FGameplayTagContainer& ContextTags)
{
	UAchievementStatics::ReportAchievementEvent(Component.GetOwner<APlayerState>(), Event, Amount, ContextTags);
}

EAchievementState FAchievementTestEnvironment::GetState(const UAchievementComponent& Component, const FGameplayTag& Id)
{
	FAchievementProgressEntry Entry;
	return Component.GetProgress(Id, Entry) ? Entry.State : EAchievementState::Locked;
}

int32 FAchievementTestEnvironment::GetValue(const UAchievementComponent& Component, const FGameplayTag& Id)
{
	FAchievementProgressEntry Entry;
	return Component.GetProgress(Id, Entry) ? Entry.Value : 0;
}
