#include "AchievementListWidgets.h"
#include "AchievementLocalPlayerSubsystem.h"
#include "Components/PanelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementListWidgets)

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

	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (UAchievementLocalPlayerSubsystem* Achievements = LocalPlayer ? LocalPlayer->GetSubsystem<UAchievementLocalPlayerSubsystem>() : nullptr)
	{
		Achievements->ClaimRewards(Data.Id);
	}
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

void UAchievementListWidget::BindToAchievements(UAchievementLocalPlayerSubsystem& InAchievements)
{
	InAchievements.OnProgressChanged.AddDynamic(this, &ThisClass::HandleProgressChanged);

	// Outside a match the list shows the local save, so it works in menus too.
	Rebuild();
}

void UAchievementListWidget::UnbindFromAchievements(UAchievementLocalPlayerSubsystem& InAchievements)
{
	InAchievements.OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleProgressChanged);
}

void UAchievementListWidget::HandleAchievementsReady()
{
	Rebuild();
	Super::HandleAchievementsReady();
}

void UAchievementListWidget::Rebuild()
{
	const UAchievementLocalPlayerSubsystem* Achievements = GetAchievements();
	if (!Achievements || !EntryContainer || !EntryClass)
	{
		UE_CLOG(!EntryClass, LogAchievements, Warning, TEXT("%s: EntryClass is not set"), *GetName());
		return;
	}

	EntryContainer->ClearChildren();
	EntriesById.Reset();

	for (const FAchievementViewData& View : Achievements->GetAchievements(EAchievementListFilter::All, EAchievementListSort::Status, bShowHiddenLocked))
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
	const UAchievementLocalPlayerSubsystem* Achievements = GetAchievements();
	const TObjectPtr<UAchievementEntryWidget>* EntryWidget = EntriesById.Find(Entry.AchievementId);
	FAchievementViewData View;

	if (!Achievements || !EntryWidget || !Achievements->GetAchievement(Entry.AchievementId, View))
	{
		// Entry was filtered out (hidden) or the list is not built yet: rebuild keeps it consistent.
		Rebuild();
		return;
	}

	// Updated in place to avoid reordering under the player's cursor; order refreshes on the next Rebuild.
	(*EntryWidget)->SetData(View);
	RecountUnlocked();
	OnListUpdated();
}

void UAchievementListWidget::ClaimAll()
{
	if (UAchievementLocalPlayerSubsystem* Achievements = GetAchievements())
	{
		Achievements->ClaimAllRewards();
	}
}

void UAchievementListWidget::RecountUnlocked()
{
	UnlockedCount = 0;
	for (const TPair<FGameplayTag, TObjectPtr<UAchievementEntryWidget>>& Pair : EntriesById)
	{
		if (Pair.Value->GetData().IsUnlocked())
		{
			++UnlockedCount;
		}
	}
}
