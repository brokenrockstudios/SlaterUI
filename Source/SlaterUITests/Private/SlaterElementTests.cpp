// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterElements.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SWidget.h"

namespace
{
	const TCHAR* const ElPath = TEXT("Layouts/Test.sltr");

	/** Tokens the layouts below use: UI.Space.S/M, UI.Color.Panel/Accent, UI.Font.Body, UI.Surface.Panel. */
	const FSlaterStyle& ElStyle()
	{
		static FSlaterStyle* Style = []()
		{
			FSlaterStyle* Result = new FSlaterStyle();
			TArray<FSlaterError> Errors;
			Slater::ParseStyle(TEXT(
				"<style name='T'>"
				"<color name='UI.Color.Panel' value='#FFFFFF'/><color name='UI.Color.Accent' value='#2F6FEB'/>"
				"<metric name='UI.Space.S' value='4'/><metric name='UI.Space.M' value='8'/>"
				"<font name='UI.Font.Body' role='Regular' size='14'/>"
				"<surface name='UI.Surface.Panel' fill='UI.Color.Panel'/>"
				"</style>"), TEXT("Styles/T.sltrstyle"), *Result, Errors);
			return Result;
		}();
		return *Style;
	}

	FSlaterBuildResult ElBuild(const TCHAR* Text, const FSlaterElementRegistry& Registry = Slater::Elements())
	{
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, ElPath, ElStyle(), Result, FSlaterBudget(), Registry);
		return Result;
	}

	/** First widget in the tree (Root included) of this Slate type, or null. */
	TSharedPtr<SWidget> ElFind(const TSharedRef<SWidget>& Root, const TCHAR* Type)
	{
		if (Root->GetType() == FName(Type))
		{
			return Root;
		}
		TSharedPtr<SWidget> Found;
		Root->GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = ElFind(Child.AsShared(), Type);
			}
		});
		return Found;
	}

	FVector2D ElDesired(const TSharedRef<SWidget>& Widget)
	{
		Widget->SlatePrepass(1.f);
		return FVector2D(Widget->GetDesiredSize());
	}

	/** Empty when Result has an error at (Line, Column) whose message holds Fragment; else says what it did have. */
	FString ElCheckError(const FSlaterBuildResult& Result, int32 Line, int32 Column, const TCHAR* Fragment)
	{
		if (Result.Succeeded())
		{
			return TEXT("built, but an error was expected");
		}
		FString All;
		for (const FSlaterError& Error : Result.Errors)
		{
			if (Error.Line == Line && Error.Column == Column && Error.Message.Contains(Fragment, ESearchCase::CaseSensitive))
			{
				return FString();
			}
			All += Error.ToString() + TEXT(" | ");
		}
		return FString::Printf(TEXT("no error at (%d,%d) containing '%s'; got: %s"), Line, Column, Fragment, *All);
	}

	const SBoxPanel::FSlot& ElPanelSlot(const TSharedRef<SWidget>& Panel, int32 Index)
	{
		return static_cast<const SBoxPanel::FSlot&>(Panel->GetChildren()->GetSlotAt(Index));
	}
}

/** Each stock element builds the Slate widget it names, with its slots set from the children's attributes. */
TEST_CLASS(SlaterElementBuildTests, "BRS.SlaterUI.Elements.Build")
{
	TEST_METHOD(VBox_AutoAndFillSlots)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><spacer height='10'/><spacer fill='2'/><spacer fill='UI.Space.S'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<SWidget> Root = Result.Root.ToSharedRef();
		ASSERT_THAT(AreEqual(Root->GetType(), FName("SVerticalBox")));
		ASSERT_THAT(AreEqual(Root->GetChildren()->Num(), 3));
		ASSERT_THAT(IsTrue(ElPanelSlot(Root, 0).GetSizeRule() == FSizeParam::SizeRule_Auto));
		ASSERT_THAT(IsTrue(ElPanelSlot(Root, 1).GetSizeRule() == FSizeParam::SizeRule_Stretch));
		ASSERT_THAT(IsTrue(ElPanelSlot(Root, 1).GetSizeValue() == 2.f));
		ASSERT_THAT(IsTrue(ElPanelSlot(Root, 2).GetSizeValue() == 4.f));
	}

	TEST_METHOD(HBox_FillIsWidthAndMarginAndAlignAreApplied)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<hbox><spacer fill='3' margin='UI.Space.M' halign='center' valign='top'/><spacer/></hbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const TSharedRef<SWidget> Root = Result.Root.ToSharedRef();
		ASSERT_THAT(AreEqual(Root->GetType(), FName("SHorizontalBox")));
		const SBoxPanel::FSlot& First = ElPanelSlot(Root, 0);
		ASSERT_THAT(IsTrue(First.GetSizeRule() == FSizeParam::SizeRule_Stretch));
		ASSERT_THAT(IsTrue(First.GetSizeValue() == 3.f));
		ASSERT_THAT(IsTrue(First.GetPadding() == FMargin(8.f)));
		ASSERT_THAT(IsTrue(First.GetHorizontalAlignment() == HAlign_Center));
		ASSERT_THAT(IsTrue(First.GetVerticalAlignment() == VAlign_Top));
		ASSERT_THAT(IsTrue(ElPanelSlot(Root, 1).GetSizeRule() == FSizeParam::SizeRule_Auto));
	}

	TEST_METHOD(Overlay_LayersChildrenInOrder)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<overlay><spacer id='below'/><spacer id='above'/></overlay>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SOverlay")));
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 2));
		ASSERT_THAT(IsTrue(Result.Root->GetChildren()->GetChildAt(0) == Result.ById.FindChecked(TEXT("below")).Pin().ToSharedRef()));
		ASSERT_THAT(IsTrue(Result.Root->GetChildren()->GetChildAt(1) == Result.ById.FindChecked(TEXT("above")).Pin().ToSharedRef()));
	}

	TEST_METHOD(Box_TakesTheSizeAttributesItself_WithNoWrapper)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<box width='120' height='UI.Space.M' padding='4'><spacer/></box>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SBox")));
		// SBox + SSpacer only: the box did not get a second wrapper.
		ASSERT_THAT(AreEqual(Result.WidgetCount, 2));
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 120.0 && Desired.Y == 8.0));
	}

	TEST_METHOD(Box_AddsChildMarginToItsPadding)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<box padding='4'><spacer width='10' height='10' margin='2'/></box>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 22.0 && Desired.Y == 22.0));
	}

	TEST_METHOD(Scroll_BuildsAScrollBoxWithItsChildren)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<scroll orientation='horizontal'><spacer/><spacer/></scroll>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SScrollBox")));
	}

	TEST_METHOD(UniformGrid_PlacesChildrenRowByRow)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<uniform-grid columns='2' gap='4'><spacer width='10' height='10'/><spacer width='10' height='10'/><spacer width='10' height='10'/></uniform-grid>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SUniformGridPanel")));
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 3));
		// Two columns by two rows of 10 + 4 gap (2 each side) is 28 by 28.
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 28.0 && Desired.Y == 28.0));
	}

	TEST_METHOD(Wrap_BuildsAWrapBox)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<wrap gap='4' orientation='vertical'><spacer/><spacer/></wrap>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SWrapBox")));
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 2));
	}

	TEST_METHOD(Spacer_WithASize_GetsOneBoxWrapper)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<spacer width='30' height='20'/>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SBox")));
		ASSERT_THAT(AreEqual(Result.WidgetCount, 2));
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 30.0 && Desired.Y == 20.0));
	}

	TEST_METHOD(NoSizeAttribute_AddsNoWrapper)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><spacer/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.WidgetCount, 2));
		ASSERT_THAT(AreEqual(Result.WidgetDepth, 2));
	}

	TEST_METHOD(Padding_OnAContainer_WrapsItInOneBox)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox padding='UI.Space.M'><spacer width='10' height='10'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SBox")));
		ASSERT_THAT(AreEqual(Result.WidgetCount, 4));
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 26.0 && Desired.Y == 26.0));
	}

	TEST_METHOD(MinAndMax_SizeAttributes_ReachTheBox)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox min-width='50' min-height='40' max-width='60'><spacer width='100' height='10'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		const FVector2D Desired = ElDesired(Result.Root.ToSharedRef());
		ASSERT_THAT(IsTrue(Desired.X == 60.0 && Desired.Y == 40.0));
	}

	TEST_METHOD(Ids_AreCollectedOnTheWidgetThatJoinsTheTree)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox id='root'><spacer id='gap' height='5'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.ById.Num(), 2));
		ASSERT_THAT(IsTrue(Result.ById.FindChecked(TEXT("root")).Pin() == Result.Root));
		// The spacer has a height, so its id names the box wrapper that sits in the vbox.
		ASSERT_THAT(AreEqual(Result.ById.FindChecked(TEXT("gap")).Pin()->GetType(), FName("SBox")));
	}

	TEST_METHOD(NegativeZero_IsNotNegative)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<box width='-0'><spacer/></box>"));
		// "-0" is a number, and zero is not negative.
		ASSERT_THAT(IsTrue(Result.Succeeded()));
	}

	TEST_METHOD(StockElements_AreRegistered)
	{
		TArray<FString> Tags;
		Slater::Elements().GetTags(Tags);
		for (const TCHAR* Tag : {TEXT("vbox"), TEXT("hbox"), TEXT("overlay"), TEXT("box"), TEXT("scroll"), TEXT("uniform-grid"), TEXT("wrap"), TEXT("spacer")})
		{
			ASSERT_THAT(IsTrue(Tags.Contains(Tag)));
		}
	}
};

/** Every error names its line and column, and suggests a fix where there is one. */
TEST_CLASS(SlaterElementErrorTests, "BRS.SlaterUI.Elements.Error")
{
	TEST_METHOD(UnknownTag_SuggestsTheNearest)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n  <hbx/>\n</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 2, 3, TEXT("unknown element <hbx>; did you mean 'hbox'?")), FString()));
		ASSERT_THAT(IsFalse(Result.Root.IsValid()));
	}

	TEST_METHOD(UnknownTag_WithNothingNear_ListsTheKnownOnes)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<zzzzzzzz/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 1, TEXT("known: box, hbox")), FString()));
		ASSERT_THAT(IsFalse(Result.Errors[0].Message.Contains(TEXT("did you mean"))));
	}

	TEST_METHOD(UnknownAttribute_SuggestsTheNearest_AndListsWhatIsAllowed)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n  <spacer pading='4'/>\n</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 2, 11, TEXT("unknown attribute 'pading' on <spacer>; did you mean 'padding'?")), FString()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("allowed: "))));
	}

	TEST_METHOD(UnknownMetricToken_SuggestsTheNearest)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox padding='UI.Space.N'/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 7, TEXT("unknown metric token 'UI.Space.N'; did you mean")), FString()));
	}

	TEST_METHOD(MetricThatIsNeitherANumberNorAToken_IsATypeMismatch)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox width='1 2'/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 7, TEXT("'1 2' is not a metric")), FString()));
	}

	TEST_METHOD(EnumOutsideItsWords_NamesTheAllowedOnes)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<hbox><spacer halign='centre'/></hbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 15, TEXT("'centre' is not a valid 'halign' on <spacer>; did you mean 'center'?")), FString()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("allowed: fill, left, center, right"))));
	}

	TEST_METHOD(IntAttribute_RejectsAFraction)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<uniform-grid columns='2.5'/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 15, TEXT("'2.5' is not an integer")), FString()));
	}

	TEST_METHOD(RequiredAttribute_MissingIsReportedOnTheElement)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n<uniform-grid/>\n</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 2, 1, TEXT("<uniform-grid> needs the attribute 'columns'")), FString()));
	}

	TEST_METHOD(RequiredAttribute_MissingErrorGivesAnExampleValue)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n<uniform-grid/>\n</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 2, 1, TEXT("<uniform-grid> needs the attribute 'columns' (a whole number), for example columns=\"3\"")), FString()));
	}

	TEST_METHOD(FillMustBeAboveZero_SaysHowToFixIt)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><spacer fill='0'/></vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 15, TEXT("'fill' is a weight and must be above 0; remove it, or use a weight such as 1")), FString()));
	}

	TEST_METHOD(UnknownTag_SaysWhatEachKnownElementIsFor_SoLabelPointsAtText)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<label/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 1, TEXT("text = words and numbers shown on screen, what other toolkits call a label")), FString()));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 1, TEXT("vbox = stacks children top to bottom")), FString()));
	}

	TEST_METHOD(SyntaxError_SaysMoreErrorsAppearOnceItIsFixed)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n  <spacer>\n</vbox>"));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.EndsWith(TEXT("(a syntax error stops the build, so more errors may appear once this one is fixed)"))));
	}

	TEST_METHOD(SlotAttribute_OnTheRoot_HasNoParentSlot)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox fill='1'/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 7, TEXT("'fill' is a slot attribute, and the root element <vbox> has no parent slot")), FString()));
	}

	TEST_METHOD(SlotAttribute_TheParentDoesNotUse_IsRefused)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<overlay><spacer fill='1'/></overlay>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 18, TEXT("'fill' has no effect here: the slots of <overlay> do not use it")), FString()));
	}

	TEST_METHOD(FillMustBeAboveZero)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><spacer fill='0'/></vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 15, TEXT("'fill' is a weight and must be above 0")), FString()));
	}

	TEST_METHOD(NegativeSize_IsRefused)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<spacer width='-3'/>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 9, TEXT("'width' must not be negative")), FString()));
	}

	TEST_METHOD(ChildrenOfAnElementThatTakesNone_AreRefused)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<spacer>\n  <spacer/>\n</spacer>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 2, 3, TEXT("<spacer> cannot have child elements")), FString()));
	}

	TEST_METHOD(SecondChildOfABox_IsRefused)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<box>\n<spacer/>\n<spacer/>\n</box>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 3, 1, TEXT("<box> takes one child; this is number 2")), FString()));
	}

	TEST_METHOD(TextInAnElementThatTakesNone_IsRefused)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>hello</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 7, TEXT("<vbox> cannot contain text")), FString()));
	}

	TEST_METHOD(DuplicateId_NamesTheFirst)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n<spacer id='a'/>\n<spacer id='a'/>\n</vbox>"));
		ASSERT_THAT(AreEqual(ElCheckError(Result, 3, 9, TEXT("duplicate id 'a' (first used at 2,9)")), FString()));
	}

	TEST_METHOD(EveryProblemIsReported_NotOnlyTheFirst)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>\n<hbx/>\n<spacer pading='1'/>\n<spacer width='x y'/>\n</vbox>"));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 3));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 2));
		ASSERT_THAT(AreEqual(Result.Errors[1].Line, 3));
		ASSERT_THAT(AreEqual(Result.Errors[2].Line, 4));
	}

	TEST_METHOD(ErrorText_UsesTheOnePathLineColumnFormat)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<hbx/>"));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Errors[0].ToString().StartsWith(TEXT("Layouts/Test.sltr(1,1): unknown element"))));
	}

	TEST_METHOD(SyntaxError_LandsInErrors)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox>"));
		ASSERT_THAT(AreEqual(Result.Errors.Num(), 1));
		ASSERT_THAT(IsFalse(Result.Root.IsValid()));
	}
};

/** The public registry: other plugins register their own elements the same way as the stock ones. */
TEST_CLASS(SlaterElementRegistryTests, "BRS.SlaterUI.Elements.Registry")
{
	static FSlaterElementDesc MakeDesc(const TCHAR* Tag)
	{
		FSlaterElementDesc Desc;
		Desc.Tag = Tag;
		Desc.Builder = [](FSlaterBuildContext&) { return TSharedPtr<SWidget>(SNew(SSpacer)); };
		return Desc;
	}

	TEST_METHOD(RegisteredElement_IsBuiltFromTypedAttributes)
	{
		FSlaterElementRegistry Registry;
		Slater::RegisterLayoutElements(Registry);

		FSlaterElementDesc Desc = MakeDesc(TEXT("swatch"));
		Desc.Attributes.Add({TEXT("color"), ESlaterAttrType::Color, {}, true});
		Desc.Attributes.Add({TEXT("size"), ESlaterAttrType::Metric});
		FLinearColor SeenColor = FLinearColor::Black;
		float SeenSize = 0.f;
		Desc.Builder = [&](FSlaterBuildContext& Context)
		{
			SeenColor = Context.GetColor(TEXT("color"));
			SeenSize = Context.GetMetric(TEXT("size"), 99.f);
			return TSharedPtr<SWidget>(SNew(SSpacer));
		};
		FString Error;
		ASSERT_THAT(IsTrue(Registry.Register(MoveTemp(Desc), &Error)));

		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><swatch color='#FF0000' size='UI.Space.M'/></vbox>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(SeenColor.Equals(FLinearColor::FromSRGBColor(FColor(255, 0, 0)), 0.0001f)));
		ASSERT_THAT(IsTrue(SeenSize == 8.f));
	}

	TEST_METHOD(RegisteredElement_GetsTheSameErrors)
	{
		FSlaterElementRegistry Registry;
		FSlaterElementDesc Desc = MakeDesc(TEXT("swatch"));
		Desc.Attributes.Add({TEXT("color"), ESlaterAttrType::Color, {}, true});
		ASSERT_THAT(IsTrue(Registry.Register(MoveTemp(Desc))));

		const FSlaterBuildResult Missing = ElBuild(TEXT("<swatch/>"), Registry);
		ASSERT_THAT(AreEqual(ElCheckError(Missing, 1, 1, TEXT("<swatch> needs the attribute 'color'")), FString()));
		const FSlaterBuildResult Unknown = ElBuild(TEXT("<swatch color='UI.Color.Pane'/>"), Registry);
		ASSERT_THAT(AreEqual(ElCheckError(Unknown, 1, 9, TEXT("unknown color token 'UI.Color.Pane'; did you mean 'UI.Color.Panel'?")), FString()));
		const FSlaterBuildResult Literal = ElBuild(TEXT("<swatch color='#12345'/>"), Registry);
		ASSERT_THAT(AreEqual(ElCheckError(Literal, 1, 9, TEXT("not a color")), FString()));
	}

	TEST_METHOD(AllAttributeTypes_ParseToTheirOwnType)
	{
		FSlaterElementRegistry Registry;
		FSlaterElementDesc Desc = MakeDesc(TEXT("probe"));
		Desc.Attributes.Add({TEXT("font"), ESlaterAttrType::Font});
		Desc.Attributes.Add({TEXT("role"), ESlaterAttrType::Surface});
		Desc.Attributes.Add({TEXT("mode"), ESlaterAttrType::Enum, {TEXT("a"), TEXT("b")}});
		Desc.Attributes.Add({TEXT("flag"), ESlaterAttrType::Bool});
		Desc.Attributes.Add({TEXT("count"), ESlaterAttrType::Int});
		Desc.Attributes.Add({TEXT("label"), ESlaterAttrType::Text});
		int32 FontSize = 0;
		FLinearColor Fill = FLinearColor::Black;
		FName Mode;
		bool bFlag = false;
		int32 Count = 0;
		FString Label;
		Desc.Builder = [&](FSlaterBuildContext& Context)
		{
			FontSize = Context.GetFont(TEXT("font")).Size;
			Fill = Context.GetSurface(TEXT("role")).Fill;
			Mode = Context.GetEnum(TEXT("mode"));
			bFlag = Context.GetBool(TEXT("flag"));
			Count = Context.GetInt(TEXT("count"));
			Label = Context.GetText(TEXT("label"));
			return TSharedPtr<SWidget>(SNew(SSpacer));
		};
		ASSERT_THAT(IsTrue(Registry.Register(MoveTemp(Desc))));

		const FSlaterBuildResult Result = ElBuild(TEXT("<probe font='UI.Font.Body' role='UI.Surface.Panel' mode='b' flag='true' count='-7' label='Hi there'/>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(FontSize, 14));
		ASSERT_THAT(IsTrue(Fill.Equals(FLinearColor::White, 0.0001f)));
		ASSERT_THAT(AreEqual(Mode, FName("b")));
		ASSERT_THAT(IsTrue(bFlag));
		ASSERT_THAT(AreEqual(Count, -7));
		ASSERT_THAT(AreEqual(Label, FString(TEXT("Hi there"))));

		const FSlaterBuildResult BadBool = ElBuild(TEXT("<probe flag='yes'/>"), Registry);
		ASSERT_THAT(AreEqual(ElCheckError(BadBool, 1, 8, TEXT("'yes' is not a bool")), FString()));
	}

	TEST_METHOD(Register_RefusesBadDescriptors)
	{
		FSlaterElementRegistry Registry;
		FString Error;

		FSlaterElementDesc BadTag = MakeDesc(TEXT("Bad_Tag"));
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(BadTag), &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("not a valid tag"))));

		FSlaterElementDesc NoBuilder;
		NoBuilder.Tag = TEXT("empty");
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(NoBuilder), &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("no builder"))));

		FSlaterElementDesc Clash = MakeDesc(TEXT("clash"));
		Clash.Attributes.Add({TEXT("padding"), ESlaterAttrType::Metric});
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(Clash), &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("common or slot attribute"))));

		FSlaterElementDesc SlotClash = MakeDesc(TEXT("slotclash"));
		SlotClash.Attributes.Add({TEXT("fill"), ESlaterAttrType::Metric});
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(SlotClash), &Error)));

		FSlaterElementDesc Twice = MakeDesc(TEXT("twice"));
		Twice.Attributes.Add({TEXT("a"), ESlaterAttrType::Text});
		Twice.Attributes.Add({TEXT("a"), ESlaterAttrType::Text});
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(Twice), &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("declared twice"))));

		FSlaterElementDesc EmptyEnum = MakeDesc(TEXT("emptyenum"));
		EmptyEnum.Attributes.Add({TEXT("mode"), ESlaterAttrType::Enum});
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(EmptyEnum), &Error)));

		ASSERT_THAT(IsTrue(Registry.Register(MakeDesc(TEXT("fine")))));
		FSlaterElementDesc Duplicate = MakeDesc(TEXT("fine"));
		ASSERT_THAT(IsFalse(Registry.Register(MoveTemp(Duplicate), &Error)));
		ASSERT_THAT(IsTrue(Error.Contains(TEXT("already registered"))));
	}

	TEST_METHOD(Unregister_RemovesTheTag)
	{
		FSlaterElementRegistry Registry;
		ASSERT_THAT(IsTrue(Registry.Register(MakeDesc(TEXT("temp")))));
		ASSERT_THAT(IsTrue(Registry.Find(TEXT("temp")) != nullptr));
		ASSERT_THAT(IsTrue(Registry.Unregister(TEXT("temp"))));
		ASSERT_THAT(IsTrue(Registry.Find(TEXT("temp")) == nullptr));
		ASSERT_THAT(IsFalse(Registry.Unregister(TEXT("temp"))));
	}

	TEST_METHOD(BuilderThatReturnsNull_ReportsOnTheElement)
	{
		FSlaterElementRegistry Registry;
		FSlaterElementDesc Desc = MakeDesc(TEXT("broken"));
		Desc.Builder = [](FSlaterBuildContext&) { return TSharedPtr<SWidget>(); };
		ASSERT_THAT(IsTrue(Registry.Register(MoveTemp(Desc))));
		const FSlaterBuildResult Result = ElBuild(TEXT("<broken/>"), Registry);
		ASSERT_THAT(AreEqual(ElCheckError(Result, 1, 1, TEXT("<broken> could not be built")), FString()));
	}

	TEST_METHOD(TextAndChildren_AreOnlyAllowedWhereDeclared)
	{
		FSlaterElementRegistry Registry;
		FSlaterElementDesc Desc = MakeDesc(TEXT("label"));
		Desc.bAllowsText = true;
		FString Seen;
		Desc.Builder = [&](FSlaterBuildContext& Context)
		{
			Seen = Context.Node.Text;
			return TSharedPtr<SWidget>(SNew(SSpacer));
		};
		ASSERT_THAT(IsTrue(Registry.Register(MoveTemp(Desc))));
		const FSlaterBuildResult Result = ElBuild(TEXT("<label>Hello</label>"), Registry);
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Seen, FString(TEXT("Hello"))));
	}
};

/** The validator measures what it built and warns above the budget. */
TEST_CLASS(SlaterElementBudgetTests, "BRS.SlaterUI.Elements.Budget")
{
	TEST_METHOD(CountAndDepth_AreMeasuredOnTheBuiltTree)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<vbox><hbox><spacer/><spacer/></hbox><spacer/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.WidgetCount, 5));
		ASSERT_THAT(AreEqual(Result.WidgetDepth, 3));
		ASSERT_THAT(AreEqual(Result.Warnings.Num(), 0));
	}

	TEST_METHOD(OverTheWidgetBudget_WarnsButStillBuilds)
	{
		FSlaterBuildResult Result;
		FSlaterBudget Budget;
		Budget.MaxWidgets = 3;
		ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(TEXT("<vbox><spacer/><spacer/><spacer/></vbox>"), ElPath, ElStyle(), Result, Budget)));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Warnings.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Warnings[0].ToString().Contains(TEXT("layout builds 4 widgets, over the budget of 3"))));
		ASSERT_THAT(AreEqual(Result.Warnings[0].Line, 1));
	}

	TEST_METHOD(OverTheDepthBudget_Warns)
	{
		FSlaterBuildResult Result;
		FSlaterBudget Budget;
		Budget.MaxDepth = 2;
		ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(TEXT("<vbox><vbox><spacer/></vbox></vbox>"), ElPath, ElStyle(), Result, Budget)));
		ASSERT_THAT(AreEqual(Result.Warnings.Num(), 1));
		ASSERT_THAT(IsTrue(Result.Warnings[0].Message.Contains(TEXT("3 widgets deep, over the budget of 2"))));
	}

	TEST_METHOD(Errors_SkipTheMeasuring)
	{
		const FSlaterBuildResult Result = ElBuild(TEXT("<hbx/>"));
		ASSERT_THAT(AreEqual(Result.WidgetCount, 0));
		ASSERT_THAT(AreEqual(Result.Warnings.Num(), 0));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
