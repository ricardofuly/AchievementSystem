// Local persistence for the signed-in user (P2P: each player owns their save).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AchievementTypes.h"
#include "AchievementSaveGame.generated.h"

USTRUCT()
struct ACHIEVEMENTSCORE_API FAchievementSaveData
{
	GENERATED_BODY()

	static constexpr int32 CurrentSchemaVersion = 1;

	UPROPERTY(SaveGame)
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY(SaveGame)
	TArray<FAchievementSnapshotEntry> Entries;

	/** Platform operations not yet confirmed; survives crashes and offline sessions. */
	UPROPERTY(SaveGame)
	TSet<FName> PendingUnlocks;

	UPROPERTY(SaveGame)
	TMap<FName, int32> PendingStats;
};

UCLASS()
class ACHIEVEMENTSCORE_API UAchievementSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FAchievementSaveData Data;
};

/** Storage abstraction so the local SaveGame can later be swapped for cloud or a backend. */
class ACHIEVEMENTSCORE_API IAchievementStorage
{
public:
	virtual ~IAchievementStorage() = default;
	virtual bool Load(FAchievementSaveData& OutData) = 0;
	virtual void SaveAsync(const FAchievementSaveData& Data) = 0;
	virtual void SaveBlocking(const FAchievementSaveData& Data) = 0;
	virtual void ClearBackup() {}
};

/**
 * SaveGame slot plus a "<Slot>_Backup" copy of the last save that loaded cleanly.
 * A corrupt or missing main slot falls back to the backup instead of starting from zero.
 */
class ACHIEVEMENTSCORE_API FLocalSaveGameStorage final : public IAchievementStorage
{
public:
	FLocalSaveGameStorage(FString InSlotName, int32 InUserIndex)
		: SlotName(MoveTemp(InSlotName)), BackupSlotName(SlotName + TEXT("_Backup")), UserIndex(InUserIndex) {}

	virtual bool Load(FAchievementSaveData& OutData) override;
	virtual void SaveAsync(const FAchievementSaveData& Data) override;
	virtual void SaveBlocking(const FAchievementSaveData& Data) override;
	virtual void ClearBackup() override;

private:
	bool LoadSlot(const FString& InSlotName, FAchievementSaveData& OutData) const;
	UAchievementSaveGame* MakeSaveObject(const FAchievementSaveData& Data) const;

	FString SlotName;
	FString BackupSlotName;
	int32 UserIndex = 0;
};
