#include "AchievementTypes.h"
#include "AchievementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementTypes)

DEFINE_LOG_CATEGORY(LogAchievements);

void FAchievementProgressEntry::PostReplicatedAdd(const FAchievementProgressArray& InArray)
{
	if (InArray.Owner)
	{
		InArray.Owner->HandleEntryChanged(*this);
	}
}

void FAchievementProgressEntry::PostReplicatedChange(const FAchievementProgressArray& InArray)
{
	if (InArray.Owner)
	{
		InArray.Owner->HandleEntryChanged(*this);
	}
}

namespace AchievementMerge
{
	/** Earliest known unlock time; 0 means "not unlocked". */
	static int64 EarliestUnlock(int64 A, int64 B)
	{
		return A == 0 ? B : (B == 0 ? A : FMath::Min(A, B));
	}
}

bool FAchievementProgressEntry::MergeFrom(const FAchievementSnapshotEntry& Saved, int32 TargetValue)
{
	const FAchievementProgressEntry Before = *this;

	Value = FMath::Clamp(FMath::Max(Value, Saved.Value), 0, TargetValue);
	State = FMath::Max(State, Saved.State);
	UnlockedAtUtcTicks = AchievementMerge::EarliestUnlock(UnlockedAtUtcTicks, Saved.UnlockedAtUtcTicks);
	ClaimedRewardMask |= Saved.ClaimedRewardMask;

	return Value != Before.Value || State != Before.State
		|| UnlockedAtUtcTicks != Before.UnlockedAtUtcTicks || ClaimedRewardMask != Before.ClaimedRewardMask;
}

bool FAchievementSnapshotEntry::MergeFrom(const FAchievementProgressEntry& Entry)
{
	const FAchievementSnapshotEntry Before = *this;

	Value = FMath::Max(Value, Entry.Value);
	State = FMath::Max(State, Entry.State);
	UnlockedAtUtcTicks = AchievementMerge::EarliestUnlock(UnlockedAtUtcTicks, Entry.UnlockedAtUtcTicks);
	ClaimedRewardMask |= Entry.ClaimedRewardMask;

	return Value != Before.Value || State != Before.State
		|| UnlockedAtUtcTicks != Before.UnlockedAtUtcTicks || ClaimedRewardMask != Before.ClaimedRewardMask;
}

FAchievementProgressEntry* FAchievementProgressArray::Find(const FGameplayTag& Id)
{
	return Entries.FindByPredicate([&Id](const FAchievementProgressEntry& Entry) { return Entry.AchievementId == Id; });
}

const FAchievementProgressEntry* FAchievementProgressArray::Find(const FGameplayTag& Id) const
{
	return Entries.FindByPredicate([&Id](const FAchievementProgressEntry& Entry) { return Entry.AchievementId == Id; });
}

FAchievementProgressEntry& FAchievementProgressArray::FindOrAdd(const FGameplayTag& Id)
{
	if (FAchievementProgressEntry* Existing = Find(Id))
	{
		return *Existing;
	}

	FAchievementProgressEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.AchievementId = Id;
	MarkItemDirty(NewEntry);
	return NewEntry;
}
