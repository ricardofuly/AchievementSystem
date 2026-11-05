#include "AchievementComponent.h"
#include "AchievementCondition.h"
#include "AchievementDefinition.h"
#include "AchievementLocalPlayerSubsystem.h"
#include "AchievementReward.h"
#include "AchievementSettings.h"
#include "AchievementSubsystem.h"
#include "Algo/AllOf.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementComponent)

UAchievementComponent::UAchievementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAchievementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UAchievementComponent, Progress, COND_OwnerOnly);
}

void UAchievementComponent::BeginPlay()
{
	Super::BeginPlay();

	Progress.Owner = this;

	// On the host the controller link exists at spawn. On clients the PlayerState can begin play
	// before its owner replicates, so keep trying for a while; for other players' PlayerStates
	// (never local here) the attempts simply run out.
	if (!TryStartHandshake() && !GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(HandshakeRetryHandle, this, &ThisClass::RetryHandshake, HandshakeRetrySeconds, true);
	}
}

void UAchievementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HandshakeRetryHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UAchievementComponent::RetryHandshake()
{
	if (TryStartHandshake() || ++HandshakeAttempts >= MaxHandshakeAttempts)
	{
		GetWorld()->GetTimerManager().ClearTimer(HandshakeRetryHandle);
	}
}

bool UAchievementComponent::TryStartHandshake()
{
	if (bHandshakeSent)
	{
		return true;
	}

	UAchievementLocalPlayerSubsystem* LocalSubsystem = IsLocallyOwned() ? GetLocalPlayerSubsystem() : nullptr;
	if (!LocalSubsystem)
	{
		return false;
	}
	bHandshakeSent = true;

	// The local save is the source of truth in P2P: hand it to the host.
	const TArray<FAchievementSnapshotEntry> Snapshot = LocalSubsystem->GetLocalSnapshot();
	for (const FAchievementSnapshotEntry& Entry : Snapshot)
	{
		if (Entry.State >= EAchievementState::Unlocked)
		{
			AnnouncedUnlocks.Add(Entry.AchievementId);
		}
		if (Entry.State == EAchievementState::RewardClaimed)
		{
			AnnouncedClaims.Add(Entry.AchievementId);
		}
	}

	// Progress that replicated before the controller link existed could not be saved yet; fold it in now.
	for (const FAchievementProgressEntry& Entry : Progress.Entries)
	{
		LocalSubsystem->HandleLocalProgress(Entry);
	}

	if (GetOwner()->HasAuthority())
	{
		ApplySnapshot(Snapshot);
	}
	else
	{
		ServerSubmitSnapshot(Snapshot);
	}
	return true;
}

// ---------------------------------------------------------------------------
// Snapshot handshake
// ---------------------------------------------------------------------------

bool UAchievementComponent::ServerSubmitSnapshot_Validate(const TArray<FAchievementSnapshotEntry>& Snapshot)
{
	return Snapshot.Num() <= GetDefault<UAchievementSettings>()->MaxSnapshotEntries;
}

void UAchievementComponent::ServerSubmitSnapshot_Implementation(const TArray<FAchievementSnapshotEntry>& Snapshot)
{
	ApplySnapshot(Snapshot);
}

void UAchievementComponent::ApplySnapshot(const TArray<FAchievementSnapshotEntry>& Snapshot)
{
	check(GetOwner()->HasAuthority());

	if (bSnapshotApplied)
	{
		UE_LOG(LogAchievements, Warning, TEXT("Ignoring duplicate snapshot for %s"), *GetNameSafe(GetOwner()));
		return;
	}
	bSnapshotApplied = true;

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	if (!Subsystem)
	{
		return;
	}

	for (const FAchievementSnapshotEntry& Saved : Snapshot)
	{
		const UAchievementDefinition* Definition = Subsystem->FindDefinition(Saved.AchievementId);
		if (!Definition)
		{
			UE_LOG(LogAchievements, Verbose, TEXT("Dropping unknown achievement %s from snapshot"), *Saved.AchievementId.ToString());
			continue;
		}

		// Merge, never overwrite: events may already have counted on the host before the snapshot arrived.
		FAchievementProgressEntry& Entry = Progress.FindOrAdd(Saved.AchievementId);
		if (Entry.MergeFrom(Saved, Definition->TargetValue))
		{
			Progress.MarkItemDirty(Entry);
		}

		// Unlocked elsewhere (previous session, Steam reconciliation) with rewards still pending.
		if (Entry.State == EAchievementState::Unlocked && GetDefault<UAchievementSettings>()->bAutoClaimRewards)
		{
			ClaimRewards(Entry, *Definition);
		}
	}
}

// ---------------------------------------------------------------------------
// Progress evaluation (host)
// ---------------------------------------------------------------------------

void UAchievementComponent::ReportEvent(const FAchievementEventContext& Context)
{
	if (!ensureMsgf(GetOwner()->HasAuthority(), TEXT("ReportEvent must be called on the host")))
	{
		return;
	}

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	if (!Subsystem || Context.Amount <= 0)
	{
		return;
	}

	for (const UAchievementDefinition* Definition : Subsystem->GetDefinitionsForEvent(Context.EventTag))
	{
		FAchievementProgressEntry& Entry = Progress.FindOrAdd(Definition->Id);
		if (Entry.IsUnlocked()
			|| !ArePrerequisitesMet(*Definition)
			|| !AreConditionsMet(*Definition, Context))
		{
			continue;
		}

		Entry.Value = FMath::Min(Entry.Value + Context.Amount, Definition->TargetValue);
		Entry.State = EAchievementState::InProgress;

		if (Entry.Value >= Definition->TargetValue)
		{
			Unlock(Entry, *Definition);
		}
		else
		{
			CommitEntry(Entry);
		}
	}
}

bool UAchievementComponent::ArePrerequisitesMet(const UAchievementDefinition& Definition) const
{
	for (const FGameplayTag& Prerequisite : Definition.Prerequisites)
	{
		if (!IsUnlocked(Prerequisite))
		{
			return false;
		}
	}
	return true;
}

bool UAchievementComponent::AreConditionsMet(const UAchievementDefinition& Definition, const FAchievementEventContext& Context) const
{
	return Algo::AllOf(Definition.Conditions, [&Context](const UAchievementCondition* Condition)
	{
		return !Condition || Condition->IsSatisfied(Context);
	});
}

void UAchievementComponent::Unlock(FAchievementProgressEntry& Entry, const UAchievementDefinition& Definition)
{
	Entry.State = EAchievementState::Unlocked;
	Entry.UnlockedAtUtcTicks = FDateTime::UtcNow().GetTicks();

	UE_LOG(LogAchievements, Log, TEXT("%s unlocked %s"), *GetNameSafe(GetOwner()), *Definition.Id.ToString());

	if (GetDefault<UAchievementSettings>()->bAutoClaimRewards)
	{
		ClaimRewards(Entry, Definition);
	}
	else
	{
		CommitEntry(Entry);
	}
}

// ---------------------------------------------------------------------------
// Rewards (host)
// ---------------------------------------------------------------------------

void UAchievementComponent::RequestClaimRewards(FGameplayTag AchievementId)
{
	ServerClaimRewards(AchievementId);
}

void UAchievementComponent::RequestClaimAllRewards()
{
	ServerClaimAllRewards();
}

void UAchievementComponent::ServerClaimAllRewards_Implementation()
{
	for (const FGameplayTag& AchievementId : GetClaimableAchievements())
	{
		ServerClaimRewards_Implementation(AchievementId);
	}
}

void UAchievementComponent::RetryPendingRewards()
{
	if (!ensureMsgf(GetOwner()->HasAuthority(), TEXT("RetryPendingRewards must be called on the host")))
	{
		return;
	}

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	if (!Subsystem)
	{
		return;
	}

	for (FAchievementProgressEntry& Entry : Progress.Entries)
	{
		if (Entry.State != EAchievementState::Unlocked)
		{
			continue;
		}
		if (const UAchievementDefinition* Definition = Subsystem->FindDefinition(Entry.AchievementId))
		{
			ClaimRewards(Entry, *Definition, /*bRetryFailed*/ false);
		}
	}
}

TArray<FGameplayTag> UAchievementComponent::GetClaimableAchievements() const
{
	TArray<FGameplayTag> Result;
	for (const FAchievementProgressEntry& Entry : Progress.Entries)
	{
		if (Entry.State == EAchievementState::Unlocked)
		{
			Result.Add(Entry.AchievementId);
		}
	}
	return Result;
}

void UAchievementComponent::ServerClaimRewards_Implementation(FGameplayTag AchievementId)
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	const UAchievementDefinition* Definition = Subsystem ? Subsystem->FindDefinition(AchievementId) : nullptr;
	FAchievementProgressEntry* Entry = Progress.Find(AchievementId);

	if (Definition && Entry)
	{
		ClaimRewards(*Entry, *Definition);
	}
}

void UAchievementComponent::ClaimRewards(FAchievementProgressEntry& Entry, const UAchievementDefinition& Definition, bool bRetryFailed)
{
	// Claimed state is written before reporting, so a reward is never granted twice.
	if (Entry.State != EAchievementState::Unlocked)
	{
		return;
	}

	UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	APlayerState* PlayerState = GetOwningPlayerState();
	bool bAllGranted = true;

	// Each reward has its own bit, so a partial grant never repeats on retry.
	for (int32 Index = 0; Index < Definition.Rewards.Num(); ++Index)
	{
		const uint32 Bit = 1u << Index;
		const UAchievementReward* Reward = Definition.Rewards[Index];
		if (!Reward || (Entry.ClaimedRewardMask & Bit))
		{
			continue;
		}
		if (!bRetryFailed && (Entry.FailedRewardMask & Bit))
		{
			bAllGranted = false;
			continue;
		}

		switch (Subsystem->GrantReward(Reward, PlayerState))
		{
		case EAchievementGrantResult::Granted:
			Entry.ClaimedRewardMask |= Bit;
			Entry.FailedRewardMask &= ~Bit;
			break;
		case EAchievementGrantResult::Deferred:
			bAllGranted = false;
			break;
		case EAchievementGrantResult::Failed:
			Entry.FailedRewardMask |= Bit;
			bAllGranted = false;
			UE_LOG(LogAchievements, Error, TEXT("Reward %d of %s failed; it will only be retried by a manual claim or next session"),
				Index, *Definition.Id.ToString());
			break;
		}
	}

	if (bAllGranted)
	{
		Entry.State = EAchievementState::RewardClaimed;
	}
	else
	{
		UE_LOG(LogAchievements, Warning, TEXT("Rewards for %s were not fully granted; they stay claimable"), *Definition.Id.ToString());
	}

	CommitEntry(Entry);
}

// ---------------------------------------------------------------------------
// Local side (owning client, or host for its own player)
// ---------------------------------------------------------------------------

void UAchievementComponent::CommitEntry(FAchievementProgressEntry& Entry)
{
	Progress.MarkItemDirty(Entry);

	// The host's own player never receives replication callbacks.
	if (IsLocallyOwned())
	{
		HandleEntryChanged(Entry);
	}
}

void UAchievementComponent::HandleEntryChanged(const FAchievementProgressEntry& Entry)
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	UAchievementLocalPlayerSubsystem* LocalSubsystem = GetLocalPlayerSubsystem();
	if (!Subsystem)
	{
		return;
	}

	if (LocalSubsystem)
	{
		LocalSubsystem->HandleLocalProgress(Entry);
	}
	OnProgressChanged.Broadcast(Entry);

	const UAchievementDefinition* Definition = Subsystem->FindDefinition(Entry.AchievementId);

	if (Entry.IsUnlocked() && !AnnouncedUnlocks.Contains(Entry.AchievementId))
	{
		AnnouncedUnlocks.Add(Entry.AchievementId);
		OnAchievementUnlocked.Broadcast(Definition);
	}

	if (Entry.State == EAchievementState::Unlocked && Definition && !Definition->Rewards.IsEmpty())
	{
		OnRewardsClaimable.Broadcast(Definition);
	}
	else if (Entry.State == EAchievementState::RewardClaimed && !AnnouncedClaims.Contains(Entry.AchievementId))
	{
		AnnouncedClaims.Add(Entry.AchievementId);
		OnRewardsClaimed.Broadcast(Definition);
	}
}

// ---------------------------------------------------------------------------
// Queries & helpers
// ---------------------------------------------------------------------------

bool UAchievementComponent::GetProgress(FGameplayTag AchievementId, FAchievementProgressEntry& OutEntry) const
{
	if (const FAchievementProgressEntry* Entry = Progress.Find(AchievementId))
	{
		OutEntry = *Entry;
		return true;
	}
	return false;
}

bool UAchievementComponent::IsUnlocked(FGameplayTag AchievementId) const
{
	const FAchievementProgressEntry* Entry = Progress.Find(AchievementId);
	return Entry && Entry->IsUnlocked();
}

APlayerState* UAchievementComponent::GetOwningPlayerState() const
{
	return GetOwner<APlayerState>();
}

bool UAchievementComponent::IsLocallyOwned() const
{
	const APlayerState* PlayerState = GetOwningPlayerState();
	const APlayerController* Controller = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	return Controller && Controller->IsLocalController();
}

UAchievementLocalPlayerSubsystem* UAchievementComponent::GetLocalPlayerSubsystem() const
{
	// Each split-screen player has its own PlayerState, controller and local player (and so its own save).
	const APlayerState* PlayerState = GetOwningPlayerState();
	const APlayerController* Controller = PlayerState ? PlayerState->GetPlayerController() : nullptr;
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>() : nullptr;
}

UAchievementSubsystem* UAchievementComponent::GetAchievementSubsystem() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr;
}
