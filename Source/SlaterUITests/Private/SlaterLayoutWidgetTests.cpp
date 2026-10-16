// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "GameplayTagContainer.h"
#include "Interfaces/IPluginManager.h"
#include "SlaterBindTestKit.h"
#include "SlaterGameFeatureAction.h"
#include "SlaterLayoutWidget.h"
#include "SlaterObjectScope.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"
#include "SlaterSlotHost.h"

namespace
{
	TArray<FString> LayoutOptionsOf(USlaterLayoutWidget& Widget)
	{
		UFunction* Function = Widget.FindFunction(TEXT("GetLayoutOptions"));
		struct FParams { TArray<FString> ReturnValue; } Params;
		if (Function)
		{
			Widget.ProcessEvent(Function, &Params);
		}
		return Params.ReturnValue;
	}
}

/** The designer path (T-352): `USlaterLayoutWidget` in the UMG palette and the `UUserWidget` the game feature action shows. */
TEST_CLASS(SlaterLayoutWidgetTests, "BRS.SlaterUI.LayoutWidget")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TEST_METHOD(TheWidget_BuildsItsLayoutIntoAView)
	{
		Fix.Write(TEXT("LwOne.sltr"), TEXT("<vbox><text>one</text></vbox>"));
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		Widget->LayoutId = TEXT("LwOne");
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		const TSharedPtr<SSlaterView> View = Widget->GetView();
		ASSERT_THAT(IsTrue(View.IsValid()));
		ASSERT_THAT(IsTrue(&Slate.Get() == View.Get()));
		ASSERT_THAT(IsTrue(View->GetLayoutId() == TEXT("LwOne")));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("one"))));
	}

	TEST_METHOD(AnEmptyLayoutId_ShowsNothing_AndSettingOneShowsIt)
	{
		Fix.Write(TEXT("LwTwo.sltr"), TEXT("<vbox><text>two</text></vbox>"));
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(Slate).Num(), 0));
		Widget->SetLayoutId(TEXT("LwTwo"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("two"))));
	}

	TEST_METHOD(ABrokenLayout_ShowsItsErrorsInTheView)
	{
		Fix.Write(TEXT("LwBad.sltr"), TEXT("<vbox><nope/></vbox>"));
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		Widget->LayoutId = TEXT("LwBad");
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsTrue(Widget->GetView()->HasErrors()));
	}

	TEST_METHOD(TheLayoutDropdown_ListsTheLayoutsFound)
	{
		Fix.Write(TEXT("LwListed.sltr"), TEXT("<vbox/>"));
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		const TArray<FString> Options = LayoutOptionsOf(*Widget);
		ASSERT_THAT(IsTrue(Options.Contains(TEXT("LwListed"))));
		ASSERT_THAT(IsTrue(Options.Contains(TEXT("Panel"))));
	}

	TEST_METHOD(AViewModel_FeedsTheBindingsAndPushesChanges)
	{
		Fix.Write(TEXT("LwBound.sltr"), TEXT("<vbox><text>{Title}</text></vbox>"));
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("First");
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		Widget->LayoutId = TEXT("LwBound");
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		Widget->SetViewModel(Root);
		ASSERT_THAT(IsFalse(Widget->GetView()->HasErrors()));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[0] == TEXT("First")));
		Root->SetTitle(TEXT("Second"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[0] == TEXT("Second")));
	}

	TEST_METHOD(AViewModelSetBeforeTheBuild_IsUsedByTheBuild)
	{
		Fix.Write(TEXT("LwEarly.sltr"), TEXT("<vbox><text>{Title}</text></vbox>"));
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("Early");
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		Widget->LayoutId = TEXT("LwEarly");
		Widget->SetViewModel(Root);
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsFalse(Widget->GetView()->HasErrors()));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[0] == TEXT("Early")));
	}

	TEST_METHOD(ReleasingTheSlate_ForgetsTheView)
	{
		USlaterLayoutWidget* Widget = Fix.Make<USlaterLayoutWidget>();
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsTrue(Widget->GetView().IsValid()));
		Widget->ReleaseSlateResources(true);
		ASSERT_THAT(IsFalse(Widget->GetView().IsValid()));
	}

};

namespace
{
	/** A slot host that only records what the action asked of it. */
	class FRecordingSlotHost : public ISlaterSlotHost
	{
	public:
		struct FCall { FGameplayTag Slot; FString LayoutId; FName StyleName; uint32 Token = 0; };
		TArray<FCall> Added;
		TArray<uint32> Removed;
		bool bRefuse = false;
		uint32 Last = 0;

		virtual uint32 AddLayout(UWorld&, FGameplayTag Slot, const FString& LayoutId, FName StyleName) override
		{
			if (bRefuse)
			{
				return 0;
			}
			FCall& Call = Added.AddDefaulted_GetRef();
			Call.Slot = Slot;
			Call.LayoutId = LayoutId;
			Call.StyleName = StyleName;
			Call.Token = ++Last;
			return Call.Token;
		}

		virtual void RemoveLayout(UWorld&, uint32 Token) override
		{
			Removed.Add(Token);
		}
	};
}

/** The game feature action (T-352): layouts go to the game's slot host when the feature activates and come out when it deactivates. */
TEST_CLASS(SlaterGameFeatureActionTests, "BRS.SlaterUI.GameFeatureAction")
{
	FSlaterBindFixture Fix;
	FActorTestSpawner Spawner;
	TStrongObjectPtr<USlaterGameFeatureAction_AddLayout> Action;
	TSharedPtr<FRecordingSlotHost> Host;
	/** Whatever host a loaded adapter plugin had set; put back after each test. */
	TSharedPtr<ISlaterSlotHost> PreviousHost;
	FGameplayTag Slot;
	FString PackContent;

	BEFORE_EACH()
	{
		Fix.Init();
		Spawner.InitializeGameSubsystems();
		// A tag the project ships (Config/DefaultGameplayTags.ini); SlaterUI has no tags of its own.
		Slot = FGameplayTag::RequestGameplayTag(TEXT("HUD.Slot.Compass"), false);
		ASSERT_THAT(IsTrue(Slot.IsValid()));

		PreviousHost = Slater::GetSlotHost();
		Host = MakeShared<FRecordingSlotHost>();
		Slater::SetSlotHost(Host);

		Action.Reset(NewObject<USlaterGameFeatureAction_AddLayout>(GetTransientPackage()));
		FSlaterLayoutSlotEntry One;
		One.LayoutId = TEXT("Panel");
		One.StyleName = TEXT("Dusk");
		One.SlotID = Slot;
		FSlaterLayoutSlotEntry Two;
		Two.LayoutId = TEXT("Tabs");
		Two.SlotID = Slot;
		Action->SetLayouts({One, Two});
	}

	AFTER_EACH()
	{
		if (Action.IsValid())
		{
			Action->Deactivate();
		}
		Slater::SetSlotHost(PreviousHost);
		PreviousHost.Reset();
		Fix.Shutdown();
		if (!PackContent.IsEmpty())
		{
			// A failed assertion in the pack test must not leave its folders in every later test.
			Slater::RemoveLayoutRoot(PackContent / TEXT("Layouts"));
			Slater::RemoveStyleRoot(PackContent / TEXT("Styles"));
			TArray<FSlaterError> Errors;
			Slater::ReloadStyles(Errors);
			PackContent.Reset();
		}
	}

	TEST_METHOD(Activating_HandsEachLayoutToTheHost_ForTheGameWorldsAlreadyRunning)
	{
		Action->Activate();
		ASSERT_THAT(IsTrue(Action->IsActive()));
		ASSERT_THAT(AreEqual(Host->Added.Num(), 2));
		ASSERT_THAT(IsTrue(Host->Added[0].LayoutId == TEXT("Panel") && Host->Added[0].StyleName == FName(TEXT("Dusk")) && Host->Added[0].Slot == Slot));
		ASSERT_THAT(IsTrue(Host->Added[1].LayoutId == TEXT("Tabs") && Host->Added[1].StyleName == FName(TEXT("Base"))));
		ASSERT_THAT(AreEqual(Action->GetRegisteredCount(), 2));
	}

	TEST_METHOD(Deactivating_TakesThemOutAgain_ByTheirTokens)
	{
		Action->Activate();
		Action->Deactivate();
		ASSERT_THAT(IsFalse(Action->IsActive()));
		ASSERT_THAT(AreEqual(Host->Removed.Num(), 2));
		ASSERT_THAT(IsTrue(Host->Removed.Contains(1) && Host->Removed.Contains(2)));
		ASSERT_THAT(AreEqual(Action->GetRegisteredCount(), 0));
	}

	TEST_METHOD(ActivatingTwice_HandsOverOnce)
	{
		Action->Activate();
		Action->Activate();
		Action->AddToWorld(Spawner.GetWorld());
		ASSERT_THAT(AreEqual(Host->Added.Num(), 2));
	}

	TEST_METHOD(ALayoutTheHostRefuses_IsNotCounted_AndNotRemoved)
	{
		Host->bRefuse = true;
		Action->Activate();
		ASSERT_THAT(AreEqual(Action->GetRegisteredCount(), 0));
		Action->Deactivate();
		ASSERT_THAT(AreEqual(Host->Removed.Num(), 0));
	}

	TEST_METHOD(WithNoHost_ItWarnsOnce_AndShowsNothing)
	{
		Slater::SetSlotHost(nullptr);
		TestRunner->AddExpectedMessagePlain(TEXT("no slot host is set"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Action->Activate();
		ASSERT_THAT(AreEqual(Action->GetRegisteredCount(), 0));
	}

	TEST_METHOD(AnEntryWithoutAValidIdOrSlot_IsSkippedWithAWarning)
	{
		FSlaterLayoutSlotEntry Bad;
		Bad.LayoutId = TEXT("../Nope");
		Bad.SlotID = Slot;
		FSlaterLayoutSlotEntry NoSlot;
		NoSlot.LayoutId = TEXT("Panel");
		FSlaterLayoutSlotEntry Good;
		Good.LayoutId = TEXT("Gallery");
		Good.SlotID = Slot;
		Action->SetLayouts({Bad, NoSlot, Good});
		TestRunner->AddExpectedMessagePlain(TEXT("Add Slater Layout: skipped"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
		Action->Activate();
		ASSERT_THAT(AreEqual(Host->Added.Num(), 1));
		ASSERT_THAT(IsTrue(Host->Added[0].LayoutId == TEXT("Gallery")));
	}

	TEST_METHOD(AHostCreatedTheEngineWay_ShowsAConfiguredLayout)
	{
		TStrongObjectPtr<USlaterLayoutUserWidget> Widget(Cast<USlaterLayoutUserWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterLayoutUserWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		Widget->Configure(TEXT("Panel"), TEXT("Dusk"));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsTrue(Widget->GetView().IsValid()));
		ASSERT_THAT(IsFalse(Widget->GetView()->HasErrors()));
		ASSERT_THAT(IsTrue(Widget->GetView()->GetLayoutId() == TEXT("Panel")));
	}

	TEST_METHOD(ACodeOwnedWidget_GetsLayoutScopeAndCommandsFromItsHooks)
	{
		Fix.Write(TEXT("CodeWidget.sltr"), TEXT("<vbox><text>{Count}</text><surface role=\"UI.Surface.Button\" on-click=\"Code.Add\"><text>add</text></surface></vbox>"));
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		const TSharedPtr<SSlaterView> View = Widget->GetView();
		ASSERT_THAT(IsTrue(View.IsValid()));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetLayoutId() == TEXT("CodeWidget")));
		ASSERT_THAT(IsTrue(View->GetStyleName() == FName(TEXT("Dusk"))));
		ASSERT_THAT(AreEqual(Widget->ScopeBuilds, 1));
		ASSERT_THAT(AreEqual(Widget->CommandRegistrations, 1));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("0"))));

		FSlaterCommandCall Call;
		Call.Name = TEXT("Code.Add");
		ASSERT_THAT(IsTrue(View->RunCommand(Call)));
		ASSERT_THAT(IsTrue(View->RunCommand(Call)));
		ASSERT_THAT(AreEqual(Widget->Count, 2));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("2"))));
	}

	TEST_METHOD(Notify_WithSeveralPaths_UpdatesEachBoundText)
	{
		Fix.Write(TEXT("CodeWidget.sltr"), TEXT("<vbox><text>{Count}</text><text>{Other}</text></vbox>"));
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsFalse(Widget->GetView()->HasErrors()));
		Widget->SetCountQuietly(3, false);
		const TArray<FString> Texts = SlaterBindKit::Texts(Slate);
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("3"))));
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("30"))));
	}

	TEST_METHOD(NotifyAll_MakesEveryBindingReadAgain)
	{
		Fix.Write(TEXT("CodeWidget.sltr"), TEXT("<vbox><text>{Count}</text><text>{Other}</text></vbox>"));
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		Widget->SetCountQuietly(4, true);
		const TArray<FString> Texts = SlaterBindKit::Texts(Slate);
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("4"))));
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("40"))));
	}

	TEST_METHOD(Notify_BeforeTheBuild_DoesNothing)
	{
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		Widget->SetCountQuietly(1, false);
		Widget->SetCountQuietly(2, true);
		ASSERT_THAT(IsFalse(Widget->GetView().IsValid()));
	}

	TEST_METHOD(Notify_AfterSetViewModel_IsSafe_AndTheViewModelStaysTheSource)
	{
		Fix.Write(TEXT("CodeWidget.sltr"), TEXT("<vbox><text>{Title}</text></vbox>"));
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("FromVm");
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		Widget->SetViewModel(Root);
		ASSERT_THAT(IsFalse(Widget->GetView()->HasErrors()));
		Widget->SetCountQuietly(5, false);
		Widget->SetCountQuietly(6, true);
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[0] == TEXT("FromVm")));
		Root->SetTitle(TEXT("Changed"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[0] == TEXT("Changed")));
	}

	TEST_METHOD(ACodeOwnedWidget_WithAMissingLayout_ShowsTheNormalBuildError)
	{
		TStrongObjectPtr<USlaterTestCodeWidget> Widget(Cast<USlaterTestCodeWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterTestCodeWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsTrue(Widget->GetView()->HasErrors()));
	}

	TEST_METHOD(TheUiPackTemplate_LoadsAsAPack_AndIsGoneWhenItsRootsAreRemoved)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlaterUI"));
		ASSERT_THAT(IsTrue(Plugin.IsValid()));
		const FString Content = Plugin->GetBaseDir() / TEXT("Templates/UIPack/Content/UI");
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*Content)));
		PackContent = Content;
		Slater::AddLayoutRoot(Content / TEXT("Layouts"));
		Slater::AddStyleRoot(Content / TEXT("Styles"));
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Slater::ReloadStyles(Errors)));

		TArray<FString> Ids;
		Slater::GetLayoutIds(Ids);
		ASSERT_THAT(IsTrue(Ids.Contains(TEXT("UIPack/Hud"))));
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("UIPack/Hud")).StyleName(TEXT("UIPack"));
			ASSERT_THAT(IsFalse(View->HasErrors()));
			ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View).Num() == 2));
		}

		Slater::RemoveLayoutRoot(Content / TEXT("Layouts"));
		Slater::RemoveStyleRoot(Content / TEXT("Styles"));
		ASSERT_THAT(IsTrue(Slater::ReloadStyles(Errors)));
		Ids.Reset();
		Slater::GetLayoutIds(Ids);
		ASSERT_THAT(IsFalse(Ids.Contains(TEXT("UIPack/Hud"))));
		ASSERT_THAT(IsFalse(Slater::StyleLibrary().Find(TEXT("UIPack")).IsValid()));
	}

	TEST_METHOD(TheUserWidgetHost_ShowsTheLayoutItIsConfiguredWith_AndOneConfiguredLater)
	{
		Fix.Write(TEXT("LwHost.sltr"), TEXT("<vbox><text>hosted</text></vbox>"));
		Fix.Write(TEXT("LwHost2.sltr"), TEXT("<vbox><text>second</text></vbox>"));
		TStrongObjectPtr<USlaterLayoutUserWidget> Widget(Cast<USlaterLayoutUserWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterLayoutUserWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		Widget->Configure(TEXT("LwHost"), TEXT("Base"));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("hosted"))));
		Widget->Configure(TEXT("LwHost2"), TEXT("Dusk"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate).Contains(TEXT("second"))));
		ASSERT_THAT(IsTrue(Widget->GetView()->GetStyleName() == FName(TEXT("Dusk"))));
	}
};

#endif