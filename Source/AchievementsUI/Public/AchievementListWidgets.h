// Achievement list screen: a list widget that fills a panel with entry widgets.

#pragma once

#include "CoreMinimal.h"
#include "AchievementTypes.h"
#include "AchievementViewData.h"
#include "AchievementWidgetBase.h"
#include "AchievementListWidgets.generated.h"

class UPanelWidget;
class UTexture2D;

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
	virtual void BindToAchievements(UAchievementLocalPlayerSubsystem& InAchievements) override;
	virtual void UnbindFromAchievements(UAchievementLocalPlayerSubsystem& InAchievements) override;
	virtual void HandleAchievementsReady() override;

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
