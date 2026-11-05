// Progress merge rules: saved and host progress may only ever move forward.

#include "AchievementTestHelpers.h"

BEGIN_DEFINE_SPEC(FAchievementMergeSpec, "AchievementSystem.Merge", AchievementTestFlags)
END_DEFINE_SPEC(FAchievementMergeSpec)

void FAchievementMergeSpec::Define()
{
	Describe("Saving replicated progress (FAchievementSnapshotEntry::MergeFrom)", [this]()
	{
		It("never lowers value or state", [this]()
		{
			FAchievementSnapshotEntry Saved;
			Saved.Value = 7;
			Saved.State = EAchievementState::Unlocked;

			FAchievementProgressEntry Replicated;
			Replicated.Value = 2;
			Replicated.State = EAchievementState::InProgress;

			TestFalse(TEXT("Lower progress reports no change"), Saved.MergeFrom(Replicated));
			TestEqual(TEXT("Value"), Saved.Value, 7);
			TestTrue(TEXT("State stays Unlocked"), Saved.State == EAchievementState::Unlocked);
		});

		It("takes higher value and state", [this]()
		{
			FAchievementSnapshotEntry Saved;
			Saved.Value = 1;

			FAchievementProgressEntry Replicated;
			Replicated.Value = 4;
			Replicated.State = EAchievementState::InProgress;

			TestTrue(TEXT("Higher progress reports a change"), Saved.MergeFrom(Replicated));
			TestEqual(TEXT("Value"), Saved.Value, 4);
			TestTrue(TEXT("State"), Saved.State == EAchievementState::InProgress);
		});

		It("keeps the earliest unlock time and unions claimed rewards", [this]()
		{
			FAchievementSnapshotEntry Saved;
			Saved.UnlockedAtUtcTicks = 200;
			Saved.ClaimedRewardMask = 0b01;

			FAchievementProgressEntry Replicated;
			Replicated.UnlockedAtUtcTicks = 100;
			Replicated.ClaimedRewardMask = 0b10;
			Saved.MergeFrom(Replicated);

			TestEqual(TEXT("Earliest unlock"), Saved.UnlockedAtUtcTicks, int64(100));
			TestEqual(TEXT("Mask union"), Saved.ClaimedRewardMask, uint32(0b11));
		});

		It("ignores a zero unlock time", [this]()
		{
			FAchievementSnapshotEntry Saved;
			Saved.UnlockedAtUtcTicks = 200;

			FAchievementProgressEntry Replicated;
			Saved.MergeFrom(Replicated);

			TestEqual(TEXT("Unlock time kept"), Saved.UnlockedAtUtcTicks, int64(200));
		});
	});

	Describe("Applying a client snapshot on the host (FAchievementProgressEntry::MergeFrom)", [this]()
	{
		It("keeps progress the host counted before the snapshot arrived", [this]()
		{
			FAchievementProgressEntry Host;
			Host.Value = 5;

			FAchievementSnapshotEntry Snapshot;
			Snapshot.Value = 3;
			Host.MergeFrom(Snapshot, /*TargetValue*/ 10);

			TestEqual(TEXT("Value"), Host.Value, 5);
		});

		It("clamps snapshot values to the target", [this]()
		{
			FAchievementProgressEntry Host;

			FAchievementSnapshotEntry Snapshot;
			Snapshot.Value = 9999;
			Host.MergeFrom(Snapshot, /*TargetValue*/ 10);

			TestEqual(TEXT("Value clamped"), Host.Value, 10);
		});

		It("clamps negative snapshot values to zero", [this]()
		{
			FAchievementProgressEntry Host;

			FAchievementSnapshotEntry Snapshot;
			Snapshot.Value = -50;
			Host.MergeFrom(Snapshot, /*TargetValue*/ 10);

			TestEqual(TEXT("Value"), Host.Value, 0);
		});

		It("reports no change for an identical snapshot", [this]()
		{
			FAchievementProgressEntry Host;
			Host.Value = 2;
			Host.State = EAchievementState::InProgress;

			FAchievementSnapshotEntry Snapshot;
			Snapshot.Value = 2;
			Snapshot.State = EAchievementState::InProgress;

			TestFalse(TEXT("No change"), Host.MergeFrom(Snapshot, /*TargetValue*/ 10));
		});
	});
}
