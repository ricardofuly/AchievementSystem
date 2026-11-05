#include "AchievementListWidgets.h"
#include "AchievementComponent.h"
#include "AchievementDefinition.h"
#include "AchievementReward.h"
#include "AchievementStatics.h"
#include "AchievementSubsystem.h"
#include "Components/PanelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementListWidgets)

#define LOCTEXT_NAMESPACE "AchievementsUI"

// ---------------------------------------------------------------------------
// View data
// ---------------------------------------------------------------------------

FAchievementViewData FAchievementViewData::Make(const UAchievementDefinition& Definition, const FAchievementProgressEntry* Entry)
{
	FAchievementViewData View;
	View.Id = Definition.Id;
	View.State = Entry ? Entry->State : EAchievementState::Locked;
	View.Value = Entry ? Entry->Value : 0;
	View.TargetValue = Definition.TargetValue;
	View.Progress = FMath::Clamp(static_cast<float>(View.Value) / FMath::Max(1, View.TargetValue), 0.f, 1.f);
	View.bHasRewards = !Definition.Rewards.IsEmpty();

	if (!(Definition.bHidden && View.State < EAchievementState::Unlocked))
	{
		const uint32 ClaimedMask = Entry ? Entry->ClaimedRewardMask : 0;
		for (int32 Index = 0; Index < Definition.Rewards.Num(); ++Index)
		{
			const UAchievementReward* Reward = Definition.Rewards[Index];
			if (!Reward)
			{
				continue;
			}

			FAchievementRewardViewData& RewardView = View.Rewards.AddDefaulted_GetRef();
			RewardView.DisplayName = Reward->DisplayName;
			RewardView.Icon = Reward->Icon;
			RewardView.bGranted = (ClaimedMask & (1u << Index)) != 0;
		}
	}
	View.bClaimable = View.bHasRewards && View.State == EAchievementState::Unlocked;
	View.bMasked = Definition.bHidden && View.State < EAchievementState::Unlocked;

	if (View.bMasked)
	{
		View.DisplayName = LOCTEXT("HiddenName", "Hidden achievement");
		View.Description = LOCTEXT("HiddenDescription", "Keep playing to discover it.");
	}
	else
	{
		View.DisplayName = Definition.DisplayName;
		View.Description = Definition.Description;
		View.Icon = Definition.Icon;
	}
	return View;
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

void UAchievementEntryWidget::SetData(const FAchievementViewData& InData)
{
	const bool bIconChanged = Data.Icon != InData.Icon || Data.Id != InData.Id;
	const bool bRewardsChanged = Data.Id != InData.Id || Data.Rewards.Num() != InData.Rewards.Num();
	Data = InData;
	OnDataChanged(Data);

	if (bIconChanged)
	{
		LoadIcon();
	}
	if (bRewardsChanged)
	{
		LoadRewardIcons();
	}
}

void UAchievementEntryWidget::LoadRewardIcons()
{
	TArray<FSoftObjectPath> Paths;
	for (const FAchievementRewardViewData& Reward : Data.Rewards)
	{
		if (!Reward.Icon.IsNull())
		{
			Paths.AddUnique(Reward.Icon.ToSoftObjectPath());
		}
	}

	const FGameplayTag RequestedId = Data.Id;
	auto Broadcast = [this, RequestedId]()
	{
		// Ignore stale loads if the entry was reused for another achievement meanwhile.
		if (Data.Id != RequestedId)
		{
			return;
		}
		for (int32 Index = 0; Index < Data.Rewards.Num(); ++Index)
		{
			OnRewardIconLoaded(Index, Data.Rewards[Index].Icon.Get());
		}
	};

	if (Paths.IsEmpty())
	{
		Broadcast();
		return;
	}
	UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateWeakLambda(this, Broadcast));
}

void UAchievementEntryWidget::LoadIcon()
{
	if (Data.Icon.IsNull())
	{
		OnIconLoaded(nullptr);
		return;
	}

	const FSoftObjectPath IconPath = Data.Icon.ToSoftObjectPath();
	UAssetManager::GetStreamableManager().RequestAsyncLoad(IconPath, FStreamableDelegate::CreateWeakLambda(this, [this, IconPath]()
	{
		// Ignore stale loads if the entry was reused for another achievement meanwhile.
		if (Data.Icon.ToSoftObjectPath() == IconPath)
		{
			OnIconLoaded(Data.Icon.Get());
		}
	}));
}

void UAchievementEntryWidget::Claim()
{
	if (!Data.bClaimable)
	{
		return;
	}

	const APlayerController* Controller = GetOwningPlayer();
	if (UAchievementComponent* Component = UAchievementStatics::GetAchievementComponent(Controller ? Controller->PlayerState.Get() : nullptr))
	{
		Component->RequestClaimRewards(Data.Id);
	}
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

namespace AchievementListSorting
{
	static int32 Rank(const FAchievementViewData& View)
	{
		if (View.bClaimable)                         return 0;
		if (View.State >= EAchievementState::Unlocked) return 1;
		if (View.State == EAchievementState::InProgress) return 2;
		return 3;
	}
}

void UAchievementListWidget::BindToComponent(UAchievementComponent& Component)
{
	Component.OnProgressChanged.AddDynamic(this, &ThisClass::HandleProgressChanged);
	Rebuild();
}

void UAchievementListWidget::UnbindFromComponent(UAchievementComponent& Component)
{
	Component.OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleProgressChanged);
}

void UAchievementListWidget::Rebuild()
{
	const UAchievementComponent* Component = GetAchievementComponent();
	const UGameInstance* GameInstance = GetGameInstance();
	const UAchievementSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr;
	if (!Component || !Subsystem || !EntryContainer || !EntryClass)
	{
		UE_CLOG(!EntryClass, LogAchievements, Warning, TEXT("%s: EntryClass is not set"), *GetName());
		return;
	}

	TArray<FAchievementViewData> Views;
	for (const UAchievementDefinition* Definition : Subsystem->GetAllDefinitions())
	{
		FAchievementProgressEntry Entry;
		const bool bHasEntry = Component->GetProgress(Definition->Id, Entry);
		FAchievementViewData View = FAchievementViewData::Make(*Definition, bHasEntry ? &Entry : nullptr);
		if (View.bMasked && !bShowHiddenLocked)
		{
			continue;
		}
		Views.Add(MoveTemp(View));
	}

	Views.StableSort([](const FAchievementViewData& A, const FAchievementViewData& B)
	{
		const int32 RankA = AchievementListSorting::Rank(A);
		const int32 RankB = AchievementListSorting::Rank(B);
		return RankA != RankB ? RankA < RankB : A.Progress > B.Progress;
	});

	EntryContainer->ClearChildren();
	EntriesById.Reset();

	for (const FAchievementViewData& View : Views)
	{
		UAchievementEntryWidget* EntryWidget = CreateWidget<UAchievementEntryWidget>(this, EntryClass);
		EntryWidget->SetData(View);
		EntryContainer->AddChild(EntryWidget);
		EntriesById.Add(View.Id, EntryWidget);
	}

	RecountUnlocked();
	OnListUpdated();
}

void UAchievementListWidget::HandleProgressChanged(const FAchievementProgressEntry& Entry)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UAchievementSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr;
	const UAchievementDefinition* Definition = Subsystem ? Subsystem->FindDefinition(Entry.AchievementId) : nullptr;
	const TObjectPtr<UAchievementEntryWidget>* EntryWidget = EntriesById.Find(Entry.AchievementId);

	if (!Definition || !EntryWidget)
	{
		// Entry was filtered out (hidden) or the list is not built yet: rebuild keeps it consistent.
		Rebuild();
		return;
	}

	// Updated in place to avoid reordering under the player's cursor; order refreshes on the next Rebuild.
	(*EntryWidget)->SetData(FAchievementViewData::Make(*Definition, &Entry));
	RecountUnlocked();
	OnListUpdated();
}

void UAchievementListWidget::ClaimAll()
{
	if (UAchievementComponent* Component = GetAchievementComponent())
	{
		Component->RequestClaimAllRewards();
	}
}

void UAchievementListWidget::RecountUnlocked()
{
	UnlockedCount = 0;
	for (const TPair<FGameplayTag, TObjectPtr<UAchievementEntryWidget>>& Pair : EntriesById)
	{
		if (Pair.Value->GetData().State >= EAchievementState::Unlocked)
		{
			++UnlockedCount;
		}
	}
}

#undef LOCTEXT_NAMESPACE
