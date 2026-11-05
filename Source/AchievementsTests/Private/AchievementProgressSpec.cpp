// Host-side progress evaluation: events, targets, prerequisites and conditions.

#include "AchievementComponent.h"
#include "AchievementCondition.h"
#include "AchievementDefinition.h"
#include "AchievementSubsystem.h"
#include "AchievementTestHelpers.h"

using namespace AchievementTestTags;

BEGIN_DEFINE_SPEC(FAchievementProgressSpec, "AchievementSystem.Progress", AchievementTestFlags)
	FAchievementTestEnvironment Env;
	UAchievementComponent* Player = nullptr;
END_DEFINE_SPEC(FAchievementProgressSpec)

void FAchievementProgressSpec::Define()
{
	BeforeEach([this]()
	{
		Env.Setup(/*bAutoClaimRewards*/ true);
		Player = Env.SpawnPlayer();
	});

	AfterEach([this]()
	{
		Player = nullptr;
		Env.Teardown();
	});

	Describe("Registry", [this]()
	{
		It("finds runtime definitions by id and by event", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);

			TestNotNull(TEXT("By id"), Env.Subsystem->FindDefinition(OneShot));
			TestEqual(TEXT("By event"), Env.Subsystem->GetDefinitionsForEvent(EventPing).Num(), 1);
		});

		It("rejects a duplicate id", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);

			AddExpectedError(TEXT("Duplicate achievement id"), EAutomationExpectedErrorFlags::Contains, 1);
			UAchievementDefinition* Duplicate = NewObject<UAchievementDefinition>(GetTransientPackage());
			Duplicate->Id = OneShot;
			TestFalse(TEXT("Registered"), Env.Subsystem->RegisterDefinition(Duplicate));
		});
	});

	Describe("ReportEvent", [this]()
	{
		It("unlocks a one-shot achievement with a single event", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);
			Env.Report(*Player, EventPing);

			TestTrue(TEXT("Unlocked"), Player->IsUnlocked(OneShot));
		});

		It("accumulates incremental progress and caps it at the target", [this]()
		{
			Env.AddDefinition(Counter, EventPing, /*TargetValue*/ 3);

			Env.Report(*Player, EventPing, 1);
			TestEqual(TEXT("Value after 1"), FAchievementTestEnvironment::GetValue(*Player, Counter), 1);
			TestTrue(TEXT("In progress"), FAchievementTestEnvironment::GetState(*Player, Counter) == EAchievementState::InProgress);

			Env.Report(*Player, EventPing, 10);
			TestEqual(TEXT("Value capped"), FAchievementTestEnvironment::GetValue(*Player, Counter), 3);
			TestTrue(TEXT("Unlocked"), Player->IsUnlocked(Counter));
		});

		It("ignores events the achievement does not listen to", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);
			Env.Report(*Player, EventOther);

			TestFalse(TEXT("Unlocked"), Player->IsUnlocked(OneShot));
		});

		It("ignores zero and negative amounts", [this]()
		{
			Env.AddDefinition(Counter, EventPing, /*TargetValue*/ 3);
			Env.Report(*Player, EventPing, 0);
			Env.Report(*Player, EventPing, -5);

			TestEqual(TEXT("Value"), FAchievementTestEnvironment::GetValue(*Player, Counter), 0);
		});

		It("feeds every achievement listening to the same event", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);
			Env.AddDefinition(Counter, EventPing, /*TargetValue*/ 2);
			Env.Report(*Player, EventPing);

			TestTrue(TEXT("One-shot unlocked"), Player->IsUnlocked(OneShot));
			TestEqual(TEXT("Counter advanced"), FAchievementTestEnvironment::GetValue(*Player, Counter), 1);
		});

		It("stops counting once unlocked", [this]()
		{
			Env.AddDefinition(Counter, EventPing, /*TargetValue*/ 2);
			Env.Report(*Player, EventPing, 2);
			Env.Report(*Player, EventPing, 2);

			TestEqual(TEXT("Value"), FAchievementTestEnvironment::GetValue(*Player, Counter), 2);
		});

		It("keeps players independent", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);
			UAchievementComponent* Other = Env.SpawnPlayer();
			Env.Report(*Player, EventPing);

			TestTrue(TEXT("Reporter unlocked"), Player->IsUnlocked(OneShot));
			TestFalse(TEXT("Other player untouched"), Other->IsUnlocked(OneShot));
		});
	});

	Describe("Prerequisites", [this]()
	{
		It("block progress until the prerequisite is unlocked", [this]()
		{
			Env.AddDefinition(OneShot, EventPing);
			UAchievementDefinition* GatedDefinition = Env.AddDefinition(Gated, EventOther);
			GatedDefinition->Prerequisites.AddTag(OneShot);

			Env.Report(*Player, EventOther);
			TestFalse(TEXT("Blocked"), Player->IsUnlocked(Gated));

			Env.Report(*Player, EventPing);
			Env.Report(*Player, EventOther);
			TestTrue(TEXT("Unlocked after prerequisite"), Player->IsUnlocked(Gated));
		});
	});

	Describe("Conditions", [this]()
	{
		It("Requires Context Tags only counts events carrying the tags", [this]()
		{
			UAchievementDefinition* Definition = Env.AddDefinition(Conditional, EventPing);
			UAchievementCondition_RequiresTags* Condition = NewObject<UAchievementCondition_RequiresTags>(Definition);
			Condition->RequiredTags.AddTag(ContextHard);
			Definition->Conditions.Add(Condition);

			Env.Report(*Player, EventPing);
			TestFalse(TEXT("Without tag"), Player->IsUnlocked(Conditional));

			FGameplayTagContainer Context;
			Context.AddTag(ContextHard);
			Env.Report(*Player, EventPing, 1, Context);
			TestTrue(TEXT("With tag"), Player->IsUnlocked(Conditional));
		});
	});
}
