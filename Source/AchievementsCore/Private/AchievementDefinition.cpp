#include "AchievementDefinition.h"
#include "AchievementCondition.h"
#include "AchievementReward.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementDefinition)

#define LOCTEXT_NAMESPACE "AchievementDefinition"

const FPrimaryAssetType UAchievementDefinition::AssetType(TEXT("AchievementDefinition"));

FPrimaryAssetId UAchievementDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UAchievementDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!Id.IsValid())
	{
		Context.AddError(LOCTEXT("MissingId", "Achievement Id tag is not set."));
		Result = EDataValidationResult::Invalid;
	}
	if (PlatformApiName.IsNone())
	{
		Context.AddWarning(LOCTEXT("MissingApiName", "PlatformApiName is empty; this achievement will not sync to Steam."));
	}
	if (ListenedEvents.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoEvents", "Achievement listens to no events and can never progress."));
		Result = EDataValidationResult::Invalid;
	}
	if (Prerequisites.HasTagExact(Id))
	{
		Context.AddError(LOCTEXT("SelfPrereq", "Achievement lists itself as a prerequisite."));
		Result = EDataValidationResult::Invalid;
	}
	// Uniqueness across all definitions, read from Asset Registry tags so nothing has to be loaded.
	TArray<FAssetData> OtherAssets;
	FAssetRegistryModule::GetRegistry().GetAssetsByClass(StaticClass()->GetClassPathName(), OtherAssets, /*bSearchSubClasses*/ true);

	for (const FAssetData& Other : OtherAssets)
	{
		if (Other.PackageName == GetPackage()->GetFName())
		{
			continue;
		}

		FGameplayTag OtherId;
		OtherId.FromExportString(Other.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAchievementDefinition, Id)));
		if (Id.IsValid() && OtherId == Id)
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateId", "Id {0} is also used by {1}."),
				FText::FromString(Id.ToString()), FText::FromName(Other.PackageName)));
			Result = EDataValidationResult::Invalid;
		}

		if (!PlatformApiName.IsNone() && Other.GetTagValueRef<FName>(GET_MEMBER_NAME_CHECKED(UAchievementDefinition, PlatformApiName)) == PlatformApiName)
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateApiName", "PlatformApiName {0} is also used by {1}."),
				FText::FromName(PlatformApiName), FText::FromName(Other.PackageName)));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (Rewards.Num() > 32)
	{
		Context.AddError(LOCTEXT("TooManyRewards", "An achievement supports at most 32 rewards."));
		Result = EDataValidationResult::Invalid;
	}
	if (Rewards.Contains(nullptr))
	{
		Context.AddError(LOCTEXT("NullReward", "Rewards contains an empty entry."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
