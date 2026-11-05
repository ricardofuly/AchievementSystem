// Editor tooling: Tools > Achievements menu with validation and a Steamworks reference export.

#include "AchievementDefinition.h"
#include "AchievementTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "WebBrowserModule.h"
#include "SWebBrowser.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AchievementsEditor"

namespace AchievementsEditor
{
	static TArray<UAchievementDefinition*> LoadAllDefinitions()
	{
		TArray<FAssetData> Assets;
		FAssetRegistryModule::GetRegistry().GetAssetsByClass(UAchievementDefinition::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

		TArray<UAchievementDefinition*> Definitions;
		for (const FAssetData& Asset : Assets)
		{
			if (UAchievementDefinition* Definition = Cast<UAchievementDefinition>(Asset.GetAsset()))
			{
				Definitions.Add(Definition);
			}
		}

		Definitions.Sort([](const UAchievementDefinition& A, const UAchievementDefinition& B)
		{
			return A.PlatformApiName.LexicalLess(B.PlatformApiName);
		});
		return Definitions;
	}

	static void Notify(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	static FString CsvEscape(const FString& Value)
	{
		return FString::Printf(TEXT("\"%s\""), *Value.Replace(TEXT("\""), TEXT("\"\"")));
	}

	static void ValidateAll()
	{
		int32 ErrorCount = 0;
		int32 WarningCount = 0;
		const TArray<UAchievementDefinition*> Definitions = LoadAllDefinitions();

		for (const UAchievementDefinition* Definition : Definitions)
		{
			FDataValidationContext Context;
			Definition->IsDataValid(Context);

			for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
			{
				if (Issue.Severity == EMessageSeverity::Error)
				{
					++ErrorCount;
					UE_LOG(LogAchievements, Error, TEXT("%s: %s"), *Definition->GetPathName(), *Issue.Message.ToString());
				}
				else
				{
					++WarningCount;
					UE_LOG(LogAchievements, Warning, TEXT("%s: %s"), *Definition->GetPathName(), *Issue.Message.ToString());
				}
			}
		}

		Notify(FText::Format(LOCTEXT("ValidateResult", "Validated {0} achievements: {1} error(s), {2} warning(s). See Output Log."),
			Definitions.Num(), ErrorCount, WarningCount), ErrorCount == 0);
	}

	/**
	 * Steamworks has no bulk import for achievements, so this produces the sheet to fill the
	 * partner site from (Stats & Achievements page): one row per achievement, in API name order.
	 */
	static void ExportSteamworksSheet()
	{
		const TArray<UAchievementDefinition*> Definitions = LoadAllDefinitions();

		TArray<FString> Lines;
		Lines.Add(TEXT("ApiName,DisplayName,Description,Hidden,StatName,TargetValue,Icon,Id"));

		TSet<FName> SeenApiNames;
		int32 SkippedCount = 0;
		for (const UAchievementDefinition* Definition : Definitions)
		{
			if (Definition->PlatformApiName.IsNone() || SeenApiNames.Contains(Definition->PlatformApiName))
			{
				++SkippedCount;
				UE_LOG(LogAchievements, Warning, TEXT("Export skipped %s (empty or duplicate PlatformApiName)"), *Definition->GetPathName());
				continue;
			}
			SeenApiNames.Add(Definition->PlatformApiName);

			Lines.Add(FString::Join(TArray<FString>{
				CsvEscape(Definition->PlatformApiName.ToString()),
				CsvEscape(Definition->DisplayName.ToString()),
				CsvEscape(Definition->Description.ToString()),
				Definition->bHidden ? TEXT("1") : TEXT("0"),
				CsvEscape(Definition->PlatformStatName.IsNone() ? FString() : Definition->PlatformStatName.ToString()),
				FString::FromInt(Definition->TargetValue),
				CsvEscape(Definition->Icon.ToString()),
				CsvEscape(Definition->Id.ToString())
			}, TEXT(",")));
		}

		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Achievements");
		const FString FilePath = Directory / TEXT("SteamworksAchievements.csv");

		if (!FFileHelper::SaveStringArrayToFile(Lines, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8))
		{
			Notify(FText::Format(LOCTEXT("ExportFailed", "Could not write {0}"), FText::FromString(FilePath)), false);
			return;
		}

		Notify(FText::Format(LOCTEXT("ExportDone", "Exported {0} achievement(s), skipped {1}."), SeenApiNames.Num(), SkippedCount), SkippedCount == 0);
		FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FilePath));
	}

	static const FName DocsTabId(TEXT("AchievementsDocumentation"));

	/** Absolute path of Docs/index.html shipped inside the plugin, or empty when missing. */
	static FString GetDocsPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AchievementSystem"));
		const FString DocsPath = Plugin.IsValid()
			? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Docs/index.html"))
			: FString();

		return !DocsPath.IsEmpty() && FPaths::FileExists(DocsPath) ? DocsPath : FString();
	}

	static TSharedRef<SDockTab> SpawnDocsTab(const FSpawnTabArgs& Args)
	{
		const FString DocsPath = GetDocsPath();

		TSharedRef<SWidget> Content = DocsPath.IsEmpty()
			? StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(LOCTEXT("DocsMissing", "Documentation not found (Docs/index.html inside the plugin).")))
			: StaticCastSharedRef<SWidget>(SNew(SWebBrowser)
				.InitialURL(TEXT("file:///") + DocsPath)
				.ShowControls(false)
				.ShowAddressBar(false));

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Content
			];
	}

	static void OpenDocumentationExternal();

	/** Opens the documentation in a dockable editor tab, or the external browser when CEF is unavailable. */
	static void OpenDocumentation()
	{
		// Get() loads the module on demand; IsAvailable() alone is false until something else loaded it.
		if (!IWebBrowserModule::Get().IsWebModuleAvailable())
		{
			Notify(LOCTEXT("NoEmbeddedBrowser", "Embedded browser unavailable; opening the documentation in your web browser."), false);
			OpenDocumentationExternal();
			return;
		}

		FGlobalTabmanager::Get()->TryInvokeTab(DocsTabId);
	}

	/** Fallback for machines where the embedded browser is unavailable. */
	static void OpenDocumentationExternal()
	{
		const FString DocsPath = GetDocsPath();
		if (DocsPath.IsEmpty())
		{
			Notify(LOCTEXT("DocsMissingExternal", "Documentation not found (Docs/index.html inside the plugin)."), false);
			return;
		}
		FPlatformProcess::LaunchURL(*(TEXT("file:///") + DocsPath), nullptr, nullptr);
	}

	static void RegisterMenus()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = Menu->AddSection(TEXT("Achievements"), LOCTEXT("AchievementsSection", "Achievements"));

		Section.AddMenuEntry(
			TEXT("ValidateAchievements"),
			LOCTEXT("ValidateLabel", "Validate Achievements"),
			LOCTEXT("ValidateTooltip", "Checks every AchievementDefinition (ids, API names, events, rewards)."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&ValidateAll)));

		Section.AddMenuEntry(
			TEXT("ExportSteamworks"),
			LOCTEXT("ExportLabel", "Export Steamworks Sheet"),
			LOCTEXT("ExportTooltip", "Writes Saved/Achievements/SteamworksAchievements.csv to fill the Steamworks partner site."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&ExportSteamworksSheet)));

		Section.AddMenuEntry(
			TEXT("AchievementsDocumentation"),
			LOCTEXT("DocsLabel", "Documentation"),
			LOCTEXT("DocsTooltip", "Opens the Achievement System documentation in an editor tab."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Help")),
			FUIAction(FExecuteAction::CreateStatic(&OpenDocumentation)));

		Section.AddMenuEntry(
			TEXT("AchievementsDocumentationExternal"),
			LOCTEXT("DocsExternalLabel", "Documentation (External Browser)"),
			LOCTEXT("DocsExternalTooltip", "Opens the documentation in your default web browser."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&OpenDocumentationExternal)));
	}
}

class FAchievementsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AchievementsEditor::DocsTabId, FOnSpawnTab::CreateStatic(&AchievementsEditor::SpawnDocsTab))
			.SetDisplayName(LOCTEXT("DocsTabTitle", "Achievements Docs"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Help")))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&AchievementsEditor::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AchievementsEditor::DocsTabId);
		}

		if (UToolMenus* ToolMenus = UToolMenus::TryGet())
		{
			ToolMenus->RemoveSection(TEXT("LevelEditor.MainMenu.Tools"), TEXT("Achievements"));
		}
	}
};

IMPLEMENT_MODULE(FAchievementsEditorModule, AchievementsEditor)

#undef LOCTEXT_NAMESPACE
