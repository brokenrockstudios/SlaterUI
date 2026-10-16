// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Modules/ModuleManager.h"
#include "SlaterBindTestKit.h"
#include "SlaterCommands.h"
#include "SlaterExamples.h"
#include "SlaterHelloWidget.h"
#include "SlaterView.h"

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
		ASSERT_THAT(AreEqual(Widget->Count, 2));
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
