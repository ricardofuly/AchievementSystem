#include "AchievementViewData.h"
#include "AchievementDefinition.h"
#include "AchievementReward.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementViewData)

#define LOCTEXT_NAMESPACE "Achievements"

FAchievementViewData FAchievementViewData::Make(const UAchievementDefinition& Definition, const FAchievementProgressEntry* Entry)
{
	FAchievementViewData View;
	View.Id = Definition.Id;
	View.State = Entry ? Entry->State : EAchievementState::Locked;
	View.Value = Entry ? Entry->Value : 0;
	View.TargetValue = Definition.TargetValue;
	View.Progress = FMath::Clamp(static_cast<float>(View.Value) / FMath::Max(1, View.TargetValue), 0.f, 1.f);
	View.bHasRewards = !Definition.Rewards.IsEmpty();
	View.bClaimable = View.bHasRewards && View.State == EAchievementState::Unlocked;
	View.bMasked = Definition.bHidden && !View.IsUnlocked();

	if (View.bMasked)
	{
		View.DisplayName = LOCTEXT("HiddenName", "Hidden achievement");
		View.Description = LOCTEXT("HiddenDescription", "Keep playing to discover it.");
		return View;
	}

	View.DisplayName = Definition.DisplayName;
	View.Description = Definition.Description;
	View.Icon = Definition.Icon;

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
	return View;
}

#undef LOCTEXT_NAMESPACE
