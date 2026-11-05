// Debug console commands. Compiled out of Shipping builds.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "AchievementComponent.h"
#include "AchievementDefinition.h"
#include "AchievementLocalPlayerSubsystem.h"
#include "AchievementSettings.h"
#include "AchievementStatics.h"
#include "AchievementSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

namespace AchievementConsole
{
	static UAchievementSubsystem* GetSubsystem(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr;
	}

	static bool IsHost(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client;
	}

	static const TCHAR* StateToString(EAchievementState State)
	{
		switch (State)
		{
		case EAchievementState::Locked:        return TEXT("Locked");
		case EAchievementState::InProgress:    return TEXT("InProgress");
		case EAchievementState::Unlocked:      return TEXT("Unlocked");
		case EAchievementState::RewardClaimed: return TEXT("RewardClaimed");
		}
		return TEXT("?");
	}

	static void Report(bool bPassed, const FString& Check)
	{
		if (bPassed)
		{
			UE_LOG(LogAchievements, Display, TEXT("[SelfTest] PASS  %s"), *Check);
		}
		else
		{
			UE_LOG(LogAchievements, Error, TEXT("[SelfTest] FAIL  %s"), *Check);
		}
	}

	static const UAchievementDefinition* ResolveDefinition(const UAchievementSubsystem& Subsystem, const TArray<FString>& Args)
	{
		if (Args.Num() > 0)
		{
			const FGameplayTag Id = FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
			return Subsystem.FindDefinition(Id);
		}

		const TArray<UAchievementDefinition*> All = Subsystem.GetAllDefinitions();
		return All.Num() > 0 ? All[0] : nullptr;
	}

	/** Client-side checks run after replication had time to arrive. Only reachable in single-process PIE. */
	static void VerifyClients(const UWorld* HostWorld, FGameplayTag AchievementId)
	{
		int32 ClientsChecked = 0;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const UWorld* World = Context.World();
			if (!World || World == HostWorld || World->GetNetMode() != NM_Client)
			{
				continue;
			}

			// Every local controller in the window: one per split-screen player.
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* Controller = It->Get();
				if (!Controller || !Controller->IsLocalController())
				{
					continue;
				}

				++ClientsChecked;
				const UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(Controller->PlayerState.Get());
				const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
				const FString Who = FString::Printf(TEXT("client '%s' player %d"), *Context.ContextHandle.ToString(),
					LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : -1);

				Report(Component && Component->IsUnlocked(AchievementId), FString::Printf(TEXT("%s received replicated unlock"), *Who));

				const UAchievementLocalPlayerSubsystem* LocalSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>() : nullptr;
				const bool bSaved = LocalSubsystem && LocalSubsystem->GetLocalSnapshot().ContainsByPredicate([&AchievementId](const FAchievementSnapshotEntry& Entry)
				{
					return Entry.AchievementId == AchievementId && Entry.State >= EAchievementState::Unlocked;
				});
				Report(bSaved, FString::Printf(TEXT("%s persisted unlock to local save"), *Who));
			}
		}

		if (ClientsChecked == 0)
		{
			UE_LOG(LogAchievements, Display, TEXT("[SelfTest] No in-process clients found; run PIE with 2+ players and 'Run Under One Process' to test replication."));
		}
		UE_LOG(LogAchievements, Display, TEXT("[SelfTest] Done."));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GAchievementsDumpCmd(
	TEXT("Achievements.Dump"),
	TEXT("Lists loaded definitions and the progress visible from this world."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UAchievementSubsystem* Subsystem = AchievementConsole::GetSubsystem(World);
		if (!Subsystem)
		{
			return;
		}

		UE_LOG(LogAchievements, Display, TEXT("--- Achievements (%s, platform: %s) ---"),
			AchievementConsole::IsHost(World) ? TEXT("host") : TEXT("client"), *Subsystem->GetPlatformName().ToString());

		for (const UAchievementDefinition* Definition : Subsystem->GetAllDefinitions())
		{
			UE_LOG(LogAchievements, Display, TEXT("  def %s  api=%s  target=%d"),
				*Definition->Id.ToString(), *Definition->PlatformApiName.ToString(), Definition->TargetValue);
		}

		const AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			return;
		}
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(PlayerState);
			if (!Component)
			{
				UE_LOG(LogAchievements, Display, TEXT("  %s: no UAchievementComponent"), *GetNameSafe(PlayerState));
				continue;
			}
			for (const FAchievementProgressEntry& Entry : Component->GetAllProgress())
			{
				UE_LOG(LogAchievements, Display, TEXT("  %s: %s  %d  %s  rewards=0x%X"), *PlayerState->GetPlayerName(),
					*Entry.AchievementId.ToString(), Entry.Value, AchievementConsole::StateToString(Entry.State), Entry.ClaimedRewardMask);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GAchievementsReportCmd(
	TEXT("Achievements.Report"),
	TEXT("Achievements.Report <EventTag> [Amount] - Host only. Reports the event for every player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!AchievementConsole::IsHost(World) || Args.Num() < 1)
		{
			UE_LOG(LogAchievements, Warning, TEXT("Usage (host window): Achievements.Report <EventTag> [Amount]"));
			return;
		}

		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false);
		if (!EventTag.IsValid())
		{
			UE_LOG(LogAchievements, Warning, TEXT("Unknown gameplay tag '%s'"), *Args[0]);
			return;
		}

		const int32 Amount = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		UAchievementStatics::ReportAchievementEventForAll(World, EventTag, Amount, FGameplayTagContainer());
	}));

static FAutoConsoleCommandWithWorldAndArgs GAchievementsResetCmd(
	TEXT("Achievements.Reset"),
	TEXT("Wipes the local saves of every player in this window (and the primary player's platform achievements). Restart PIE afterwards."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		if (!GameInstance)
		{
			return;
		}
		for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (UAchievementLocalPlayerSubsystem* LocalSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>() : nullptr)
			{
				LocalSubsystem->ResetLocalData();
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GAchievementsClaimCmd(
	TEXT("Achievements.Claim"),
	TEXT("Achievements.Claim [AchievementId] - Asks the host to grant pending rewards for this window's player (all if no id)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(Controller ? Controller->PlayerState.Get() : nullptr);
		if (!Component)
		{
			UE_LOG(LogAchievements, Warning, TEXT("Achievements.Claim: no local player with UAchievementComponent"));
			return;
		}

		if (Args.Num() > 0)
		{
			Component->RequestClaimRewards(FGameplayTag::RequestGameplayTag(FName(*Args[0]), /*ErrorIfNotFound*/ false));
		}
		else
		{
			UE_LOG(LogAchievements, Display, TEXT("Claiming %d pending achievement(s)"), Component->GetClaimableAchievements().Num());
			Component->RequestClaimAllRewards();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GAchievementsSelfTestCmd(
	TEXT("Achievements.SelfTest"),
	TEXT("Achievements.SelfTest [AchievementId] - Host only. Unlocks one achievement for every player and verifies host, replication and save."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace AchievementConsole;

		if (!IsHost(World))
		{
			UE_LOG(LogAchievements, Warning, TEXT("[SelfTest] Run this from the host (listen server) window."));
			return;
		}

		const UAchievementSubsystem* Subsystem = GetSubsystem(World);
		Report(Subsystem && Subsystem->GetAllDefinitions().Num() > 0, TEXT("definitions loaded"));
		const UAchievementDefinition* Definition = Subsystem ? ResolveDefinition(*Subsystem, Args) : nullptr;
		if (!Definition || Definition->ListenedEvents.IsEmpty())
		{
			Report(false, TEXT("found a definition with at least one listened event"));
			return;
		}

		const AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			Report(false, TEXT("game state available"));
			return;
		}

		const FGameplayTag AchievementId = Definition->Id;
		UE_LOG(LogAchievements, Display, TEXT("[SelfTest] Target %s, %d player(s), %d reward(s), platform %s"), *AchievementId.ToString(),
			GameState->PlayerArray.Num(), Definition->Rewards.Num(), *Subsystem->GetPlatformName().ToString());

		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(PlayerState);
			Report(Component != nullptr, FString::Printf(TEXT("%s has UAchievementComponent"), *GetNameSafe(PlayerState)));
			if (Component && Component->IsUnlocked(AchievementId))
			{
				UE_LOG(LogAchievements, Warning, TEXT("[SelfTest] %s already has %s; run Achievements.Reset in every window and restart PIE for a clean test."),
					*GetNameSafe(PlayerState), *AchievementId.ToString());
			}
		}

		// Report enough to reach the target in one go. Conditions on the definition may still block it.
		UAchievementStatics::ReportAchievementEventForAll(World, Definition->ListenedEvents.First(), Definition->TargetValue, FGameplayTagContainer());

		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(PlayerState);
			Report(Component && Component->IsUnlocked(AchievementId), FString::Printf(TEXT("host unlocked for %s"), *GetNameSafe(PlayerState)));

			if (!Definition->Rewards.IsEmpty() && GetDefault<UAchievementSettings>()->bAutoClaimRewards)
			{
				FAchievementProgressEntry Entry;
				const bool bClaimed = Component && Component->GetProgress(AchievementId, Entry) && Entry.State == EAchievementState::RewardClaimed;
				Report(bClaimed, FString::Printf(TEXT("rewards granted for %s (check Default Granters if this fails)"), *GetNameSafe(PlayerState)));
			}
		}

		// Give replication time to reach the clients before checking them.
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, AchievementId]()
		{
			if (WeakWorld.IsValid())
			{
				VerifyClients(WeakWorld.Get(), AchievementId);
			}
		}), 2.f, false);
	}));

#endif // !UE_BUILD_SHIPPING
