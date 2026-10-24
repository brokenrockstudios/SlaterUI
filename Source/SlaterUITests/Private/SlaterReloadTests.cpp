// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlaterView.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace SlaterReloadTestsPrivate
{
	TSharedPtr<STextBlock> RlFindText(const TSharedRef<SWidget>& Root)
	{
		if (Root->GetType() == FName(TEXT("STextBlock")))
		{
			return StaticCastSharedRef<STextBlock>(Root);
		}
		TSharedPtr<STextBlock> Found;
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = RlFindText(Child.AsShared());
			}
		});
		return Found;
	}

	FString RlText(const TSharedRef<SSlaterView>& View)
	{
		const TSharedPtr<STextBlock> Block = RlFindText(View);
		return Block.IsValid() ? Block->GetText().ToString() : FString();
	}

	FLinearColor RlTextColor(const TSharedRef<SSlaterView>& View)
	{
		const TSharedPtr<STextBlock> Block = RlFindText(View);
		return Block.IsValid() ? Block->GetColorAndOpacity().GetSpecifiedColor() : FLinearColor::Transparent;
	}

	FString RlStyleFile(const TCHAR* Color)
	{
		return FString::Printf(TEXT("<style name=\"ReloadTest\" extends=\"Base\"><color name=\"UI.Color.Text\" value=\"%s\"/></style>"), Color);
	}
}
using namespace SlaterReloadTestsPrivate;

/** Live reload (T-344): what a changed file on disk does, without the folder watcher (that is editor-only; the tests call what it calls). */
TEST_CLASS(SlaterReloadTests, "BRS.SlaterUI.Reload")
{
	FString Dir;
	FString LayoutDir;
	FString StyleDir;

	BEFORE_EACH()
	{
		Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/ReloadTests");
		LayoutDir = Dir / TEXT("Layouts");
		StyleDir = Dir / TEXT("Styles");
		IFileManager::Get().MakeDirectory(*LayoutDir, true);
		IFileManager::Get().MakeDirectory(*StyleDir, true);
		Slater::AddLayoutRoot(LayoutDir);
		Slater::AddStyleRoot(StyleDir);
	}

	AFTER_EACH()
	{
		Slater::RemoveLayoutRoot(LayoutDir);
		Slater::RemoveStyleRoot(StyleDir);
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		// Drop the test style from the shared library again.
		TArray<FSlaterError> Errors;
		Slater::ReloadStyles(Errors);
	}

	void Write(const FString& Folder, const TCHAR* Name, const FString& Text)
	{
		FFileHelper::SaveStringToFile(Text, *(Folder / Name));
	}

	TEST_METHOD(ChangedLayout_IsShownAfterTheNotification)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>One</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("One"))));
		const int32 Builds = View->GetBuildCount();

		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Two</text>"));
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")})));

		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Two"))));
		ASSERT_THAT(AreEqual(View->GetBuildCount(), Builds + 1));
		ASSERT_THAT(IsFalse(View->HasReloadErrors()));
	}

	TEST_METHOD(BrokenSave_KeepsTheLastGoodTree_AndReportsTheErrors)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Good</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		const int32 Builds = View->GetBuildCount();

		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Half"));
		Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")});

		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		ASSERT_THAT(IsTrue(View->GetReloadErrors()[0].ToString().Contains(TEXT("ReloadMe.sltr"))));
		ASSERT_THAT(AreEqual(View->GetBuildCount(), Builds));
		// The good tree is still the one in the view (the banner is the other text block).
		ASSERT_THAT(IsTrue(View->GetResult().Succeeded()));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Good"))));

		// Fixing the file clears the banner and shows the new tree.
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Fixed</text>"));
		Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")});
		ASSERT_THAT(IsFalse(View->HasReloadErrors()));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Fixed"))));
	}

	TEST_METHOD(BrokenLayoutFromTheStart_IsFixedByASave)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<txet>x</txet>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		ASSERT_THAT(IsTrue(View->HasErrors()));

		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Now fine</text>"));
		Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")});
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Now fine"))));
	}

	TEST_METHOD(DeletedLayout_KeepsTheLastGoodTree)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Here</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		IFileManager::Get().Delete(*(LayoutDir / TEXT("ReloadMe.sltr")));
		Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")});
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		ASSERT_THAT(IsTrue(View->GetReloadErrors()[0].Message.Contains(TEXT("no such layout"))));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Here"))));
	}

	TEST_METHOD(OtherFiles_AreIgnored)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>One</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		const int32 Builds = View->GetBuildCount();
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Two</text>"));
		ASSERT_THAT(IsFalse(Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.mock.json"), LayoutDir / TEXT("Notes.txt")})));
		ASSERT_THAT(AreEqual(View->GetBuildCount(), Builds));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("One"))));
	}

	TEST_METHOD(ViewWithAutoRefreshOff_IsLeftAlone)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>One</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		View->SetAutoRefresh(false);
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>Two</text>"));
		Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")});
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("One"))));
		ASSERT_THAT(IsTrue(View->Refresh()));
		ASSERT_THAT(AreEqual(RlText(View), FString(TEXT("Two"))));
	}

	TEST_METHOD(DestroyedView_IsNotCalled)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>One</text>"));
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe"));
		}
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({LayoutDir / TEXT("ReloadMe.sltr")})));
	}

	TEST_METHOD(ChangedStyle_ReloadsTheLibrary_AndRecolorsTheView)
	{
		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("#FF0000")));
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Slater::ReloadStyles(Errors)));
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().Find(TEXT("ReloadTest")).IsValid()));

		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>x</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe")).StyleName(TEXT("ReloadTest"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const FLinearColor Red = RlTextColor(View);
		ASSERT_THAT(IsTrue(Red.R > 0.9f && Red.G < 0.1f));

		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("#00FF00")));
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({StyleDir / TEXT("ReloadTest.sltrstyle")})));
		const FLinearColor Green = RlTextColor(View);
		ASSERT_THAT(IsTrue(Green.G > 0.9f && Green.R < 0.1f));
	}

	TEST_METHOD(BrokenStyleSave_KeepsTheStylesInUse)
	{
		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("#FF0000")));
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Slater::ReloadStyles(Errors)));
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>x</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("ReloadMe")).StyleName(TEXT("ReloadTest"));
		const int32 Builds = View->GetBuildCount();

		TestRunner->AddExpectedMessagePlain(TEXT("Style reload refused"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, -1);
		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("not-a-color")));
		ASSERT_THAT(IsFalse(Slater::NotifyFilesChanged({StyleDir / TEXT("ReloadTest.sltrstyle")})));
		Errors.Reset();
		ASSERT_THAT(IsFalse(Slater::ReloadStyles(Errors)));
		ASSERT_THAT(IsTrue(Errors.Num() > 0));
		ASSERT_THAT(IsTrue(Errors[0].ToString().Contains(TEXT("ReloadTest.sltrstyle"))));

		// The old style is still the one in use, and the view rebuilt on it.
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().Find(TEXT("ReloadTest")).IsValid()));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsFalse(View->HasReloadErrors()));
		ASSERT_THAT(IsTrue(View->GetBuildCount() > Builds));
		ASSERT_THAT(IsTrue(RlTextColor(View).R > 0.9f));
	}

	TEST_METHOD(BrokenStyleSave_ExposesItsErrors_UntilTheNextGoodSave)
	{
		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("#FF0000")));
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Slater::ReloadStyles(Errors)));
		ASSERT_THAT(IsTrue(Slater::GetStyleReloadErrors().IsEmpty()));

		TestRunner->AddExpectedMessagePlain(TEXT("Style reload refused"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, -1);
		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("not-a-color")));
		bool bSeenInDelegate = false;
		const FDelegateHandle Handle = Slater::OnContentChanged().AddLambda([&bSeenInDelegate]()
		{
			bSeenInDelegate = Slater::GetStyleReloadErrors().Num() > 0;
		});
		ASSERT_THAT(IsFalse(Slater::NotifyFilesChanged({StyleDir / TEXT("ReloadTest.sltrstyle")})));
		Slater::OnContentChanged().Remove(Handle);
		ASSERT_THAT(IsTrue(bSeenInDelegate));
		ASSERT_THAT(IsTrue(Slater::GetStyleReloadErrors().Num() > 0));
		ASSERT_THAT(IsTrue(Slater::GetStyleReloadErrors()[0].ToString().Contains(TEXT("ReloadTest.sltrstyle"))));

		Write(StyleDir, TEXT("ReloadTest.sltrstyle"), RlStyleFile(TEXT("#00FF00")));
		ASSERT_THAT(IsTrue(Slater::NotifyFilesChanged({StyleDir / TEXT("ReloadTest.sltrstyle")})));
		ASSERT_THAT(IsTrue(Slater::GetStyleReloadErrors().IsEmpty()));
	}

	TEST_METHOD(LayoutIds_ListsTheRootsLayouts)
	{
		Write(LayoutDir, TEXT("ReloadMe.sltr"), TEXT("<text>x</text>"));
		IFileManager::Get().MakeDirectory(*(LayoutDir / TEXT("Sub")), true);
		Write(LayoutDir / TEXT("Sub"), TEXT("Deep.sltr"), TEXT("<text>x</text>"));
		TArray<FString> Ids;
		Slater::GetLayoutIds(Ids);
		ASSERT_THAT(IsTrue(Ids.Contains(TEXT("ReloadMe"))));
		ASSERT_THAT(IsTrue(Ids.Contains(TEXT("Sub/Deep"))));
		ASSERT_THAT(IsTrue(Ids.Contains(TEXT("Panel"))));
	}
};

#endif
