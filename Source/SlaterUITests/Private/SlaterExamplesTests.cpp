// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Modules/ModuleManager.h"
#include "SlaterBindTestKit.h"
#include "SlaterCommands.h"
#include "SlaterExamples.h"
#include "SlaterHealthHud.h"
#include "SlaterHelloWidget.h"
#include "SlaterListTestKit.h"
#include "SlaterMeter.h"
#include "SlaterObjectScope.h"
#include "SlaterView.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/** Whether the widget and every parent up to the root show: a `visible="false"` element collapses itself or its wrapper box. */
	bool IsShown(TSharedPtr<SWidget> Widget)
	{
		for (; Widget.IsValid(); Widget = Widget->GetParentWidget())
		{
			if (!Widget->GetVisibility().IsVisible())
			{
				return false;
			}
		}
		return true;
	}

	TSharedPtr<SWidget> FindText(const TSharedRef<SWidget>& Root, const TCHAR* Text)
	{
		TSharedPtr<SWidget> Found;
		if (Root->GetType() == FName(TEXT("STextBlock")) && SlaterBindKit::TextOf(Root) == Text)
		{
			return Root;
		}
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = FindText(Child.AsShared(), Text);
			}
		});
		return Found;
	}
}

/**
 * The worked examples (SlaterUIExamples, T-553): each one builds in both shipped styles with no errors, so a renamed token
 * or binding path, or a view model that stops matching its layout, fails here and not in front of a reader.
 */
TEST_CLASS(SlaterExamplesTests, "BRS.SlaterUI.Examples")
{
	BEFORE_EACH()
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("SlaterUIExamples"));
	}

	TEST_METHOD(TheRegistry_ListsOneNamePerExample_AndRefusesADuplicate)
	{
		FSlaterExampleRegistry& Registry = FSlaterExampleRegistry::Get();
		ASSERT_THAT(IsNotNull(Registry.Find(TEXT("Hello"))));

		FSlaterExample Copy = *Registry.Find(TEXT("Hello"));
		ASSERT_THAT(IsFalse(Registry.Register(Copy)));

		Copy.Name = TEXT("ExamplesTestOnly");
		ASSERT_THAT(IsTrue(Registry.Register(Copy)));
		ASSERT_THAT(IsNotNull(Registry.Find(TEXT("ExamplesTestOnly"))));
		ASSERT_THAT(IsTrue(Registry.Unregister(TEXT("ExamplesTestOnly"))));
		ASSERT_THAT(IsNull(Registry.Find(TEXT("ExamplesTestOnly"))));
		ASSERT_THAT(IsFalse(Registry.Register(FSlaterExample())));
	}

	TEST_METHOD(EveryRegisteredExample_BuildsWithNoErrors_InBothStyles)
	{
		TArray<FName> Names;
		FSlaterExampleRegistry::Get().GetNames(Names);
		ASSERT_THAT(IsTrue(Names.Num() >= 1));

		for (const FName Name : Names)
		{
			const FSlaterExample* Example = FSlaterExampleRegistry::Get().Find(Name);
			if (!Example->Create)
			{
				continue; // A widget example: HelloWidget_* below builds it.
			}
			for (const TCHAR* Style : { TEXT("Base"), TEXT("Dusk") })
			{
				const TSharedRef<SSlaterView> View = Example->Create(FName(Style));
				const FString Where = FString::Printf(TEXT("%s in %s"), *Name.ToString(), Style);
				ASSERT_THAT(IsFalse(View->HasErrors(), *Where));
				ASSERT_THAT(IsFalse(View->HasReloadErrors(), *Where));
				ASSERT_THAT(IsTrue(View->GetLayoutId() == Example->LayoutId, *Where));
				ASSERT_THAT(IsTrue(View->GetContext().IsValid(), *Where));
			}
		}
	}

	TEST_METHOD(EveryExampleLayout_AlsoBuildsFromItsMockFile_ForThePreviewTab)
	{
		TArray<FName> Names;
		FSlaterExampleRegistry::Get().GetNames(Names);
		for (const FName Name : Names)
		{
			const FSlaterExample* Example = FSlaterExampleRegistry::Get().Find(Name);
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(Example->LayoutId).UseMockContext(true);
			ASSERT_THAT(IsFalse(View->HasErrors(), *Name.ToString()));
		}
	}

	TEST_METHOD(Hello_TheCommandsMoveTheCount_AndTheGreetingFollowsIt)
	{
		const TSharedRef<SSlaterView> View = SlaterExamples::MakeHello(TEXT("Base"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		// Heading, greeting, "Pressed", count, "times", then the two button labels.
		TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 7));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Press the button.")));
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("0")));

		FSlaterCommandCall Add;
		Add.Name = TEXT("Hello.Add");
		ASSERT_THAT(IsTrue(View->RunCommand(Add)));
		ASSERT_THAT(IsTrue(View->RunCommand(Add)));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("2")));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Keep going.")));

		for (int32 Press = 0; Press < 8; ++Press)
		{
			View->RunCommand(Add);
		}
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("10")));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("That is plenty.")));

		FSlaterCommandCall Reset;
		Reset.Name = TEXT("Hello.Reset");
		ASSERT_THAT(IsTrue(View->RunCommand(Reset)));
		Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("0")));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Press the button.")));
	}

	TEST_METHOD(Hello_TwoViews_KeepTheirOwnCounts)
	{
		const TSharedRef<SSlaterView> First = SlaterExamples::MakeHello(TEXT("Base"));
		const TSharedRef<SSlaterView> Second = SlaterExamples::MakeHello(TEXT("Base"));
		FSlaterCommandCall Add;
		Add.Name = TEXT("Hello.Add");
		First->RunCommand(Add);
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(First)[3] == TEXT("1")));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Second)[3] == TEXT("0")));
	}

	TEST_METHOD(MvvmViewModelBase_PushesChangesThroughMakeObjectScope)
	{
		// The T-554 question: does a UMVVMViewModelBase subclass, with no Slater-specific code, work as a scope?
		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel(NewObject<USlaterHealthHudViewModel>(GetTransientPackage()));
		const TSharedRef<ISlaterScope> Scope = Slater::MakeObjectScope(ViewModel.Get());

		// Read appends to its output, so each read starts from an empty string.
		FString Text;
		const auto Read = [&](const TCHAR* Path)
		{
			Text.Reset();
			return Scope->Read(Path, Text);
		};
		for (const TCHAR* Path : { TEXT("PlayerName"), TEXT("Health"), TEXT("MaxHealth"), TEXT("HealthFraction"), TEXT("IsLow") })
		{
			FString Why;
			ASSERT_THAT(IsTrue(Scope->Check(Path, false, Why), *FString::Printf(TEXT("%s: %s"), Path, *Why)));
		}
		ASSERT_THAT(IsTrue(Read(TEXT("Health")), TEXT("read Health")));
		ASSERT_THAT(IsTrue(Text == TEXT("100"), *Text));
		ASSERT_THAT(IsTrue(Read(TEXT("IsLow")), TEXT("read IsLow")));
		ASSERT_THAT(IsTrue(Text == TEXT("false"), *Text));

		int32 HealthPushes = 0;
		int32 FractionPushes = 0;
		int32 LowPushes = 0;
		TUniquePtr<ISlaterSubscription> OnHealth = Scope->Subscribe(TEXT("Health"), [&HealthPushes]() { ++HealthPushes; });
		TUniquePtr<ISlaterSubscription> OnFraction = Scope->Subscribe(TEXT("HealthFraction"), [&FractionPushes]() { ++FractionPushes; });
		TUniquePtr<ISlaterSubscription> OnLow = Scope->Subscribe(TEXT("IsLow"), [&LowPushes]() { ++LowPushes; });

		ViewModel->SetHealth(20);
		ASSERT_THAT(AreEqual(HealthPushes, 1));
		ASSERT_THAT(AreEqual(FractionPushes, 1)); // a UFUNCTION(FieldNotify) getter is a field of its own
		ASSERT_THAT(AreEqual(LowPushes, 1));
		ASSERT_THAT(IsTrue(Read(TEXT("Health"))));
		ASSERT_THAT(IsTrue(Text == TEXT("20"), *Text));
		ASSERT_THAT(IsTrue(Read(TEXT("IsLow"))));
		ASSERT_THAT(IsTrue(Text == TEXT("true")));

		ViewModel->SetHealth(20); // UE_MVVM_SET_PROPERTY_VALUE broadcasts only when the value changed
		ASSERT_THAT(AreEqual(HealthPushes, 1));

		OnHealth.Reset();
		ViewModel->SetHealth(30);
		ASSERT_THAT(AreEqual(HealthPushes, 1)); // dropping the subscription removed the delegate
		ASSERT_THAT(AreEqual(FractionPushes, 2));
	}

	TEST_METHOD(HealthHud_IsRegistered_AndShowsTheViewModel)
	{
		ASSERT_THAT(IsNotNull(FSlaterExampleRegistry::Get().Find(TEXT("HealthHud"))));

		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel(NewObject<USlaterHealthHudViewModel>(GetTransientPackage()));
		ViewModel->SetPlayerName(TEXT("Wren"));
		const TSharedRef<SSlaterView> View = SlaterExamples::MakeHealthHud(TEXT("Base"), ViewModel.Get());
		ASSERT_THAT(IsFalse(View->HasErrors()));
		// Name, health, "of", max, warning, then the two button labels.
		const TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 7));
		ASSERT_THAT(IsTrue(Texts[0] == TEXT("Wren")));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("100")));
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("100")));
	}

	TEST_METHOD(HealthHud_TheMeterAndTextsFollowTheViewModel)
	{
		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel(NewObject<USlaterHealthHudViewModel>(GetTransientPackage()));
		const TSharedRef<SSlaterView> View = SlaterExamples::MakeHealthHud(TEXT("Base"), ViewModel.Get());
		const TSharedPtr<SSlaterMeter> Meter = StaticCastSharedPtr<SSlaterMeter>(SlaterListKit::Find(View, TEXT("SSlaterMeter")));
		ASSERT_THAT(IsTrue(Meter.IsValid()));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 1.f)));

		ViewModel->SetHealth(50);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 0.5f)));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[1] == TEXT("50")));

		ViewModel->SetMaxHealth(200);
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Meter->GetValue(), 0.25f)));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[3] == TEXT("200")));
	}

	TEST_METHOD(HealthHud_LowHealth_ShowsTheWarning_AndRecoveryHidesIt)
	{
		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel(NewObject<USlaterHealthHudViewModel>(GetTransientPackage()));
		const TSharedRef<SSlaterView> View = SlaterExamples::MakeHealthHud(TEXT("Base"), ViewModel.Get());
		const TSharedPtr<SWidget> Warning = FindText(View, TEXT("Low health!"));
		ASSERT_THAT(IsTrue(Warning.IsValid()));
		ASSERT_THAT(IsFalse(IsShown(Warning)));

		ViewModel->SetHealth(20);
		ASSERT_THAT(IsTrue(IsShown(Warning)));

		ViewModel->SetHealth(80);
		ASSERT_THAT(IsFalse(IsShown(Warning)));
	}

	TEST_METHOD(HealthHud_TheCommandsTakeTheirArgument_AndTheTimerStepWalksTheHealthDown)
	{
		TStrongObjectPtr<USlaterHealthHudViewModel> ViewModel(NewObject<USlaterHealthHudViewModel>(GetTransientPackage()));
		const TSharedRef<SSlaterView> View = SlaterExamples::MakeHealthHud(TEXT("Base"), ViewModel.Get());

		FSlaterCommandCall Hurt;
		Hurt.Name = TEXT("HealthHud.Hurt");
		Hurt.Argument = TEXT("30");
		ASSERT_THAT(IsTrue(View->RunCommand(Hurt)));
		ASSERT_THAT(AreEqual(ViewModel->GetHealth(), 70));

		FSlaterCommandCall Heal;
		Heal.Name = TEXT("HealthHud.Heal");
		Heal.Argument = TEXT("500");
		ASSERT_THAT(IsTrue(View->RunCommand(Heal)));
		ASSERT_THAT(AreEqual(ViewModel->GetHealth(), 100)); // clamped to the maximum

		ViewModel->Advance();
		ASSERT_THAT(AreEqual(ViewModel->GetHealth(), 97));
		ViewModel->SetHealth(0);
		ViewModel->Advance();
		ASSERT_THAT(AreEqual(ViewModel->GetHealth(), 4)); // empty: the timer turns around and heals
	}

	TEST_METHOD(HelloWidget_IsRegistered_AsAWidgetExample)
	{
		const FSlaterExample* Example = FSlaterExampleRegistry::Get().Find(TEXT("HelloWidget"));
		ASSERT_THAT(IsNotNull(Example));
		ASSERT_THAT(IsTrue(Example->LayoutId == TEXT("Examples/Hello")));
		ASSERT_THAT(IsTrue(static_cast<bool>(Example->Show)));
		ASSERT_THAT(IsFalse(static_cast<bool>(Example->Create)));
	}

	TEST_METHOD(HelloWidget_CreatedLikeAnyWidget_CountsTwoPresses)
	{
		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		TStrongObjectPtr<USlaterHelloWidget> Widget(Cast<USlaterHelloWidget>(UUserWidget::CreateWidgetInstance(Spawner.GetWorld(), USlaterHelloWidget::StaticClass(), NAME_None)));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		const TSharedRef<SWidget> Slate = Widget->TakeWidget();
		const TSharedPtr<SSlaterView> View = Widget->GetView();
		ASSERT_THAT(IsTrue(View.IsValid()));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[3] == TEXT("0")));

		FSlaterCommandCall Add;
		Add.Name = TEXT("Hello.Add");
		ASSERT_THAT(IsTrue(View->RunCommand(Add)));
		ASSERT_THAT(IsTrue(View->RunCommand(Add)));
		ASSERT_THAT(AreEqual(Widget->GetCount(), 2));
		TArray<FString> Texts = SlaterBindKit::Texts(Slate);
		ASSERT_THAT(IsTrue(Texts[3] == TEXT("2")));
		ASSERT_THAT(IsTrue(Texts[1] == TEXT("Keep going.")));

		FSlaterCommandCall Reset;
		Reset.Name = TEXT("Hello.Reset");
		ASSERT_THAT(IsTrue(View->RunCommand(Reset)));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(Slate)[3] == TEXT("0")));
	}
};

#endif
