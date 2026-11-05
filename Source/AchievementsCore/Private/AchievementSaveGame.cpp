#include "AchievementSaveGame.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementSaveGame)

bool FLocalSaveGameStorage::Load(FAchievementSaveData& OutData)
{
	if (LoadSlot(SlotName, OutData))
	{
		// Refresh the backup only from a save that is known to load.
		UGameplayStatics::SaveGameToSlot(MakeSaveObject(OutData), BackupSlotName, UserIndex);
		return true;
	}

	if (LoadSlot(BackupSlotName, OutData))
	{
		UE_LOG(LogAchievements, Warning, TEXT("Achievement save '%s' missing or corrupt; restored from '%s'"), *SlotName, *BackupSlotName);
		UGameplayStatics::SaveGameToSlot(MakeSaveObject(OutData), SlotName, UserIndex);
		return true;
	}

	return false;
}

bool FLocalSaveGameStorage::LoadSlot(const FString& InSlotName, FAchievementSaveData& OutData) const
{
	if (!UGameplayStatics::DoesSaveGameExist(InSlotName, UserIndex))
	{
		return false;
	}

	const UAchievementSaveGame* SaveGame = Cast<UAchievementSaveGame>(UGameplayStatics::LoadGameFromSlot(InSlotName, UserIndex));
	if (!SaveGame)
	{
		UE_LOG(LogAchievements, Error, TEXT("Achievement save '%s' is corrupt or of the wrong type"), *InSlotName);
		return false;
	}

	OutData = SaveGame->Data;

	// Future schema migrations go here, keyed on OutData.SchemaVersion.
	OutData.SchemaVersion = FAchievementSaveData::CurrentSchemaVersion;
	return true;
}

void FLocalSaveGameStorage::SaveAsync(const FAchievementSaveData& Data)
{
	UGameplayStatics::AsyncSaveGameToSlot(MakeSaveObject(Data), SlotName, UserIndex);
}

void FLocalSaveGameStorage::SaveBlocking(const FAchievementSaveData& Data)
{
	UGameplayStatics::SaveGameToSlot(MakeSaveObject(Data), SlotName, UserIndex);
}

void FLocalSaveGameStorage::ClearBackup()
{
	UGameplayStatics::DeleteGameInSlot(BackupSlotName, UserIndex);
}

UAchievementSaveGame* FLocalSaveGameStorage::MakeSaveObject(const FAchievementSaveData& Data) const
{
	UAchievementSaveGame* SaveGame = NewObject<UAchievementSaveGame>();
	SaveGame->Data = Data;
	return SaveGame;
}
