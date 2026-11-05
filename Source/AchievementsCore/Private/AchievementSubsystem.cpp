#include "AchievementSubsystem.h"
#include "AchievementDefinition.h"
#include "AchievementPlatform.h"
#include "AchievementSettings.h"
#include "AchievementTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Features/IModularFeatures.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementSubsystem)

void UAchievementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadDefinitions();
	CreateDefaultGranters();
	CreatePlatform();
}

void UAchievementSubsystem::Deinitialize()
{
	// Local players are removed (and flush their saves) before game instance subsystems shut down.
	Platform.Reset();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

void UAchievementSubsystem::LoadDefinitions()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> AssetIds;
	AssetManager.GetPrimaryAssetIdList(UAchievementDefinition::AssetType, AssetIds);

	// Definitions are small and needed before the first event, so a blocking load is acceptable here.
	if (const TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssets(AssetIds))
	{
		Handle->WaitUntilComplete();
	}

	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		RegisterDefinition(AssetManager.GetPrimaryAssetObject<UAchievementDefinition>(AssetId));
	}

	UE_LOG(LogAchievements, Log, TEXT("Loaded %d achievement definitions"), Definitions.Num());
	UE_CLOG(AssetIds.IsEmpty(), LogAchievements, Warning,
		TEXT("No assets of primary type '%s' found. In Project Settings > Asset Manager, the Primary Asset Type must be exactly '%s' and point at your definitions folder."),
		*UAchievementDefinition::AssetType.ToString(), *UAchievementDefinition::AssetType.ToString());
}

bool UAchievementSubsystem::RegisterDefinition(UAchievementDefinition* Definition)
{
	if (!Definition || !Definition->Id.IsValid())
	{
		return false;
	}
	if (DefinitionsById.Contains(Definition->Id))
	{
		UE_LOG(LogAchievements, Error, TEXT("Duplicate achievement id %s in %s"), *Definition->Id.ToString(), *GetPathNameSafe(Definition));
		return false;
	}

	Definitions.Add(Definition);
	DefinitionsById.Add(Definition->Id, Definition);
	for (const FGameplayTag& EventTag : Definition->ListenedEvents)
	{
		DefinitionsByEvent.Add(EventTag, Definition);
	}
	return true;
}

const UAchievementDefinition* UAchievementSubsystem::FindDefinition(const FGameplayTag& AchievementId) const
{
	const UAchievementDefinition* const* Found = DefinitionsById.Find(AchievementId);
	return Found ? *Found : nullptr;
}

TArray<const UAchievementDefinition*> UAchievementSubsystem::GetDefinitionsForEvent(const FGameplayTag& EventTag) const
{
	TArray<const UAchievementDefinition*> Result;
	DefinitionsByEvent.MultiFind(EventTag, Result);
	return Result;
}

TArray<UAchievementDefinition*> UAchievementSubsystem::GetAllDefinitions() const
{
	return ObjectPtrDecay(Definitions);
}

// ---------------------------------------------------------------------------
// Rewards
// ---------------------------------------------------------------------------

void UAchievementSubsystem::RegisterRewardGranter(TSubclassOf<UAchievementReward> RewardClass, TScriptInterface<IAchievementRewardGranter> Granter)
{
	if (RewardClass && Granter.GetObject())
	{
		Granters.Add(RewardClass.Get(), Granter.GetObject());
	}
}

void UAchievementSubsystem::CreateDefaultGranters()
{
	for (const TPair<TSoftClassPtr<UAchievementReward>, TSoftClassPtr<UObject>>& Pair : GetDefault<UAchievementSettings>()->DefaultGranters)
	{
		UClass* RewardClass = Pair.Key.LoadSynchronous();
		UClass* GranterClass = Pair.Value.LoadSynchronous();
		if (!RewardClass || !GranterClass || !GranterClass->ImplementsInterface(UAchievementRewardGranter::StaticClass()))
		{
			UE_LOG(LogAchievements, Error, TEXT("Invalid default granter %s -> %s (granter must implement AchievementRewardGranter)"),
				*Pair.Key.ToString(), *Pair.Value.ToString());
			continue;
		}

		Granters.Add(RewardClass, NewObject<UObject>(this, GranterClass));
	}
}

EAchievementGrantResult UAchievementSubsystem::GrantReward(const UAchievementReward* Reward, APlayerState* Player) const
{
	// Walk up the class hierarchy so a granter for a base reward class covers its subclasses.
	for (const UClass* Class = Reward->GetClass(); Class && Class != UAchievementReward::StaticClass(); Class = Class->GetSuperClass())
	{
		if (const TObjectPtr<UObject>* Granter = Granters.Find(Class))
		{
			return IAchievementRewardGranter::Execute_GrantReward(*Granter, Reward, Player);
		}
	}

	UE_LOG(LogAchievements, Error, TEXT("No granter registered for reward class %s"), *GetNameSafe(Reward->GetClass()));
	return EAchievementGrantResult::Failed;
}

// ---------------------------------------------------------------------------
// Platform
// ---------------------------------------------------------------------------

FName UAchievementSubsystem::GetPlatformName() const
{
	return Platform ? Platform->GetPlatformName() : NAME_None;
}

void UAchievementSubsystem::CreatePlatform()
{
	const FName Preferred = GetDefault<UAchievementSettings>()->PreferredPlatform;

	IModularFeatures& Features = IModularFeatures::Get();
	const FName FeatureName = IAchievementPlatformFactory::GetModularFeatureName();
	for (IAchievementPlatformFactory* Factory : Features.GetModularFeatureImplementations<IAchievementPlatformFactory>(FeatureName))
	{
		if (Factory->GetPlatformName() == Preferred)
		{
			Platform = Factory->CreatePlatform();
			break;
		}
	}

	if (!Platform)
	{
		UE_LOG(LogAchievements, Warning, TEXT("Platform '%s' unavailable, using Null platform"), *Preferred.ToString());
		Platform = MakeShared<FNullAchievementPlatform>();
	}
}
