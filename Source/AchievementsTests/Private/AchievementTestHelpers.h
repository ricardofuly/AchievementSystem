// Shared fixtures for the achievement automation tests.

#pragma once

#include "CoreMinimal.h"
#include "AchievementReward.h"
#include "AchievementTypes.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "AchievementTestHelpers.generated.h"

class UAchievementComponent;
class UAchievementDefinition;
class UAchievementSubsystem;
class UGameInstance;

namespace AchievementTestTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(OneShot);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Counter);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gated);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Conditional);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rewarded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventPing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventOther);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(ContextHard);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gold);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gems);
}

/** Granter whose result is scripted per currency, recording every call. */
UCLASS(Transient)
class UAchievementTestGranter : public UObject, public IAchievementRewardGranter
{
	GENERATED_BODY()

public:
	virtual EAchievementGrantResult GrantReward_Implementation(const UAchievementReward* Reward, APlayerState* Player) override;

	/** Result returned for a currency; Granted when not set. */
	TMap<FGameplayTag, EAchievementGrantResult> ResultByCurrency;

	int32 GetGranted(const FGameplayTag& Currency) const { return GrantedByCurrency.FindRef(Currency); }
	int32 GetCalls() const { return Calls; }

private:
	TMap<FGameplayTag, int32> GrantedByCurrency;
	int32 Calls = 0;
};

/**
 * A standalone game instance with a dummy game world, the achievement subsystem and a scripted granter.
 * Players are bare PlayerStates with authority, so the host-side logic runs without networking.
 */
struct FAchievementTestEnvironment
{
	void Setup(bool bAutoClaimRewards);
	void Teardown();

	UAchievementComponent* SpawnPlayer();
	UAchievementDefinition* AddDefinition(const FGameplayTag& Id, const FGameplayTag& Event, int32 TargetValue = 1);
	void AddCurrencyReward(UAchievementDefinition& Definition, const FGameplayTag& Currency);

	void Report(UAchievementComponent& Component, const FGameplayTag& Event, int32 Amount = 1, const FGameplayTagContainer& ContextTags = FGameplayTagContainer());

	static EAchievementState GetState(const UAchievementComponent& Component, const FGameplayTag& Id);
	static int32 GetValue(const UAchievementComponent& Component, const FGameplayTag& Id);

	UGameInstance* GameInstance = nullptr;
	UWorld* World = nullptr;
	UAchievementSubsystem* Subsystem = nullptr;
	UAchievementTestGranter* Granter = nullptr;

private:
	bool bPreviousAutoClaim = true;
};

/** Spec flags shared by every achievement test. */
constexpr EAutomationTestFlags AchievementTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
