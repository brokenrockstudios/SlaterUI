// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlaterElements.h"
#include "SlaterTemplates.h"
#include "SlaterView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const TCHAR* const TpPath = TEXT("Layouts/Test.sltr");

	const FSlaterStyle& TpStyle()
	{
		static FSlaterStyle* Style = []()
		{
			FSlaterStyle* Result = new FSlaterStyle();
			TArray<FSlaterError> Errors;
			Slater::ParseStyle(TEXT(
				"<style name='T'>"
				"<color name='UI.Color.Panel' value='#FFFFFF'/><color name='UI.Color.Text' value='#101010'/>"
				"<metric name='UI.Space.S' value='4'/>"
				"<font name='UI.Font.Body' role='Regular' size='14'/>"
				"<surface name='UI.Surface.Panel' fill='UI.Color.Panel'/>"
				"</style>"), TEXT("Styles/T.sltrstyle"), *Result, Errors);
			return Result;
		}();
		return *Style;
	}

	FSlaterBuildResult TpBuild(const TCHAR* Text, const FSlaterElementRegistry& Registry = Slater::Elements())
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, TpPath, TpStyle(), Result, FSlaterBudget(), Registry);
		return Result;
	}

	/** Every STextBlock under Root, in tree order. */
	void TpCollectTexts(const TSharedRef<SWidget>& Root, TArray<FString>& OutTexts)
	{
		if (Root->GetType() == FName(TEXT("STextBlock")))
		{
			OutTexts.Add(StaticCastSharedRef<STextBlock>(Root)->GetText().ToString());
		}
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			TpCollectTexts(Child.AsShared(), OutTexts);
		});
	}

	FString TpJoinTexts(const FSlaterBuildResult& Result)
	{
		TArray<FString> Texts;
		TpCollectTexts(Result.Root.ToSharedRef(), Texts);
		return FString::Join(Texts, TEXT("|"));
	}

	/** Empty when Result has an error at (Line, Column), under Path, whose message holds Fragment; else says what it did have. */
	FString TpCheckError(const TArray<FSlaterError>& Errors, const TCHAR* Path, int32 Line, int32 Column, const TCHAR* Fragment)
	{
		FString All;
		for (const FSlaterError& Error : Errors)
		{
			if (Error.Path == Path && Error.Line == Line && Error.Column == Column && Error.Message.Contains(Fragment, ESearchCase::CaseSensitive))
			{
				return FString();
			}
			All += Error.ToString() + TEXT(" | ");
		}
		return FString::Printf(TEXT("no error at %s(%d,%d) containing '%s'; got: %s"), Path, Line, Column, Fragment, *All);
	}

	/** Parses Text, expands it with an in-memory import resolver, and returns the errors (empty when it worked). */
	TArray<FSlaterError> TpExpand(const TCHAR* Text, const TMap<FString, FString>& Files, FSlaterNode& OutRoot)
	{
		TArray<FSlaterError> Errors;
		FSlaterError ParseError;
		if (!Slater::ParseMarkup(Text, TpPath, OutRoot, ParseError))
		{
			Errors.Add(ParseError);
			return Errors;
		}
		const FSlaterImportResolver Resolver = [&Files](const FString& Id, FString& OutText, FString& OutLabel, FString& OutProblem) -> bool
		{
			OutLabel = FString::Printf(TEXT("Layouts/%s.sltr"), *Id);
			if (const FString* Found = Files.Find(Id))
			{
				OutText = *Found;
				return true;
			}
			OutProblem = TEXT("no such file");
			return false;
		};
		Slater::ExpandTemplates(OutRoot, TpPath, Slater::Elements(), Resolver, Errors);
		return Errors;
	}

	/** A host element with two template slots, standing in for `<inventory-grid>`. It instantiates each slot's template once and records what it saw. */
	struct FTpHostLog
	{
		int32 Calls = 0;
		TArray<FName> SlotsSeen;
		int32 InstancesBuilt = 0;
		bool bInstancesAreText = false;
	};

	FTpHostLog GTpHostLog;

	FSlaterElementRegistry TpRegistryWithHost(bool bCellRequired)
	{
		FSlaterElementRegistry Registry;
		Slater::RegisterLayoutElements(Registry);
		Slater::RegisterContentElements(Registry);
		Slater::RegisterListElements(Registry);

		FSlaterElementDesc Host;
		Host.Tag = TEXT("test-grid");
		Host.Purpose = TEXT("a test host with template slots");
		Host.Children = ESlaterChildRule::None;
		Host.TemplateSlots = {{FName(TEXT("cell")), bCellRequired}, {FName(TEXT("item")), false}};
		Host.Builder = [](FSlaterBuildContext& Context) -> TSharedPtr<SWidget>
		{
			++GTpHostLog.Calls;
			TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
			for (const TPair<FName, TSharedPtr<const FSlaterCompiledTemplate>>& Pair : Context.SlotTemplates)
			{
				GTpHostLog.SlotsSeen.Add(Pair.Key);
				FSlaterBuildResult Instance;
				if (Slater::InstantiateTemplate(*Pair.Value, *Context.Style, *Context.Registry, Instance))
				{
					++GTpHostLog.InstancesBuilt;
					GTpHostLog.bInstancesAreText = Instance.Root->GetType() == FName(TEXT("STextBlock"));
					Box->AddSlot()[Instance.Root.ToSharedRef()];
				}
			}
			return Box;
		};
		FString Error;
		Registry.Register(MoveTemp(Host), &Error);
		return Registry;
	}
}

/** Templates (T-346): declared in the layout, used like elements, expanded before the layout is checked. */
TEST_CLASS(SlaterTemplateTests, "BRS.SlaterUI.Templates")
{
	TEST_METHOD(UsedTwiceWithTwoParameters_BuildsTwoDifferentRows)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>"
			"<template name='stat-row' params='label value'><hbox><text>$label</text><text>$value</text></hbox></template>"
			"<stat-row label='Health' value='{Health}'/>"
			"<stat-row label='Armor' value='42'/>"
			"</vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 2));
		// The binding is kept as a binding: the first row's value text is bound, not literal.
		ASSERT_THAT(AreEqual(Result.Bindings.Num(), 1));
		ASSERT_THAT(AreEqual(Result.Bindings[0].Spec->Path, FString(TEXT("Health"))));
		TArray<FString> Texts;
		TpCollectTexts(Result.Root.ToSharedRef(), Texts);
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("Health"))));
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("Armor"))));
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("42"))));
	}

	TEST_METHOD(ParameterInAnAttribute_IsSubstituted)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox><template name='gap' params='size'><spacer height='$size'/></template><gap size='UI.Space.S'/><gap size='10'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<SWidget> Root = Result.Root.ToSharedRef();
		Root->SlatePrepass(1.f);
		ASSERT_THAT(IsNear(Root->GetDesiredSize().Y, 14.f, 0.01f));
	}

	TEST_METHOD(UseCarriesSlotAttributesAndId_ToTheBodyRoot)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox><template name='row' params='label'><text>$label</text></template><row id='Top' label='A' margin='3'/><row label='B' fill='2'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.ById.Contains(TEXT("Top"))));
		const SBoxPanel::FSlot& Second = static_cast<const SBoxPanel::FSlot&>(Result.Root->GetChildren()->GetSlotAt(1));
		ASSERT_THAT(IsTrue(Second.GetSizeRule() == FSizeParam::SizeRule_Stretch));
		ASSERT_THAT(IsTrue(Second.GetSizeValue() == 2.f));
	}

	TEST_METHOD(TemplatesMayUseTemplates)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>"
			"<template name='tag' params='t'><text>[$t]</text></template>"
			"<template name='pair' params='a b'><hbox><tag t='$a'/><tag t='$b'/></hbox></template>"
			"<pair a='x' b='y'/>"
			"</vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpJoinTexts(Result), FString(TEXT("[x]|[y]"))));
	}

	TEST_METHOD(ADollarSign_IsWrittenTwice)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox><template name='price' params='n'><text>$$$n</text></template><price n='5'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpJoinTexts(Result), FString(TEXT("$5"))));
	}

	TEST_METHOD(AnArgumentIsNotScannedAgain)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox><template name='say' params='t'><text>$t</text></template><say t='$t'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpJoinTexts(Result), FString(TEXT("$t"))));
	}

	TEST_METHOD(UnknownParameterOnAUse_FailsAtItsLine)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"\t<template name='stat-row' params='label value'><text>$label</text></template>\n"
			"\t<stat-row label='Health' vlaue='3'/>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		// Line 3, the column of `vlaue`, with the nearest parameter suggested.
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 27, TEXT("unknown parameter 'vlaue' on <stat-row>; did you mean 'value'?")), FString()));
	}

	TEST_METHOD(MissingParameter_FailsAtTheUse)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='stat-row' params='label value'><text>$label</text></template>\n"
			"<stat-row label='Health'/>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 1, TEXT("<stat-row> needs the attribute 'value'")), FString()));
	}

	TEST_METHOD(UnknownDollarNameInABody_FailsAtTheDefinition)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='row' params='label'>\n"
			"<text font='$fnt'>$label</text>\n"
			"</template>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 7, TEXT("unknown parameter '$fnt'; the parameters are label")), FString()));
	}

	TEST_METHOD(AWrongValue_IsReportedWhereTheUseWroteIt)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='note' params='col'><text color='$col'>x</text></template>\n"
			"<note col='UI.Color.Nope'/>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 7, TEXT("unknown color token 'UI.Color.Nope'")), FString()));
	}

	TEST_METHOD(ATemplateThatUsesItself_IsRefused)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='a'><vbox><b/></vbox></template>\n"
			"<template name='b'><vbox><a/></vbox></template>\n"
			"<a/>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 26, TEXT("template 'a' uses itself: a -> b -> a")), FString()));
	}

	TEST_METHOD(ANameThatIsABuiltInElementOrTaken_IsRefused)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='text'><spacer/></template>\n"
			"<template name='mine'><spacer/></template>\n"
			"<template name='mine'><spacer/></template>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 11, TEXT("'text' is a built-in element")), FString()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 4, 11, TEXT("template 'mine' is already defined (at 3,1)")), FString()));
	}

	TEST_METHOD(ABadDeclaration_IsReportedAndStopsTheExpansion)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='box2' title='x'><spacer/><spacer/></template>\n"
			"<template name='row'><spacer/></template>\n"
			"<row/>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 23, TEXT("unknown attribute 'title' on <template>")), FString()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 1, TEXT("needs exactly one child element")), FString()));
	}

	TEST_METHOD(AUseTakesNoChildren)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox>\n"
			"<template name='row'><spacer/></template>\n"
			"<row><spacer/></row>\n"
			"</vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 6, TEXT("<row> is a template and takes no child elements")), FString()));
	}

	TEST_METHOD(ADeclarationBelowTheRoot_IsAnUnknownElement)
	{
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox><hbox><template name='x'><spacer/></template></hbox></vbox>"));
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("unknown element <template>"))));
	}

	TEST_METHOD(NoDeclarations_LeavesTheLayoutAlone)
	{
		FSlaterNode Root;
		FSlaterError ParseError;
		ASSERT_THAT(IsTrue(Slater::ParseMarkup(TEXT("<vbox><spacer/></vbox>"), TpPath, Root, ParseError)));
		ASSERT_THAT(IsFalse(Slater::HasTemplateDeclarations(Root)));
	}
};

/** Imports (T-346): a file whose root is `<templates>` shares templates; found through the layout folders. */
TEST_CLASS(SlaterImportTests, "BRS.SlaterUI.Imports")
{
	TEST_METHOD(AnImportedTemplate_IsUsedLikeALocalOne)
	{
		TMap<FString, FString> Files;
		Files.Add(TEXT("Common"), TEXT("<templates><template name='pill' params='label'><text>($label)</text></template></templates>"));
		FSlaterNode Root;
		const TArray<FSlaterError> Errors = TpExpand(TEXT("<vbox><import src='Common'/><pill label='A'/><pill label='B'/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(Errors.Num(), 0));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 2));
		ASSERT_THAT(AreEqual(Root.Children[0].Tag, FString(TEXT("text"))));
		ASSERT_THAT(AreEqual(Root.Children[1].Text, FString(TEXT("(B)"))));
	}

	TEST_METHOD(ImportsNest_AndAFileImportedTwiceIsReadOnce)
	{
		TMap<FString, FString> Files;
		Files.Add(TEXT("A"), TEXT("<templates><import src='B'/><template name='one'><spacer/></template></templates>"));
		Files.Add(TEXT("B"), TEXT("<templates><template name='two'><spacer/></template></templates>"));
		FSlaterNode Root;
		const TArray<FSlaterError> Errors = TpExpand(TEXT("<vbox><import src='A'/><import src='B'/><one/><two/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(Errors.Num(), 0));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 2));
	}

	TEST_METHOD(AnImportCycle_IsRefused)
	{
		TMap<FString, FString> Files;
		Files.Add(TEXT("A"), TEXT("<templates><import src='B'/></templates>"));
		Files.Add(TEXT("B"), TEXT("<templates>\n<import src='A'/></templates>"));
		FSlaterNode Root;
		const TArray<FSlaterError> Errors = TpExpand(TEXT("<vbox><import src='A'/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(TpCheckError(Errors, TEXT("Layouts/B.sltr"), 2, 9, TEXT("import cycle: A -> B -> A")), FString()));
	}

	TEST_METHOD(AMissingImport_FailsAtItsSrc)
	{
		FSlaterNode Root;
		const TArray<FSlaterError> Errors = TpExpand(TEXT("<vbox>\n<import src='Nope'/></vbox>"), TMap<FString, FString>(), Root);
		ASSERT_THAT(AreEqual(TpCheckError(Errors, TpPath, 2, 9, TEXT("cannot import 'Nope': no such file")), FString()));
	}

	TEST_METHOD(AnImportedFileHoldsOnlyTemplates)
	{
		TMap<FString, FString> Files;
		Files.Add(TEXT("Bad"), TEXT("<templates>\n<spacer/></templates>"));
		Files.Add(TEXT("Wrong"), TEXT("<vbox/>"));
		FSlaterNode Root;
		TArray<FSlaterError> Errors = TpExpand(TEXT("<vbox><import src='Bad'/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(TpCheckError(Errors, TEXT("Layouts/Bad.sltr"), 2, 1, TEXT("may hold only <template> and <import>")), FString()));
		Errors = TpExpand(TEXT("<vbox><import src='Wrong'/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(TpCheckError(Errors, TEXT("Layouts/Wrong.sltr"), 1, 1, TEXT("must have <templates> as its root")), FString()));
	}

	TEST_METHOD(AnErrorInsideAnImportedBody_IsReportedUnderThatFile)
	{
		TMap<FString, FString> Files;
		Files.Add(TEXT("Common"), TEXT("<templates>\n<template name='pill'>\n<text color='UI.Color.Nope'>x</text>\n</template></templates>"));
		FSlaterNode Root;
		const TArray<FSlaterError> ExpandErrors = TpExpand(TEXT("<vbox><import src='Common'/><pill/></vbox>"), Files, Root);
		ASSERT_THAT(AreEqual(ExpandErrors.Num(), 0));
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::BuildLayout(Root, TpPath, TpStyle(), Result)));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TEXT("Layouts/Common.sltr"), 3, 7, TEXT("unknown color token 'UI.Color.Nope'")), FString()));
	}
};

/** Imports through the layout folders, the way a view finds them. */
TEST_CLASS(SlaterImportFileTests, "BRS.SlaterUI.ImportFile")
{
	FString Dir;

	BEFORE_EACH()
	{
		Dir = FPaths::ProjectSavedDir() / TEXT("SlaterUI/TemplateTests/Layouts");
		IFileManager::Get().MakeDirectory(*Dir, true);
		Slater::AddLayoutRoot(Dir);
	}

	AFTER_EACH()
	{
		Slater::RemoveLayoutRoot(Dir);
		IFileManager::Get().DeleteDirectory(*(FPaths::ProjectSavedDir() / TEXT("SlaterUI/TemplateTests")), false, true);
	}

	TEST_METHOD(AViewShowsALayoutThatImports_AndFollowsAChangeToTheImport)
	{
		FFileHelper::SaveStringToFile(TEXT("<templates><template name='pill' params='label'><text>($label)</text></template></templates>"), *(Dir / TEXT("TplCommon.sltr")));
		FFileHelper::SaveStringToFile(TEXT("<vbox><import src='TplCommon'/><pill label='A'/></vbox>"), *(Dir / TEXT("TplScreen.sltr")));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TplScreen"));
		View->SetAutoRefresh(false);
		ASSERT_THAT(IsFalse(View->HasErrors()));
		TArray<FString> Texts;
		TpCollectTexts(View->GetResult().Root.ToSharedRef(), Texts);
		ASSERT_THAT(AreEqual(FString::Join(Texts, TEXT("|")), FString(TEXT("(A)"))));

		// Only the imported file changes; the watcher reports it like any layout file.
		FFileHelper::SaveStringToFile(TEXT("<templates><template name='pill' params='label'><text>&lt;$label&gt;</text></template></templates>"), *(Dir / TEXT("TplCommon.sltr")));
		ASSERT_THAT(IsTrue(View->Refresh()));
		Texts.Reset();
		TpCollectTexts(View->GetResult().Root.ToSharedRef(), Texts);
		ASSERT_THAT(AreEqual(FString::Join(Texts, TEXT("|")), FString(TEXT("<A>"))));
	}

	TEST_METHOD(ABrokenImport_ShowsItsFileAndLine)
	{
		FFileHelper::SaveStringToFile(TEXT("<templates>\n<template name='pill'>\n<text color='UI.Color.Nope'>x</text></template></templates>"), *(Dir / TEXT("TplCommon.sltr")));
		FFileHelper::SaveStringToFile(TEXT("<vbox><import src='TplCommon'/><pill/></vbox>"), *(Dir / TEXT("TplScreen.sltr")));
		FSlaterBuildResult Result;
		ASSERT_THAT(IsFalse(Slater::LoadLayout(TEXT("TplScreen"), TpStyle(), Result)));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TEXT("UI/Layouts/TplCommon.sltr"), 3, 7, TEXT("unknown color token")), FString()));
	}
};

/** Elements with template slots (T-346): the host side of `<inventory-grid>`. */
TEST_CLASS(SlaterTemplateSlotTests, "BRS.SlaterUI.TemplateSlots")
{
	BEFORE_EACH()
	{
		GTpHostLog = FTpHostLog();
	}

	TEST_METHOD(ARegisteredElementWithSlots_GetsItsTemplates)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(true);
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox><test-grid><cell><surface role='UI.Surface.Panel'/></cell><item><text>x</text></item></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(GTpHostLog.Calls, 1));
		ASSERT_THAT(AreEqual(GTpHostLog.SlotsSeen.Num(), 2));
		ASSERT_THAT(IsTrue(GTpHostLog.SlotsSeen.Contains(FName(TEXT("cell")))));
		ASSERT_THAT(IsTrue(GTpHostLog.SlotsSeen.Contains(FName(TEXT("item")))));
		ASSERT_THAT(AreEqual(GTpHostLog.InstancesBuilt, 2));
		// The slot templates are not in the tree the layout built: only the host's own widgets are.
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 1));
	}

	TEST_METHOD(AnOptionalSlotMayBeLeftOut)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(true);
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox><test-grid><cell><spacer/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(GTpHostLog.SlotsSeen.Num(), 1));
	}

	TEST_METHOD(ARequiredSlotMissing_FailsAtTheHost)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(true);
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox>\n<test-grid><item><spacer/></item></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 1, TEXT("<test-grid> needs a <cell> child")), FString()));
		ASSERT_THAT(AreEqual(GTpHostLog.Calls, 0));
	}

	TEST_METHOD(AnUnknownSlotElement_SuggestsTheNearest)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(false);
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox>\n<test-grid>\n<cel><spacer/></cel></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 3, 1, TEXT("<test-grid> has no slot <cel>; did you mean 'cell'?")), FString()));
	}

	TEST_METHOD(ASlotHoldsExactlyOneElement_AndEachSlotOnce)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(false);
		FSlaterBuildResult Result = TpBuild(TEXT("<vbox>\n<test-grid><cell><spacer/><spacer/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 12, TEXT("<cell> needs exactly one child element")), FString()));
		Result = TpBuild(TEXT("<vbox>\n<test-grid><cell><spacer/></cell><cell><spacer/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(AreEqual(TpCheckError(Result.Errors, TpPath, 2, 34, TEXT("<test-grid> has <cell> twice")), FString()));
	}

	TEST_METHOD(ASlotTemplateIsCheckedLikeARepeatChild)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(false);
		FSlaterBuildResult Result = TpBuild(TEXT("<vbox><test-grid><cell><spacer id='Gap'/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("'id' is not allowed inside a template"))));
		Result = TpBuild(TEXT("<vbox><test-grid><cell><surface role='UI.Surface.Nope'/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("unknown surface token"))));
		ASSERT_THAT(AreEqual(GTpHostLog.Calls, 0));
	}

	TEST_METHOD(ASlotBindingBelongsToTheHost_NotTheLayout)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(false);
		const FSlaterBuildResult Result = TpBuild(TEXT("<vbox><test-grid><item><text>{Name}</text></item></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Bindings.Num(), 0));
		ASSERT_THAT(AreEqual(GTpHostLog.InstancesBuilt, 1));
	}

	TEST_METHOD(ATemplateMayBeUsedInASlot_EvenOneNamedLikeTheSlot)
	{
		const FSlaterElementRegistry Registry = TpRegistryWithHost(false);
		const FSlaterBuildResult Result = TpBuild(TEXT(
			"<vbox><template name='cell' params='t'><text>$t</text></template>"
			"<test-grid><cell><cell t='in slot'/></cell></test-grid></vbox>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(GTpHostLog.InstancesBuilt, 1));
		ASSERT_THAT(IsTrue(GTpHostLog.bInstancesAreText));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
