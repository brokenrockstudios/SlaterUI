// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterStyle.h"

namespace
{
	const TCHAR* const StPath = TEXT("Styles/Test.sltrstyle");

	TSharedRef<FSlaterStyle> StParse(const TCHAR* Text)
	{
		TSharedRef<FSlaterStyle> Style = MakeShared<FSlaterStyle>();
		TArray<FSlaterError> Errors;
		Slater::ParseStyle(Text, StPath, *Style, Errors);
		return Style;
	}

	/** Empty when Text fails with an error at (Line, Column) whose message holds Fragment; else says what happened. */
	FString StCheckError(const TCHAR* Text, int32 Line, int32 Column, const TCHAR* Fragment)
	{
		FSlaterStyle Style;
		TArray<FSlaterError> Errors;
		if (Slater::ParseStyle(Text, StPath, Style, Errors))
		{
			return TEXT("parsed, but an error was expected");
		}
		for (const FSlaterError& Error : Errors)
		{
			if (Error.Line == Line && Error.Column == Column && Error.Message.Contains(Fragment, ESearchCase::CaseSensitive))
			{
				return FString();
			}
		}
		FString All;
		for (const FSlaterError& Error : Errors)
		{
			All += Error.ToString() + TEXT(" | ");
		}
		return FString::Printf(TEXT("no error at (%d,%d) containing '%s'; got: %s"), Line, Column, Fragment, *All);
	}

	FLinearColor StSrgb(uint8 R, uint8 G, uint8 B, uint8 A = 255)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
	}

	bool StSame(const FLinearColor& A, const FLinearColor& B)
	{
		return A.Equals(B, 0.0001f);
	}
}

/** A style looks a token up by name, then by each parent, and never by a child. */
TEST_CLASS(SlaterStyleFallbackTests, "BRS.SlaterUI.Style.Fallback")
{
	TEST_METHOD(Exact_BeatsItsParent)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='UI.Color.Accent' value='#FF0000'/><color name='UI.Color.Accent.Hover' value='#00FF00'/></style>"));
		FLinearColor Color;
		ASSERT_THAT(IsTrue(Style->FindColor(TEXT("UI.Color.Accent.Hover"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(0, 255, 0))));
	}

	TEST_METHOD(MissingState_FallsBackToTheParent)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='UI.Color.Accent' value='#FF0000'/></style>"));
		FLinearColor Color;
		ASSERT_THAT(IsTrue(Style->FindColor(TEXT("UI.Color.Accent.Hover.Disabled"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(255, 0, 0))));
	}

	TEST_METHOD(FallsBackSeveralLevels)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><metric name='UI.Space' value='6'/></style>"));
		float Metric = 0.f;
		ASSERT_THAT(IsTrue(Style->FindMetric(TEXT("UI.Space.Gap.Small"), Metric)));
		ASSERT_THAT(IsTrue(Metric == 6.f));
	}

	TEST_METHOD(NeverFallsDownToAChild)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='UI.Color.Accent.Hover' value='#00FF00'/></style>"));
		FLinearColor Color;
		ASSERT_THAT(IsFalse(Style->FindColor(TEXT("UI.Color.Accent"), Color)));
	}

	TEST_METHOD(KindsDoNotShareNames)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='UI.X' value='#000000'/></style>"));
		float Metric = 0.f;
		FSlaterFont Font;
		ASSERT_THAT(IsFalse(Style->FindMetric(TEXT("UI.X"), Metric)));
		ASSERT_THAT(IsFalse(Style->FindFont(TEXT("UI.X"), Font)));
	}

	TEST_METHOD(Unknown_IsNotFound)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'/>"));
		FLinearColor Color;
		ASSERT_THAT(IsFalse(Style->FindColor(TEXT("UI.Color.Accent"), Color)));
		ASSERT_THAT(IsFalse(Style->FindColor(NAME_None, Color)));
	}

	TEST_METHOD(Font_IsRoleAndSize)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><font name='UI.Font.Body' role='Bold' size='18'/></style>"));
		FSlaterFont Font;
		ASSERT_THAT(IsTrue(Style->FindFont(TEXT("UI.Font.Body.Small"), Font)));
		ASSERT_THAT(AreEqual(Font.Role, FName(TEXT("Bold"))));
		ASSERT_THAT(AreEqual(Font.Size, 18));
	}

	TEST_METHOD(Surface_ResolvesItsColorsAndMetrics)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='C.Fill' value='#102030'/><color name='C.Edge' value='#405060'/><metric name='M.Round' value='5'/>"
			"<surface name='Surf' fill='C.Fill' border='C.Edge' border-width='2' radius='M.Round'/></style>"));
		FSlaterSurface Surface;
		ASSERT_THAT(IsTrue(Style->FindSurface(TEXT("Surf.Hover"), Surface)));
		ASSERT_THAT(IsTrue(StSame(Surface.Fill, StSrgb(0x10, 0x20, 0x30))));
		ASSERT_THAT(IsTrue(StSame(Surface.Border, StSrgb(0x40, 0x50, 0x60))));
		ASSERT_THAT(IsTrue(Surface.BorderWidth == 2.f));
		ASSERT_THAT(IsTrue(Surface.CornerRadius == 5.f));
	}

	TEST_METHOD(ParentToken_DropsTheLastSegment)
	{
		ASSERT_THAT(AreEqual(Slater::ParentToken(TEXT("A.B.C")), FName(TEXT("A.B"))));
		ASSERT_THAT(IsTrue(Slater::ParentToken(TEXT("A")).IsNone()));
	}
};

/** Layers are searched top first, and an exact name in a lower layer beats a parent in a higher one. */
TEST_CLASS(SlaterStyleLayerTests, "BRS.SlaterUI.Style.Layers")
{
	TEST_METHOD(HigherLayer_OverridesTheLower)
	{
		FSlaterStyleStack Stack;
		Stack.PushLayer(StParse(TEXT("<style name='A'><color name='X.Y' value='#FF0000'/></style>")));
		Stack.PushLayer(StParse(TEXT("<style name='B'><color name='X.Y' value='#0000FF'/></style>")));
		FLinearColor Color;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("X.Y"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(0, 0, 255))));
	}

	TEST_METHOD(LowerLayer_ProvidesWhatTheHigherLacks)
	{
		FSlaterStyleStack Stack;
		Stack.PushLayer(StParse(TEXT("<style name='A'><metric name='M.One' value='1'/><metric name='M.Two' value='2'/></style>")));
		Stack.PushLayer(StParse(TEXT("<style name='B'><metric name='M.Two' value='20'/></style>")));
		float One = 0.f;
		float Two = 0.f;
		ASSERT_THAT(IsTrue(Stack.FindMetric(TEXT("M.One"), One)));
		ASSERT_THAT(IsTrue(Stack.FindMetric(TEXT("M.Two"), Two)));
		ASSERT_THAT(IsTrue(One == 1.f));
		ASSERT_THAT(IsTrue(Two == 20.f));
	}

	TEST_METHOD(ExactInLowerLayer_BeatsParentInHigherLayer)
	{
		FSlaterStyleStack Stack;
		Stack.PushLayer(StParse(TEXT("<style name='A'><color name='X.Y.Hover' value='#00FF00'/></style>")));
		Stack.PushLayer(StParse(TEXT("<style name='B'><color name='X.Y' value='#FF0000'/></style>")));
		FLinearColor Color;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("X.Y.Hover"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(0, 255, 0))));
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("X.Y.Pressed"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(255, 0, 0))));
	}

	TEST_METHOD(Surface_TakesItsColorFromTheTopLayer)
	{
		FSlaterStyleStack Stack;
		Stack.PushLayer(StParse(TEXT("<style name='A'><color name='C.Fill' value='#FF0000'/><surface name='Surf' fill='C.Fill'/></style>")));
		Stack.PushLayer(StParse(TEXT("<style name='B'><color name='C.Fill' value='#0000FF'/></style>")));
		FSlaterSurface Surface;
		ASSERT_THAT(IsTrue(Stack.FindSurface(TEXT("Surf"), Surface)));
		ASSERT_THAT(IsTrue(StSame(Surface.Fill, StSrgb(0, 0, 255))));
	}

	TEST_METHOD(KnownTokens_ListsEveryLayerOnce)
	{
		FSlaterStyleStack Stack;
		Stack.PushLayer(StParse(TEXT("<style name='A'><metric name='M.One' value='1'/><metric name='M.Two' value='2'/></style>")));
		Stack.PushLayer(StParse(TEXT("<style name='B'><metric name='M.Two' value='20'/><metric name='M.Three' value='3'/></style>")));
		TArray<FName> Known;
		Stack.GetKnownTokens(ESlaterTokenKind::Metric, Known);
		ASSERT_THAT(AreEqual(Known.Num(), 3));
	}

	TEST_METHOD(Extends_BuildsTheChainBaseFirst)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Base'><color name='C.A' value='#FF0000'/><color name='C.B' value='#FF0000'/></style>"), TEXT("Base.sltrstyle"), Errors)));
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Mid' extends='Base'><color name='C.B' value='#00FF00'/></style>"), TEXT("Mid.sltrstyle"), Errors)));
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Top' extends='Mid'><color name='C.C' value='#0000FF'/></style>"), TEXT("Top.sltrstyle"), Errors)));
		ASSERT_THAT(IsTrue(Library.Validate(Errors)));

		FSlaterStyleStack Stack;
		ASSERT_THAT(IsTrue(Library.BuildStack(TEXT("Top"), Stack)));
		ASSERT_THAT(AreEqual(Stack.NumLayers(), 3));
		FLinearColor A, B, C;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("C.A"), A)));
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("C.B"), B)));
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("C.C"), C)));
		ASSERT_THAT(IsTrue(StSame(A, StSrgb(255, 0, 0))));
		ASSERT_THAT(IsTrue(StSame(B, StSrgb(0, 255, 0))));
		ASSERT_THAT(IsTrue(StSame(C, StSrgb(0, 0, 255))));
	}

	TEST_METHOD(BuildStack_RefusesAnUnknownStyle)
	{
		FSlaterStyleLibrary Library;
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsFalse(Library.BuildStack(TEXT("Nope"), Stack)));
		ASSERT_THAT(AreEqual(Stack.NumLayers(), 0));
	}
};

/** Each mistake in a style file is reported with its line and column, and a near miss names the token it probably meant. */
TEST_CLASS(SlaterStyleFileTests, "BRS.SlaterUI.Style.File")
{
	TEST_METHOD(Valid_FileReadsEveryKind)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S' extends='B'>\n<color name='C.One' value='#11223344'/>\n<metric name='M.One' value='-2.5'/>\n<font name='F.One' role='Italic' size='9'/>\n<surface name='S.One' radius='3'/>\n</style>"));
		ASSERT_THAT(AreEqual(Style->Name, FName(TEXT("S"))));
		ASSERT_THAT(AreEqual(Style->Extends, FName(TEXT("B"))));
		FLinearColor Color;
		float Metric = 0.f;
		ASSERT_THAT(IsTrue(Style->FindColor(TEXT("C.One"), Color)));
		ASSERT_THAT(IsTrue(StSame(Color, StSrgb(0x11, 0x22, 0x33, 0x44))));
		ASSERT_THAT(IsTrue(Style->FindMetric(TEXT("M.One"), Metric)));
		ASSERT_THAT(IsTrue(Metric == -2.5f));
		ASSERT_THAT(AreEqual(Style->Fonts.Num(), 1));
		ASSERT_THAT(AreEqual(Style->Surfaces.Num(), 1));
	}

	TEST_METHOD(SyntaxError_ComesFromTheMarkupParser)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value=#FFF/></style>"), 2, 23, TEXT("")), FString()));
	}

	TEST_METHOD(WrongRoot_SuggestsStyle)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<styel name='S'/>"), 1, 1, TEXT("did you mean 'style'")), FString()));
	}

	TEST_METHOD(MissingName_IsReportedOnTheRoot)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style/>"), 1, 1, TEXT("needs a name")), FString()));
	}

	TEST_METHOD(DottedStyleName_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='A.B'/>"), 1, 8, TEXT("single word")), FString()));
	}

	TEST_METHOD(UnknownElement_SuggestsTheNearestKind)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n  <colour name='A' value='#000000'/>\n</style>"), 2, 3, TEXT("did you mean 'color'")), FString()));
	}

	TEST_METHOD(UnknownAttribute_SuggestsTheNearestAttribute)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n  <color name='A' vlaue='#000000'/>\n</style>"), 2, 19, TEXT("did you mean 'value'")), FString()));
	}

	TEST_METHOD(MissingAttribute_PointsAtTheElement)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n  <color name='A'/>\n</style>"), 2, 3, TEXT("needs a 'value'")), FString()));
	}

	TEST_METHOD(BadTokenName_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n  <color name='UI..Bad' value='#000000'/>\n</style>"), 2, 10, TEXT("not a token name")), FString()));
	}

	TEST_METHOD(DuplicateToken_NamesTheFirst)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value='#000000'/>\n<color name='A' value='#FFFFFF'/>\n</style>"), 3, 8, TEXT("already defined at (2,8)")), FString()));
	}

	TEST_METHOD(SameNameInTwoKinds_IsAllowed)
	{
		const TSharedRef<FSlaterStyle> Style = StParse(TEXT("<style name='S'><color name='A' value='#000000'/><metric name='A' value='1'/></style>"));
		ASSERT_THAT(AreEqual(Style->Colors.Num() + Style->Metrics.Num(), 2));
	}

	TEST_METHOD(BadColor_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value='red'/>\n</style>"), 2, 17, TEXT("#RRGGBB")), FString()));
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value='#12345G'/>\n</style>"), 2, 17, TEXT("#RRGGBB")), FString()));
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value='#FFF'/>\n</style>"), 2, 17, TEXT("#RRGGBB")), FString()));
	}

	TEST_METHOD(BadMetric_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<metric name='A' value='wide'/>\n</style>"), 2, 18, TEXT("not a number")), FString()));
	}

	TEST_METHOD(BadFontRole_SuggestsTheNearestRole)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<font name='A' role='Bld' size='12'/>\n</style>"), 2, 16, TEXT("did you mean 'Bold'")), FString()));
	}

	TEST_METHOD(BadFontSize_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<font name='A' role='Bold' size='0'/>\n</style>"), 2, 28, TEXT("whole number")), FString()));
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<font name='A' role='Bold' size='1.5'/>\n</style>"), 2, 28, TEXT("whole number")), FString()));
	}

	TEST_METHOD(NegativeBorderWidth_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<surface name='A' border-width='-1'/>\n</style>"), 2, 19, TEXT("negative")), FString()));
	}

	TEST_METHOD(ContentInsideALeaf_IsRefused)
	{
		ASSERT_THAT(AreEqual(StCheckError(TEXT("<style name='S'>\n<color name='A' value='#000000'>hello</color>\n</style>"), 2, 33, TEXT("takes attributes only")), FString()));
	}

	TEST_METHOD(SeveralMistakes_AreAllReported)
	{
		FSlaterStyle Style;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsFalse(Slater::ParseStyle(TEXT("<style name='S'>\n<color name='A' value='red'/>\n<metric name='B' value='x'/>\n</style>"), StPath, Style, Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 2));
		ASSERT_THAT(AreEqual(Errors[0].Line, 2));
		ASSERT_THAT(AreEqual(Errors[1].Line, 3));
		ASSERT_THAT(AreEqual(Errors[0].Path, FString(StPath)));
	}

	TEST_METHOD(OverTheCaps_FailsLikeALayout)
	{
		FSlaterStyle Style;
		TArray<FSlaterError> Errors;
		FSlaterLimits Limits;
		Limits.MaxNodes = 2;
		ASSERT_THAT(IsFalse(Slater::ParseStyle(TEXT("<style name='S'><color name='A' value='#000000'/><color name='B' value='#000000'/></style>"), StPath, Style, Errors, Limits)));
		ASSERT_THAT(AreEqual(Errors.Num(), 1));
	}
};

/** Checks that need every style loaded: extends, and the colors a surface names. */
TEST_CLASS(SlaterStyleLibraryTests, "BRS.SlaterUI.Style.Library")
{
	FString ValidateOne(const TCHAR* Text, int32 Line, int32 Column, const TCHAR* Fragment)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		if (!Library.AddFromText(Text, StPath, Errors))
		{
			return TEXT("AddFromText failed");
		}
		if (Library.Validate(Errors))
		{
			return TEXT("validated, but an error was expected");
		}
		return Errors.Num() == 1 && Errors[0].Line == Line && Errors[0].Column == Column && Errors[0].Message.Contains(Fragment, ESearchCase::CaseSensitive)
			? FString() : FString::Printf(TEXT("got %d errors, first '%s'"), Errors.Num(), Errors.Num() ? *Errors[0].ToString() : TEXT(""));
	}

	TEST_METHOD(SurfaceColor_NearMiss_SuggestsTheToken)
	{
		ASSERT_THAT(AreEqual(ValidateOne(TEXT("<style name='S'>\n<color name='UI.Color.Accent' value='#000000'/>\n<surface name='P' fill='UI.Color.Acent'/>\n</style>"), 3, 19, TEXT("did you mean 'UI.Color.Accent'")), FString()));
	}

	TEST_METHOD(SurfaceColor_NoNearMiss_SaysOnlyThatItIsUndefined)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='S'>\n<color name='UI.Color.Accent' value='#000000'/>\n<surface name='P' border='Zebra'/>\n</style>"), StPath, Errors)));
		ASSERT_THAT(IsFalse(Library.Validate(Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 1));
		ASSERT_THAT(IsFalse(Errors[0].Message.Contains(TEXT("did you mean"))));
		ASSERT_THAT(AreEqual(Errors[0].Column, 19));
	}

	TEST_METHOD(SurfaceColor_ResolvesThroughParentFallback)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='S'><color name='UI.Color.Accent' value='#000000'/><surface name='P' fill='UI.Color.Accent.Hover'/></style>"), StPath, Errors)));
		ASSERT_THAT(IsTrue(Library.Validate(Errors)));
	}

	TEST_METHOD(SurfaceMetric_NearMiss_SuggestsTheToken)
	{
		ASSERT_THAT(AreEqual(ValidateOne(TEXT("<style name='S'>\n<metric name='UI.Radius.M' value='4'/>\n<surface name='P' radius='UI.Radius.N'/>\n</style>"), 3, 19, TEXT("did you mean 'UI.Radius.M'")), FString()));
	}

	TEST_METHOD(SurfaceColor_MayComeFromTheExtendedStyle)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Base'><color name='C.A' value='#000000'/></style>"), TEXT("Base"), Errors)));
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Kid' extends='Base'><surface name='P' fill='C.A'/></style>"), TEXT("Kid"), Errors)));
		ASSERT_THAT(IsTrue(Library.Validate(Errors)));
	}

	TEST_METHOD(UnknownExtends_SuggestsTheNearestStyle)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='Base'/>"), TEXT("Base"), Errors)));
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("\n<style name='Kid' extends='Bsae'/>"), TEXT("Kid"), Errors)));
		ASSERT_THAT(IsFalse(Library.Validate(Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Errors[0].Line, 2));
		ASSERT_THAT(AreEqual(Errors[0].Column, 19));
		ASSERT_THAT(IsTrue(Errors[0].Message.Contains(TEXT("did you mean 'Base'"))));
	}

	TEST_METHOD(ExtendsCycle_IsRefused)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='A' extends='B'/>"), TEXT("A"), Errors)));
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='B' extends='A'/>"), TEXT("B"), Errors)));
		ASSERT_THAT(IsFalse(Library.Validate(Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 2));
		ASSERT_THAT(IsTrue(Errors[0].Message.Contains(TEXT("cycle"))));
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsFalse(Library.BuildStack(TEXT("A"), Stack)));
	}

	TEST_METHOD(DuplicateStyleName_NamesTheFirstFile)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.AddFromText(TEXT("<style name='A'/>"), TEXT("First.sltrstyle"), Errors)));
		ASSERT_THAT(IsFalse(Library.AddFromText(TEXT("<style name='A'/>"), TEXT("Second.sltrstyle"), Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Errors[0].Path, FString(TEXT("Second.sltrstyle"))));
		ASSERT_THAT(IsTrue(Errors[0].Message.Contains(TEXT("First.sltrstyle"))));
	}

	TEST_METHOD(MissingDirectory_IsAnError)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsFalse(Library.LoadDirectory(TEXT("Z:/definitely/not/here"), Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 1));
	}

	TEST_METHOD(SuggestNearest_IgnoresCase_AndRefusesFarNames)
	{
		const TArray<FString> Candidates = {TEXT("UI.Color.Accent"), TEXT("UI.Color.Panel")};
		ASSERT_THAT(AreEqual(Slater::SuggestNearest(TEXT("ui.color.accent"), Candidates), FString(TEXT("UI.Color.Accent"))));
		ASSERT_THAT(IsTrue(Slater::SuggestNearest(TEXT("Zebra"), Candidates).IsEmpty()));
	}
};

/** The styles that ship in Content/UI/Styles. */
TEST_CLASS(SlaterShippedStyleTests, "BRS.SlaterUI.Style.Shipped")
{
	TEST_METHOD(BaseAndDusk_LoadWithoutErrors)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		const bool bLoaded = Library.LoadPluginStyles(Errors);
		for (const FSlaterError& Error : Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsTrue(bLoaded));
		ASSERT_THAT(IsTrue(Library.Find(TEXT("Base")).IsValid()));
		ASSERT_THAT(IsTrue(Library.Find(TEXT("Dusk")).IsValid()));
	}

	TEST_METHOD(Dusk_RecolorsBase_AndKeepsItsMetrics)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.LoadPluginStyles(Errors)));
		FSlaterStyleStack Base;
		FSlaterStyleStack Dusk;
		ASSERT_THAT(IsTrue(Library.BuildStack(TEXT("Base"), Base)));
		ASSERT_THAT(IsTrue(Library.BuildStack(TEXT("Dusk"), Dusk)));
		ASSERT_THAT(AreEqual(Dusk.NumLayers(), 2));

		FLinearColor BasePanel, DuskPanel;
		ASSERT_THAT(IsTrue(Base.FindColor(TEXT("UI.Color.Panel"), BasePanel)));
		ASSERT_THAT(IsTrue(Dusk.FindColor(TEXT("UI.Color.Panel"), DuskPanel)));
		ASSERT_THAT(IsFalse(StSame(BasePanel, DuskPanel)));

		float BaseSpace = 0.f, DuskSpace = 0.f;
		ASSERT_THAT(IsTrue(Base.FindMetric(TEXT("UI.Space.M"), BaseSpace)));
		ASSERT_THAT(IsTrue(Dusk.FindMetric(TEXT("UI.Space.M"), DuskSpace)));
		ASSERT_THAT(IsTrue(BaseSpace == DuskSpace));
	}

	TEST_METHOD(Dusk_SurfaceUsesDuskColors)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.LoadPluginStyles(Errors)));
		FSlaterStyleStack Dusk;
		ASSERT_THAT(IsTrue(Library.BuildStack(TEXT("Dusk"), Dusk)));
		FSlaterSurface Surface;
		FLinearColor DuskPanel;
		ASSERT_THAT(IsTrue(Dusk.FindSurface(TEXT("UI.Surface.Panel"), Surface)));
		ASSERT_THAT(IsTrue(Dusk.FindColor(TEXT("UI.Color.Panel"), DuskPanel)));
		ASSERT_THAT(IsTrue(StSame(Surface.Fill, DuskPanel)));
		ASSERT_THAT(IsTrue(Surface.CornerRadius == 4.f));
	}

	TEST_METHOD(HoverSurface_FallsBackToTheButton)
	{
		FSlaterStyleLibrary Library;
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsTrue(Library.LoadPluginStyles(Errors)));
		FSlaterStyleStack Base;
		ASSERT_THAT(IsTrue(Library.BuildStack(TEXT("Base"), Base)));
		FSlaterSurface Surface;
		ASSERT_THAT(IsTrue(Base.FindSurface(TEXT("UI.Surface.Button.Disabled"), Surface)));
		FLinearColor Accent;
		ASSERT_THAT(IsTrue(Base.FindColor(TEXT("UI.Color.Accent"), Accent)));
		ASSERT_THAT(IsTrue(StSame(Surface.Fill, Accent)));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
