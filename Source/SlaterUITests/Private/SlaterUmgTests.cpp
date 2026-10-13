// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Engine.h"
#include "SlaterElements.h"
#include "SlaterUmg.h"
#include "SlaterUmgTestTypes.h"
#include "Widgets/SWidget.h"

namespace
{
	const TCHAR* const UmPath = TEXT("Layouts/Test.sltr");
	const TCHAR* const TestWidgetClass = TEXT("/Script/SlaterUITests.SlaterTestUserWidget");

	const FSlaterStyle& UmStyle()
	{
		static FSlaterStyle* Style = []()
		{
			FSlaterStyle* Result = new FSlaterStyle();
			TArray<FSlaterError> Errors;
			Slater::ParseStyle(TEXT(
				"<style name='T'><color name='UI.Color.Text' value='#101010'/><font name='UI.Font.Body' role='Regular' size='14'/></style>"),
				TEXT("Styles/T.sltrstyle"), *Result, Errors);
			return Result;
		}();
		return *Style;
	}

	FSlaterBuildResult UmBuild(const FString& Text, const FSlaterElementRegistry& Registry = Slater::Elements())
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, UmPath, UmStyle(), Result, FSlaterBudget(), Registry);
		return Result;
	}

	FString UmLayout(const TCHAR* Class)
	{
		return FString::Printf(TEXT("<vbox><umg class=\"%s\" height=\"20\"/></vbox>"), Class);
	}
}

/** The `<umg class>` host (T-346): a UserWidget made and owned by a SlaterUI subsystem, shown inside a layout. */
TEST_CLASS(SlaterUmgTests, "BRS.SlaterUI.Umg")
{
	FActorTestSpawner Spawner;
	USlaterUmgSubsystem* Subsystem = nullptr;

	BEFORE_EACH()
	{
		Subsystem = GEngine ? GEngine->GetEngineSubsystem<USlaterUmgSubsystem>() : nullptr;
		ASSERT_THAT(IsNotNull(Subsystem));
		Subsystem->SetWorldOverride(&Spawner.GetWorld());
		Subsystem->ReleaseUnused();
	}

	AFTER_EACH()
	{
		if (Subsystem)
		{
			Subsystem->SetWorldOverride(nullptr);
		}
	}

	TEST_METHOD(TheElementIsInTheStockRegistryOnceTheModuleLoaded)
	{
		ASSERT_THAT(IsNotNull(Slater::Elements().Find(TEXT("umg"))));
	}

	TEST_METHOD(AUmgHost_AppearsInTheTree_AndIsKeptBySubsystem)
	{
		const int32 Before = Subsystem->GetHostedCount();
		FSlaterBuildResult Result = UmBuild(UmLayout(TestWidgetClass));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Subsystem->GetHostedCount(), Before + 1));
		// A UserWidget shows as an SObjectWidget, sized by the common attributes of the element.
		bool bFound = false;
		TFunction<void(SWidget&)> Walk = [&](SWidget& Widget)
		{
			bFound |= Widget.GetType() == FName(TEXT("SObjectWidget"));
			Widget.GetChildren()->ForEachWidget(Walk);
		};
		Walk(*Result.Root);
		ASSERT_THAT(IsTrue(bFound));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsNear(Result.Root->GetDesiredSize().Y, 20.f, 0.01f));
	}

	TEST_METHOD(TheWidgetSurvivesAGarbageCollectionWhileItsTreeLives)
	{
		FSlaterBuildResult Result = UmBuild(UmLayout(TestWidgetClass));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const int32 Count = Subsystem->GetHostedCount();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		ASSERT_THAT(AreEqual(Subsystem->GetHostedCount(), Count));
		ASSERT_THAT(AreEqual(Subsystem->ReleaseUnused(), Count));
	}

	TEST_METHOD(TheWidgetIsReleasedWhenItsTreeIsGone)
	{
		const int32 Before = Subsystem->GetHostedCount();
		{
			FSlaterBuildResult Result = UmBuild(UmLayout(TestWidgetClass));
			ASSERT_THAT(IsTrue(Result.Succeeded()));
			ASSERT_THAT(AreEqual(Subsystem->GetHostedCount(), Before + 1));
		}
		ASSERT_THAT(AreEqual(Subsystem->ReleaseUnused(), Before));
	}

	TEST_METHOD(AnUnknownClass_FailsAtTheElementWithAHint)
	{
		const FSlaterBuildResult Result = UmBuild(UmLayout(TEXT("/Script/SlaterUITests.NoSuchWidget")));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 1));
		ASSERT_THAT(AreEqual(Result.Errors[0].Column, 7));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("was not found, or is not a UserWidget"))));
	}

	TEST_METHOD(AClassThatIsNotAUserWidget_IsRefused)
	{
		const FSlaterBuildResult Result = UmBuild(UmLayout(TEXT("/Script/Engine.Actor")));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("is not a UserWidget"))));
	}

	TEST_METHOD(AnAbstractClass_IsRefused)
	{
		const FSlaterBuildResult Result = UmBuild(UmLayout(TEXT("/Script/SlaterUITests.SlaterTestAbstractUserWidget")));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("is abstract"))));
	}

	TEST_METHOD(ABadClassPath_IsCaughtAtTheAttribute)
	{
		const FSlaterBuildResult Result = UmBuild(UmLayout(TEXT("SlaterTestUserWidget")));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("is not a class path"))));
		FSlaterBuildResult Missing = UmBuild(TEXT("<vbox><umg/></vbox>"));
		ASSERT_THAT(IsFalse(Missing.Succeeded()));
		ASSERT_THAT(IsTrue(Missing.Errors[0].Message.Contains(TEXT("needs the attribute 'class'"))));
	}

	TEST_METHOD(ARegistryWithoutTheElement_RefusesUmg)
	{
		// What a mod layer builds with (T-395): the same elements, minus `umg`.
		FSlaterElementRegistry Registry;
		Slater::RegisterLayoutElements(Registry);
		Slater::RegisterContentElements(Registry);
		const int32 Before = Subsystem->GetHostedCount();
		const FSlaterBuildResult Result = UmBuild(UmLayout(TestWidgetClass), Registry);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("unknown element <umg>"))));
		ASSERT_THAT(AreEqual(Subsystem->GetHostedCount(), Before));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
