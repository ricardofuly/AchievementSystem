// Reference reward integration: a minimal replicated wallet plus the granter that feeds it.
// Copy and adapt to your inventory/economy; the core does not depend on these classes.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AchievementReward.h"
#include "AchievementExampleWallet.generated.h"

USTRUCT(BlueprintType)
struct FAchievementExampleBalance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements|Example")
	FGameplayTag CurrencyTag;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements|Example")
	int32 Amount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnExampleWalletChanged);

/** Add to the PlayerState next to UAchievementComponent. */
UCLASS(ClassGroup = (Achievements), meta = (BlueprintSpawnableComponent))
class ACHIEVEMENTSCORE_API UAchievementExampleWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAchievementExampleWalletComponent();

	/** Host only. */
	void AddCurrency(const FGameplayTag& CurrencyTag, int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Achievements|Example")
	int32 GetBalance(FGameplayTag CurrencyTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Achievements|Example")
	FOnExampleWalletChanged OnWalletChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Balances();

	UPROPERTY(ReplicatedUsing = OnRep_Balances)
	TArray<FAchievementExampleBalance> Balances;
};

/** Grants UAchievementReward_Currency into the player's UAchievementExampleWalletComponent. */
UCLASS()
class ACHIEVEMENTSCORE_API UAchievementExampleCurrencyGranter : public UObject, public IAchievementRewardGranter
{
	GENERATED_BODY()

public:
	virtual EAchievementGrantResult GrantReward_Implementation(const UAchievementReward* Reward, APlayerState* Player) override;
};
