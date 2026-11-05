// Reward granting: auto/manual claim, Deferred/Failed handling and the never-twice guarantee.

#include "AchievementComponent.h"
#include "AchievementDefinition.h"
#include "AchievementTestHelpers.h"

using namespace AchievementTestTags;

BEGIN_DEFINE_SPEC(FAchievementRewardsSpec, "AchievementSystem.Rewards", AchievementTestFlags)
	FAchievementTestEnvironment Env;
	UAchievementComponent* Player = nullptr;

	/** Rewarded achievement with a Gold reward and, optionally, a Gems reward. */
	void AddRewardedAchievement(bool bWithGems)
	{
		UAchievementDefinition* Definition = Env.AddDefinition(Rewarded, EventPing);
		Env.AddCurrencyReward(*Definition, Gold);
		if (bWithGems)
		{
			Env.AddCurrencyReward(*Definition, Gems);
		}
	}

	EAchievementState State() const { return FAchievementTestEnvironment::GetState(*Player, Rewarded); }
END_DEFINE_SPEC(FAchievementRewardsSpec)

void FAchievementRewardsSpec::Define()
{
	AfterEach([this]()
	{
		Player = nullptr;
		Env.Teardown();
	});

	Describe("Auto-claim", [this]()
	{
		BeforeEach([this]()
		{
			Env.Setup(/*bAutoClaimRewards*/ true);
			Player = Env.SpawnPlayer();
		});

		It("grants every reward on unlock and marks the achievement claimed", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ true);
			Env.Report(*Player, EventPing);

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 1);
			TestEqual(TEXT("Gems"), Env.Granter->GetGranted(Gems), 1);
			TestTrue(TEXT("RewardClaimed"), State() == EAchievementState::RewardClaimed);
		});

		It("never grants twice", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ false);
			Env.Report(*Player, EventPing);
			Env.Report(*Player, EventPing);
			Player->RetryPendingRewards();
			Player->RequestClaimRewards(Rewarded);

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 1);
		});

		It("keeps the achievement claimable when a reward is Deferred", [this]()
		{
			Env.Granter->ResultByCurrency.Add(Gems, EAchievementGrantResult::Deferred);
			AddRewardedAchievement(/*bWithGems*/ true);

			AddExpectedError(TEXT("were not fully granted"), EAutomationExpectedErrorFlags::Contains, 0);
			Env.Report(*Player, EventPing);

			TestTrue(TEXT("Still Unlocked"), State() == EAchievementState::Unlocked);
			TestEqual(TEXT("Gold granted"), Env.Granter->GetGranted(Gold), 1);
			TestEqual(TEXT("Gems pending"), Env.Granter->GetGranted(Gems), 0);
			TestTrue(TEXT("Listed as claimable"), Player->GetClaimableAchievements().Contains(Rewarded));
		});

		It("RetryPendingRewards delivers Deferred rewards without repeating granted ones", [this]()
		{
			Env.Granter->ResultByCurrency.Add(Gems, EAchievementGrantResult::Deferred);
			AddRewardedAchievement(/*bWithGems*/ true);
			AddExpectedError(TEXT("were not fully granted"), EAutomationExpectedErrorFlags::Contains, 0);
			Env.Report(*Player, EventPing);

			Env.Granter->ResultByCurrency.Remove(Gems);
			Player->RetryPendingRewards();

			TestEqual(TEXT("Gold not repeated"), Env.Granter->GetGranted(Gold), 1);
			TestEqual(TEXT("Gems delivered"), Env.Granter->GetGranted(Gems), 1);
			TestTrue(TEXT("RewardClaimed"), State() == EAchievementState::RewardClaimed);
		});

		It("RetryPendingRewards skips Failed rewards", [this]()
		{
			Env.Granter->ResultByCurrency.Add(Gold, EAchievementGrantResult::Failed);
			AddRewardedAchievement(/*bWithGems*/ false);
			AddExpectedError(TEXT("failed; it will only be retried"), EAutomationExpectedErrorFlags::Contains, 0);
			AddExpectedError(TEXT("were not fully granted"), EAutomationExpectedErrorFlags::Contains, 0);
			Env.Report(*Player, EventPing);

			const int32 CallsAfterUnlock = Env.Granter->GetCalls();
			Env.Granter->ResultByCurrency.Remove(Gold);
			Player->RetryPendingRewards();

			TestEqual(TEXT("Granter not called again"), Env.Granter->GetCalls(), CallsAfterUnlock);
			TestTrue(TEXT("Still Unlocked"), State() == EAchievementState::Unlocked);
		});

		It("a manual claim retries Failed rewards", [this]()
		{
			Env.Granter->ResultByCurrency.Add(Gold, EAchievementGrantResult::Failed);
			AddRewardedAchievement(/*bWithGems*/ false);
			AddExpectedError(TEXT("failed; it will only be retried"), EAutomationExpectedErrorFlags::Contains, 0);
			AddExpectedError(TEXT("were not fully granted"), EAutomationExpectedErrorFlags::Contains, 0);
			Env.Report(*Player, EventPing);

			Env.Granter->ResultByCurrency.Remove(Gold);
			Player->RequestClaimRewards(Rewarded);

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 1);
			TestTrue(TEXT("RewardClaimed"), State() == EAchievementState::RewardClaimed);
		});
	});

	Describe("Manual claim", [this]()
	{
		BeforeEach([this]()
		{
			Env.Setup(/*bAutoClaimRewards*/ false);
			Player = Env.SpawnPlayer();
		});

		It("does not grant on unlock", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ false);
			Env.Report(*Player, EventPing);

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 0);
			TestTrue(TEXT("Unlocked"), State() == EAchievementState::Unlocked);
			TestTrue(TEXT("Claimable"), Player->GetClaimableAchievements().Contains(Rewarded));
		});

		It("grants when the player claims", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ false);
			Env.Report(*Player, EventPing);
			Player->RequestClaimRewards(Rewarded);

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 1);
			TestTrue(TEXT("RewardClaimed"), State() == EAchievementState::RewardClaimed);
		});

		It("ClaimAll grants every pending achievement", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ false);
			UAchievementDefinition* Second = Env.AddDefinition(OneShot, EventPing);
			Env.AddCurrencyReward(*Second, Gems);
			Env.Report(*Player, EventPing);

			Player->RequestClaimAllRewards();

			TestEqual(TEXT("Gold"), Env.Granter->GetGranted(Gold), 1);
			TestEqual(TEXT("Gems"), Env.Granter->GetGranted(Gems), 1);
			TestEqual(TEXT("Nothing left to claim"), Player->GetClaimableAchievements().Num(), 0);
		});

		It("ignores a claim for a locked achievement", [this]()
		{
			AddRewardedAchievement(/*bWithGems*/ false);
			Player->RequestClaimRewards(Rewarded);

			TestEqual(TEXT("Granter not called"), Env.Granter->GetCalls(), 0);
		});
	});
}
