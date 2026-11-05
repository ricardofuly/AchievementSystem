// Per-game-instance services shared by every player: definition registry, reward granters and the platform.
// Per-player save and platform sync live in UAchievementLocalPlayerSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AchievementReward.h"
#include "AchievementSubsystem.generated.h"

class IAchievementPlatform;
class UAchievementDefinition;

UCLASS()
class ACHIEVEMENTSCORE_API UAchievementSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Registry -----------------------------------------------------------

	/**
	 * Adds a definition at runtime (DLC, generated content, tests). Asset definitions are registered
	 * automatically on startup. Fails on an invalid or duplicate Id.
	 */
	bool RegisterDefinition(UAchievementDefinition* Definition);

	const UAchievementDefinition* FindDefinition(const FGameplayTag& AchievementId) const;
	TArray<const UAchievementDefinition*> GetDefinitionsForEvent(const FGameplayTag& EventTag) const;

	UFUNCTION(BlueprintPure, Category = "Achievements")
	TArray<UAchievementDefinition*> GetAllDefinitions() const;

	// --- Rewards (host) -----------------------------------------------------

	/** Registers the game-side handler for a reward class (and its subclasses). */
	UFUNCTION(BlueprintCallable, Category = "Achievements")
	void RegisterRewardGranter(TSubclassOf<UAchievementReward> RewardClass, TScriptInterface<IAchievementRewardGranter> Granter);

	EAchievementGrantResult GrantReward(const UAchievementReward* Reward, APlayerState* Player) const;

	// --- Platform -----------------------------------------------------------

	/** Shared by every local player; only the primary local player writes to it. */
	IAchievementPlatform* GetPlatform() const { return Platform.Get(); }

	FName GetPlatformName() const;

private:
	void LoadDefinitions();
	void CreateDefaultGranters();
	void CreatePlatform();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAchievementDefinition>> Definitions;

	TMap<FGameplayTag, const UAchievementDefinition*> DefinitionsById;
	TMultiMap<FGameplayTag, const UAchievementDefinition*> DefinitionsByEvent;

	/** Reward class -> object implementing IAchievementRewardGranter. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UClass>, TObjectPtr<UObject>> Granters;

	TSharedPtr<IAchievementPlatform> Platform;
};
