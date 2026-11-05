#include "AchievementLocalPlayerSubsystem.h"
#include "AchievementDefinition.h"
#include "AchievementPlatform.h"
#include "AchievementSettings.h"
#include "AchievementSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementLocalPlayerSubsystem)

void UAchievementLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Definitions and the platform live in UAchievementSubsystem, which the game instance
	// initializes before any local player exists.
	Super::Initialize(Collection);

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	Storage = MakeUnique<FLocalSaveGameStorage>(MakeSlotName(), LocalPlayer ? LocalPlayer->GetPlatformUserIndex() : 0);
	Storage->Load(SaveData);

	if (IsPrimaryPlayer())
	{
		QueueSavedUnlocksForPlatform();
	}
	else
	{
		// Guests never reach the platform; drop anything queued by an older version of the save.
		SaveData.PendingUnlocks.Reset();
		SaveData.PendingStats.Reset();
	}

	CommitTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickCommit),
		GetDefault<UAchievementSettings>()->CommitIntervalSeconds);
}

void UAchievementLocalPlayerSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(CommitTickerHandle);

	if (UAchievementComponent* Component = BoundComponent.Get())
	{
		UnbindAchievementComponent(*Component);
	}

	FlushPlatform();
	if (Storage)
	{
		Storage->SaveBlocking(SaveData);
	}
	Storage.Reset();

	Super::Deinitialize();
}

FString UAchievementLocalPlayerSubsystem::MakeSlotName() const
{
	FString SlotName = GetDefault<UAchievementSettings>()->SaveSlotName;

	// The first local player keeps the plain slot name, so saves from before split-screen still load.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const int32 LocalIndex = LocalPlayer ? LocalPlayer->GetLocalPlayerIndex() : 0;
	if (LocalIndex > 0)
	{
		SlotName += FString::Printf(TEXT("_P%d"), LocalIndex);
	}

#if WITH_EDITOR
	// Single-process PIE runs every player on one machine; give each instance its own save.
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (const FWorldContext* WorldContext = GameInstance ? GameInstance->GetWorldContext() : nullptr; WorldContext && WorldContext->WorldType == EWorldType::PIE)
	{
		SlotName += FString::Printf(TEXT("_PIE%d"), WorldContext->PIEInstance);
	}
#endif

	return SlotName;
}

bool UAchievementLocalPlayerSubsystem::IsPrimaryPlayer() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance && GameInstance->GetFirstGamePlayer() == LocalPlayer;
}

UAchievementSubsystem* UAchievementLocalPlayerSubsystem::GetAchievementSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr;
}

FAchievementSnapshotEntry& UAchievementLocalPlayerSubsystem::FindOrAddSaved(const FGameplayTag& AchievementId)
{
	if (FAchievementSnapshotEntry* Saved = SaveData.Entries.FindByPredicate(
		[&AchievementId](const FAchievementSnapshotEntry& Candidate) { return Candidate.AchievementId == AchievementId; }))
	{
		return *Saved;
	}

	FAchievementSnapshotEntry& NewEntry = SaveData.Entries.AddDefaulted_GetRef();
	NewEntry.AchievementId = AchievementId;
	return NewEntry;
}

// ---------------------------------------------------------------------------
// Component binding & event forwarding
// ---------------------------------------------------------------------------

void UAchievementLocalPlayerSubsystem::BindAchievementComponent(UAchievementComponent& Component)
{
	if (BoundComponent.Get() == &Component)
	{
		return;
	}
	if (UAchievementComponent* Previous = BoundComponent.Get())
	{
		UnbindAchievementComponent(*Previous);
	}

	BoundComponent = &Component;
	Component.OnProgressChanged.AddDynamic(this, &ThisClass::ForwardProgressChanged);
	Component.OnAchievementUnlocked.AddDynamic(this, &ThisClass::ForwardUnlocked);
	Component.OnRewardsClaimable.AddDynamic(this, &ThisClass::ForwardRewardsClaimable);
	Component.OnRewardsClaimed.AddDynamic(this, &ThisClass::ForwardRewardsClaimed);

	OnAchievementsReady.Broadcast();
}

void UAchievementLocalPlayerSubsystem::UnbindAchievementComponent(UAchievementComponent& Component)
{
	Component.OnProgressChanged.RemoveDynamic(this, &ThisClass::ForwardProgressChanged);
	Component.OnAchievementUnlocked.RemoveDynamic(this, &ThisClass::ForwardUnlocked);
	Component.OnRewardsClaimable.RemoveDynamic(this, &ThisClass::ForwardRewardsClaimable);
	Component.OnRewardsClaimed.RemoveDynamic(this, &ThisClass::ForwardRewardsClaimed);

	if (BoundComponent.Get() == &Component)
	{
		BoundComponent.Reset();
	}
}

void UAchievementLocalPlayerSubsystem::ForwardProgressChanged(const FAchievementProgressEntry& Entry)
{
	OnProgressChanged.Broadcast(Entry);
}

void UAchievementLocalPlayerSubsystem::ForwardUnlocked(const UAchievementDefinition* Definition)
{
	OnAchievementUnlocked.Broadcast(Definition);
}

void UAchievementLocalPlayerSubsystem::ForwardRewardsClaimable(const UAchievementDefinition* Definition)
{
	OnRewardsClaimable.Broadcast(Definition);
}

void UAchievementLocalPlayerSubsystem::ForwardRewardsClaimed(const UAchievementDefinition* Definition)
{
	OnRewardsClaimed.Broadcast(Definition);
}

// ---------------------------------------------------------------------------
// Queries & commands
// ---------------------------------------------------------------------------

namespace AchievementQueries
{
	static int32 StatusRank(const FAchievementViewData& View)
	{
		if (View.bClaimable)                             return 0;
		if (View.IsUnlocked())                           return 1;
		if (View.State == EAchievementState::InProgress) return 2;
		return 3;
	}

	static bool PassesFilter(const FAchievementViewData& View, EAchievementListFilter Filter)
	{
		switch (Filter)
		{
		case EAchievementListFilter::Unlocked:  return View.IsUnlocked();
		case EAchievementListFilter::Locked:    return !View.IsUnlocked();
		case EAchievementListFilter::Claimable: return View.bClaimable;
		default:                                return true;
		}
	}
}

bool UAchievementLocalPlayerSubsystem::FindProgress(const FGameplayTag& AchievementId, FAchievementProgressEntry& OutEntry) const
{
	if (const UAchievementComponent* Component = BoundComponent.Get())
	{
		return Component->GetProgress(AchievementId, OutEntry);
	}

	const FAchievementSnapshotEntry* Saved = SaveData.Entries.FindByPredicate(
		[&AchievementId](const FAchievementSnapshotEntry& Candidate) { return Candidate.AchievementId == AchievementId; });
	if (!Saved)
	{
		return false;
	}

	OutEntry.AchievementId = Saved->AchievementId;
	OutEntry.Value = Saved->Value;
	OutEntry.State = Saved->State;
	OutEntry.UnlockedAtUtcTicks = Saved->UnlockedAtUtcTicks;
	OutEntry.ClaimedRewardMask = Saved->ClaimedRewardMask;
	return true;
}

TArray<FAchievementViewData> UAchievementLocalPlayerSubsystem::GetAchievements(EAchievementListFilter Filter, EAchievementListSort Sort, bool bIncludeHiddenLocked) const
{
	TArray<FAchievementViewData> Views;
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	if (!Subsystem)
	{
		return Views;
	}

	for (const UAchievementDefinition* Definition : Subsystem->GetAllDefinitions())
	{
		FAchievementProgressEntry Entry;
		const bool bHasEntry = FindProgress(Definition->Id, Entry);
		FAchievementViewData View = FAchievementViewData::Make(*Definition, bHasEntry ? &Entry : nullptr);

		if ((View.bMasked && !bIncludeHiddenLocked) || !AchievementQueries::PassesFilter(View, Filter))
		{
			continue;
		}
		Views.Add(MoveTemp(View));
	}

	if (Sort == EAchievementListSort::Status)
	{
		Views.StableSort([](const FAchievementViewData& A, const FAchievementViewData& B)
		{
			const int32 RankA = AchievementQueries::StatusRank(A);
			const int32 RankB = AchievementQueries::StatusRank(B);
			return RankA != RankB ? RankA < RankB : A.Progress > B.Progress;
		});
	}
	return Views;
}

bool UAchievementLocalPlayerSubsystem::GetAchievement(FGameplayTag AchievementId, FAchievementViewData& OutView) const
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	const UAchievementDefinition* Definition = Subsystem ? Subsystem->FindDefinition(AchievementId) : nullptr;
	if (!Definition)
	{
		return false;
	}

	FAchievementProgressEntry Entry;
	const bool bHasEntry = FindProgress(AchievementId, Entry);
	OutView = FAchievementViewData::Make(*Definition, bHasEntry ? &Entry : nullptr);
	return true;
}

int32 UAchievementLocalPlayerSubsystem::GetUnlockedCount() const
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	if (!Subsystem)
	{
		return 0;
	}

	int32 Count = 0;
	for (const UAchievementDefinition* Definition : Subsystem->GetAllDefinitions())
	{
		FAchievementProgressEntry Entry;
		if (FindProgress(Definition->Id, Entry) && Entry.IsUnlocked())
		{
			++Count;
		}
	}
	return Count;
}

int32 UAchievementLocalPlayerSubsystem::GetTotalCount() const
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	return Subsystem ? Subsystem->GetAllDefinitions().Num() : 0;
}

void UAchievementLocalPlayerSubsystem::ClaimRewards(FGameplayTag AchievementId)
{
	if (UAchievementComponent* Component = BoundComponent.Get())
	{
		Component->RequestClaimRewards(AchievementId);
	}
	else
	{
		UE_LOG(LogAchievements, Warning, TEXT("ClaimRewards(%s) ignored: rewards can only be claimed during a match"), *AchievementId.ToString());
	}
}

void UAchievementLocalPlayerSubsystem::ClaimAllRewards()
{
	if (UAchievementComponent* Component = BoundComponent.Get())
	{
		Component->RequestClaimAllRewards();
	}
	else
	{
		UE_LOG(LogAchievements, Warning, TEXT("ClaimAllRewards ignored: rewards can only be claimed during a match"));
	}
}

// ---------------------------------------------------------------------------
// Local progress
// ---------------------------------------------------------------------------

void UAchievementLocalPlayerSubsystem::HandleLocalProgress(const FAchievementProgressEntry& Entry)
{
	FAchievementSnapshotEntry& Saved = FindOrAddSaved(Entry.AchievementId);
	const bool bWasUnlocked = Saved.State >= EAchievementState::Unlocked;

	// The host may briefly hold lower values (handshake not merged yet); the save only ever moves forward.
	if (!Saved.MergeFrom(Entry))
	{
		return;
	}
	bSaveDirty = true;

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	const UAchievementDefinition* Definition = Subsystem ? Subsystem->FindDefinition(Entry.AchievementId) : nullptr;
	if (!Definition || !IsPrimaryPlayer())
	{
		return;
	}

	if (!Definition->PlatformStatName.IsNone())
	{
		SaveData.PendingStats.Add(Definition->PlatformStatName, Saved.Value);
		bPlatformDirty = true;
	}

	if (!Definition->PlatformApiName.IsNone())
	{
		IAchievementPlatform* Platform = Subsystem->GetPlatform();
		if (Saved.State >= EAchievementState::Unlocked)
		{
			SaveData.PendingUnlocks.Add(Definition->PlatformApiName);
			bPlatformDirty = true;
		}
		else if (Platform && Platform->IsReady() && Definition->TargetValue > 1)
		{
			Platform->IndicateProgress(Definition->PlatformApiName, Saved.Value, Definition->TargetValue);
		}
	}

	// Unlocks are pushed right away so the Steam overlay pops in sync with the game.
	if (!bWasUnlocked && Saved.State >= EAchievementState::Unlocked)
	{
		FlushPlatform();
	}
}

// ---------------------------------------------------------------------------
// Platform sync (primary player only)
// ---------------------------------------------------------------------------

void UAchievementLocalPlayerSubsystem::QueueSavedUnlocksForPlatform()
{
	// Anything unlocked locally is re-sent: unlocks are idempotent and this heals lost syncs.
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	for (const FAchievementSnapshotEntry& Entry : SaveData.Entries)
	{
		const UAchievementDefinition* Definition = Subsystem ? Subsystem->FindDefinition(Entry.AchievementId) : nullptr;
		if (Definition && Entry.State >= EAchievementState::Unlocked && !Definition->PlatformApiName.IsNone())
		{
			SaveData.PendingUnlocks.Add(Definition->PlatformApiName);
		}
	}
	bPlatformDirty = !SaveData.PendingUnlocks.IsEmpty() || !SaveData.PendingStats.IsEmpty();
}

void UAchievementLocalPlayerSubsystem::FlushPlatform()
{
	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	IAchievementPlatform* Platform = Subsystem ? Subsystem->GetPlatform() : nullptr;
	if (!Platform || !Platform->IsReady() || !IsPrimaryPlayer())
	{
		return;
	}

	ReconcileWithPlatform();

	if (!bPlatformDirty)
	{
		return;
	}

	for (auto It = SaveData.PendingStats.CreateIterator(); It; ++It)
	{
		if (Platform->SetStat(It.Key(), It.Value()))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = SaveData.PendingUnlocks.CreateIterator(); It; ++It)
	{
		if (Platform->UnlockAchievement(*It))
		{
			It.RemoveCurrent();
		}
	}

	if (Platform->Commit())
	{
		bPlatformDirty = !SaveData.PendingUnlocks.IsEmpty() || !SaveData.PendingStats.IsEmpty();
	}

	bSaveDirty = true;
}

void UAchievementLocalPlayerSubsystem::ReconcileWithPlatform()
{
	if (bReconciled)
	{
		return;
	}
	bReconciled = true;

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	IAchievementPlatform* Platform = Subsystem->GetPlatform();

	// Platform says unlocked but the local save does not (reinstall, other machine): trust the platform.
	// The healed save is sent to the host on the next session handshake.
	for (const UAchievementDefinition* Definition : Subsystem->GetAllDefinitions())
	{
		bool bUnlockedOnPlatform = false;
		if (Definition->PlatformApiName.IsNone()
			|| !Platform->IsAchievementUnlocked(Definition->PlatformApiName, bUnlockedOnPlatform)
			|| !bUnlockedOnPlatform)
		{
			continue;
		}

		FAchievementSnapshotEntry& Saved = FindOrAddSaved(Definition->Id);
		if (Saved.State < EAchievementState::Unlocked)
		{
			Saved.State = EAchievementState::Unlocked;
			Saved.Value = Definition->TargetValue;
			Saved.UnlockedAtUtcTicks = FDateTime::UtcNow().GetTicks();
			bSaveDirty = true;
		}
	}
}

bool UAchievementLocalPlayerSubsystem::TickCommit(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_AchievementLocalPlayerSubsystem_TickCommit);

	FlushPlatform();

	// Debounced: written on this tick, or blocking on shutdown.
	if (bSaveDirty && Storage)
	{
		Storage->SaveAsync(SaveData);
		bSaveDirty = false;
	}
	return true;
}

#if !UE_BUILD_SHIPPING
void UAchievementLocalPlayerSubsystem::ResetLocalData()
{
	SaveData = FAchievementSaveData();
	bSaveDirty = false;
	bPlatformDirty = false;

	if (Storage)
	{
		Storage->SaveBlocking(SaveData);
		Storage->ClearBackup();
	}

	const UAchievementSubsystem* Subsystem = GetAchievementSubsystem();
	IAchievementPlatform* Platform = Subsystem ? Subsystem->GetPlatform() : nullptr;
	if (Platform && IsPrimaryPlayer())
	{
		Platform->ResetAll();
	}

	UE_LOG(LogAchievements, Warning, TEXT("Local achievement data reset for %s (%s)"),
		*MakeSlotName(), IsPrimaryPlayer() ? TEXT("primary, platform reset too") : TEXT("guest"));
}
#endif
