// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SlaterContentWatcher.h"
#include "SlaterPreviewTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SlaterUIEditor"

namespace SlaterUIEditorModulePrivate
{
	const FName PreviewTabName(TEXT("SlaterLayoutPreview"));
	TWeakPtr<SSlaterPreview> ActivePreview;
}
using namespace SlaterUIEditorModulePrivate;

class FSlaterUIEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Watcher.Start();

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PreviewTabName, FOnSpawnTab::CreateStatic(&FSlaterUIEditorModule::SpawnPreviewTab))
			.SetDisplayName(LOCTEXT("PreviewTab", "Slater Layout Preview"))
			.SetTooltipText(LOCTEXT("PreviewTabTip", "Shows a .sltr layout and refreshes it when the file or a style changes."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

		LayoutCommand = MakeUnique<FAutoConsoleCommand>(
			TEXT("Slater.Layout"),
			TEXT("Slater.Layout [LayoutId]: opens the layout preview tab and shows the layout, if one is given."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&FSlaterUIEditorModule::OnLayoutCommand));
	}

	virtual void ShutdownModule() override
	{
		LayoutCommand.Reset();
		Watcher.Stop();
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PreviewTabName);
		}
	}

private:
	FSlaterContentWatcher Watcher;
	TUniquePtr<FAutoConsoleCommand> LayoutCommand;

	static TSharedRef<SDockTab> SpawnPreviewTab(const FSpawnTabArgs& Args)
	{
		const TSharedRef<SSlaterPreview> Preview = SNew(SSlaterPreview)
			.OnRescan(FSimpleDelegate::CreateLambda([]()
			{
				if (FSlaterUIEditorModule* Module = FModuleManager::GetModulePtr<FSlaterUIEditorModule>(TEXT("SlaterUIEditor")))
				{
					Module->Watcher.Rescan();
				}
			}));
		ActivePreview = Preview;
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[Preview];
	}

	static void OnLayoutCommand(const TArray<FString>& Args)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(PreviewTabName);
		if (Args.Num() > 0)
		{
			if (const TSharedPtr<SSlaterPreview> Preview = ActivePreview.Pin())
			{
				Preview->SetLayout(Args[0]);
			}
		}
	}
};

IMPLEMENT_MODULE(FSlaterUIEditorModule, SlaterUIEditor)

#undef LOCTEXT_NAMESPACE
