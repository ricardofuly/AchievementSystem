// Local save storage and the Null platform.

#include "AchievementPlatform.h"
#include "AchievementSaveGame.h"
#include "AchievementTestHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

BEGIN_DEFINE_SPEC(FAchievementStorageSpec, "AchievementSystem.Storage", AchievementTestFlags)
	FString SlotName;
	FString BackupSlotName;

	static FAchievementSaveData MakeData(int32 Value)
	{
		FAchievementSaveData Data;
		FAchievementSnapshotEntry& Entry = Data.Entries.AddDefaulted_GetRef();
		Entry.AchievementId = AchievementTestTags::OneShot;
		Entry.Value = Value;
		Entry.State = EAchievementState::Unlocked;
		Data.PendingUnlocks.Add(TEXT("ACH_TEST"));
		return Data;
	}

	void CorruptSlot(const FString& InSlotName) const
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (InSlotName + TEXT(".sav"));
		FFileHelper::SaveStringToFile(TEXT("definitely not a save game"), *Path);
	}
END_DEFINE_SPEC(FAchievementStorageSpec)

void FAchievementStorageSpec::Define()
{
	BeforeEach([this]()
	{
		SlotName = TEXT("AchievementsTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		BackupSlotName = SlotName + TEXT("_Backup");
	});

	AfterEach([this]()
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		UGameplayStatics::DeleteGameInSlot(BackupSlotName, 0);
	});

	Describe("FLocalSaveGameStorage", [this]()
	{
		It("returns false when nothing was saved", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			FAchievementSaveData Data;
			TestFalse(TEXT("Load"), Storage.Load(Data));
		});

		It("round-trips entries and pending platform operations", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			Storage.SaveBlocking(MakeData(3));

			FAchievementSaveData Loaded;
			TestTrue(TEXT("Load"), Storage.Load(Loaded));
			if (TestEqual(TEXT("Entries"), Loaded.Entries.Num(), 1))
			{
				TestEqual(TEXT("Value"), Loaded.Entries[0].Value, 3);
				TestTrue(TEXT("Id"), Loaded.Entries[0].AchievementId == AchievementTestTags::OneShot);
			}
			TestTrue(TEXT("Pending unlock kept"), Loaded.PendingUnlocks.Contains(TEXT("ACH_TEST")));
		});

		It("writes a backup after a clean load", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			Storage.SaveBlocking(MakeData(3));

			FAchievementSaveData Loaded;
			Storage.Load(Loaded);

			TestTrue(TEXT("Backup exists"), UGameplayStatics::DoesSaveGameExist(BackupSlotName, 0));
		});

		It("restores from the backup when the main slot is corrupt", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			Storage.SaveBlocking(MakeData(5));
			FAchievementSaveData Loaded;
			Storage.Load(Loaded); // creates the backup

			CorruptSlot(SlotName);
			// 0 = expected at least once; the engine's save loader may also log about the bad file.
			AddExpectedError(TEXT("is corrupt or of the wrong type"), EAutomationExpectedErrorFlags::Contains, 0);

			FAchievementSaveData Restored;
			TestTrue(TEXT("Load"), Storage.Load(Restored));
			if (TestEqual(TEXT("Entries"), Restored.Entries.Num(), 1))
			{
				TestEqual(TEXT("Value from backup"), Restored.Entries[0].Value, 5);
			}

			// The main slot is rewritten from the backup, so the next load is clean.
			FAchievementSaveData Again;
			TestTrue(TEXT("Main slot repaired"), Storage.Load(Again));
		});

		It("restores from the backup when the main slot is missing", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			Storage.SaveBlocking(MakeData(2));
			FAchievementSaveData Loaded;
			Storage.Load(Loaded);

			UGameplayStatics::DeleteGameInSlot(SlotName, 0);

			FAchievementSaveData Restored;
			TestTrue(TEXT("Load"), Storage.Load(Restored));
			TestEqual(TEXT("Entries"), Restored.Entries.Num(), 1);
		});

		It("ClearBackup removes the backup slot", [this]()
		{
			FLocalSaveGameStorage Storage(SlotName, 0);
			Storage.SaveBlocking(MakeData(1));
			FAchievementSaveData Loaded;
			Storage.Load(Loaded);

			Storage.ClearBackup();
			TestFalse(TEXT("Backup gone"), UGameplayStatics::DoesSaveGameExist(BackupSlotName, 0));
		});
	});

	Describe("FNullAchievementPlatform", [this]()
	{
		It("is always ready and remembers unlocks until reset", [this]()
		{
			FNullAchievementPlatform Platform;
			TestTrue(TEXT("Ready"), Platform.IsReady());

			bool bUnlocked = true;
			TestTrue(TEXT("Query"), Platform.IsAchievementUnlocked(TEXT("ACH_A"), bUnlocked));
			TestFalse(TEXT("Locked at start"), bUnlocked);

			TestTrue(TEXT("Unlock"), Platform.UnlockAchievement(TEXT("ACH_A")));
			Platform.IsAchievementUnlocked(TEXT("ACH_A"), bUnlocked);
			TestTrue(TEXT("Unlocked"), bUnlocked);

			Platform.ResetAll();
			Platform.IsAchievementUnlocked(TEXT("ACH_A"), bUnlocked);
			TestFalse(TEXT("Locked after reset"), bUnlocked);
		});
	});
}
