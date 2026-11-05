#include "Examples/AchievementExampleWallet.h"
#include "AchievementTypes.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementExampleWallet)

UAchievementExampleWalletComponent::UAchievementExampleWalletComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAchievementExampleWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UAchievementExampleWalletComponent, Balances, COND_OwnerOnly);
}

void UAchievementExampleWalletComponent::AddCurrency(const FGameplayTag& CurrencyTag, int32 Amount)
{
	if (!ensure(GetOwner()->HasAuthority()) || !CurrencyTag.IsValid() || Amount <= 0)
	{
		return;
	}

	FAchievementExampleBalance* Balance = Balances.FindByPredicate(
		[&CurrencyTag](const FAchievementExampleBalance& Candidate) { return Candidate.CurrencyTag == CurrencyTag; });
	if (!Balance)
	{
		Balance = &Balances.AddDefaulted_GetRef();
		Balance->CurrencyTag = CurrencyTag;
	}
	Balance->Amount += Amount;

	// The host's own player gets no OnRep.
	OnRep_Balances();
}

int32 UAchievementExampleWalletComponent::GetBalance(FGameplayTag CurrencyTag) const
{
	const FAchievementExampleBalance* Balance = Balances.FindByPredicate(
		[&CurrencyTag](const FAchievementExampleBalance& Candidate) { return Candidate.CurrencyTag == CurrencyTag; });
	return Balance ? Balance->Amount : 0;
}

void UAchievementExampleWalletComponent::OnRep_Balances()
{
	OnWalletChanged.Broadcast();
}

EAchievementGrantResult UAchievementExampleCurrencyGranter::GrantReward_Implementation(const UAchievementReward* Reward, APlayerState* Player)
{
	const UAchievementReward_Currency* Currency = Cast<UAchievementReward_Currency>(Reward);
	UAchievementExampleWalletComponent* Wallet = Player ? Player->FindComponentByClass<UAchievementExampleWalletComponent>() : nullptr;
	if (!Currency || !Wallet)
	{
		UE_LOG(LogAchievements, Warning, TEXT("Currency granter: missing reward data or wallet on %s"), *GetNameSafe(Player));
		return EAchievementGrantResult::Failed;
	}

	Wallet->AddCurrency(Currency->CurrencyTag, Currency->Amount);
	UE_LOG(LogAchievements, Log, TEXT("Granted %d %s to %s"), Currency->Amount, *Currency->CurrencyTag.ToString(), *GetNameSafe(Player));
	return EAchievementGrantResult::Granted;
}
