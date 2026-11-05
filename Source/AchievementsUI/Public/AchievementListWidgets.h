// Achievement list screen: a list widget that fills a panel with entry widgets.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "AchievementWidgetBase.h"
#include "AchievementListWidgets.generated.h"

class UAchievementDefinition;
class UPanelWidget;
class UTexture2D;

/** Display-ready data for one reward of an achievement. */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSUI_API FAchievementRewardViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TSoftObjectPtr<UTexture2D> Icon;

	/** This specific reward was already granted (partial grants are possible). */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bGranted = false;
};

/** Display-ready data for one achievement; hidden achievements are already masked. */
USTRUCT(BlueprintType)
struct ACHIEVEMENTSUI_API FAchievementViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FGameplayTag Id;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	EAchievementState State = EAchievementState::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	int32 TargetValue = 1;

	/** 0..1, ready for a progress bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	float Progress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bHasRewards = false;

	/** Same order as the definition's Rewards. Empty while the achievement is masked. */
	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	TArray<FAchievementRewardViewData> Rewards;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bClaimable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Achievements")
	bool bMasked = false;

	static FAchievementViewData Make(const UAchievementDefinition& Definition, const FAchievementProgressEntry* Entry);
};

UCLASS(Abstract)
class ACHIEVEMENTSUI_API UAchievementEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetData(const FAchievementViewData& InData);

	UFUNCTION(BlueprintPure, Category = "Achievements")
	const FAchievementViewData& GetData() const { return Data; }

	/** Bind the Claim button to this. */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void Claim();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnDataChanged(const FAchievementViewData& NewData);

	/** Fired after the icon finished streaming (null for masked or icon-less entries). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnIconLoaded(UTexture2D* Icon);

	/** Fired once per reward after its icon streamed in (null when the reward has no icon). Index matches Data.Rewards. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnRewardIconLoaded(int32 RewardIndex, UTexture2D* Icon);

private:
	void LoadIcon();
	void LoadRewardIcons();

	UPROPERTY(Transient)
	FAchievementViewData Data;
};

/**
 * Add a panel named "EntryContainer" (Vertical Box, Scroll Box, Wrap Box...) in the designer
 * and set EntryClass. Entries are sorted: claimable, unlocked, in progress, locked.
 */
UCLASS(Abstract)
class ACHIEVEMENTSUI_API UAchievementListWidget : public UAchievementWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void Rebuild();

	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void ClaimAll();

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetUnlockedCount() const { return UnlockedCount; }

	UFUNCTION(BlueprintPure, Category = "Achievements")
	int32 GetTotalCount() const { return EntriesById.Num(); }

protected:
	virtual void BindToComponent(UAchievementComponent& Component) override;
	virtual void UnbindFromComponent(UAchievementComponent& Component) override;

	/** Fired after every rebuild or update; use it for counters like "3 / 20". */
	UFUNCTION(BlueprintImplementableEvent, Category = "Achievements")
	void OnListUpdated();

	UPROPERTY(EditAnywhere, Category = "Achievements")
	TSubclassOf<UAchievementEntryWidget> EntryClass;

	UPROPERTY(EditAnywhere, Category = "Achievements")
	bool bShowHiddenLocked = true;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryContainer;

private:
	UFUNCTION()
	void HandleProgressChanged(const FAchievementProgressEntry& Entry);

	void RecountUnlocked();

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<UAchievementEntryWidget>> EntriesById;

	int32 UnlockedCount = 0;
};
