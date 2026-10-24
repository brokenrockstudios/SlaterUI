// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Layout/ArrangedChildren.h"
#include "SlaterElements.h"
#include "Widgets/SWidget.h"

namespace SlaterPanelElementTestsPrivate
{
	const TCHAR* const PanelPath = TEXT("Layouts/Panel.sltr");

	FSlaterBuildResult PanelBuild(const TCHAR* Text)
	{
		static FSlaterStyle* Style = new FSlaterStyle();
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Text, PanelPath, *Style, Result);
		return Result;
	}

	/** Where the root's child `Id` lands when the root is given `Size`: top-left corner and size, in the root's space. */
	bool PanelPlace(const FSlaterBuildResult& Result, const TCHAR* Id, const FVector2D& Size, FVector2D& OutPosition, FVector2D& OutSize)
	{
		const TSharedPtr<SWidget> Target = Result.ById.FindChecked(Id).Pin();
		Result.Root->SlatePrepass(1.f);
		FArrangedChildren Arranged(EVisibility::Visible);
		Result.Root->ArrangeChildren(FGeometry::MakeRoot(FVector2f(Size), FSlateLayoutTransform()), Arranged);
		for (int32 Index = 0; Index < Arranged.Num(); ++Index)
		{
			if (Arranged[Index].Widget == Target)
			{
				OutPosition = FVector2D(Arranged[Index].Geometry.GetAbsolutePosition());
				OutSize = FVector2D(Arranged[Index].Geometry.GetLocalSize());
				return true;
			}
		}
		return false;
	}

	FString PanelCheckError(const FSlaterBuildResult& Result, int32 Line, int32 Column, const TCHAR* Fragment)
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
		return FString::Printf(TEXT("no error at %d,%d containing '%s'; got: %s"), Line, Column, Fragment, *All);
	}
}
using namespace SlaterPanelElementTestsPrivate;

/** `canvas`, `grid` and `safe-zone` (T-529): the elements, their slot attributes, and where those put a child. */
TEST_CLASS(SlaterPanelElementTests, "BRS.SlaterUI.Panels")
{
	TEST_METHOD(TheElements_AreRegistered)
	{
		TArray<FString> Tags;
		Slater::Elements().GetTags(Tags);
		for (const TCHAR* Tag : {TEXT("canvas"), TEXT("grid"), TEXT("safe-zone")})
		{
			ASSERT_THAT(IsTrue(Tags.Contains(Tag)));
		}
	}

	TEST_METHOD(Canvas_AnchoredToACorner_FollowsTheViewportSize)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><spacer id='c' anchor='bottom-right' offset='-10, -20' size='40, 30'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SConstraintCanvas")));
		FVector2D Position, Size;
		// The child's bottom-right corner sits 10 left of and 20 above the viewport's corner.
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(800, 600), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(750, 550) && Size == FVector2D(40, 30)));
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(1000, 500), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(950, 450) && Size == FVector2D(40, 30)));
	}

	TEST_METHOD(Canvas_WithoutAnAnchor_PinsToTheTopLeft)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><spacer id='c' offset='12, 8' size='20, 10'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FVector2D Position, Size;
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(800, 600), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(12, 8) && Size == FVector2D(20, 10)));
	}

	TEST_METHOD(Canvas_Center_UsesTheChildsOwnSize_AndAlignMovesIt)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><spacer id='a' anchor='center' width='30' height='10'/><spacer id='b' anchor='center' align='0, 0' width='30' height='10'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FVector2D Position, Size;
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("a"), FVector2D(800, 600), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(385, 295) && Size == FVector2D(30, 10)));
		// align="0, 0" puts the child's own top-left corner on the anchor instead of its middle.
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("b"), FVector2D(800, 600), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(400, 300)));
	}

	TEST_METHOD(Canvas_AWideAnchor_StretchesAcrossTheParent_WithInsets)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><spacer id='c' anchor='top-wide' offset='5, 7' size='0, 20'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FVector2D Position, Size;
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(800, 600), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(5, 7) && Size == FVector2D(790, 20)));
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(400, 300), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(5, 7) && Size == FVector2D(390, 20)));
	}

	TEST_METHOD(Canvas_Fill_CoversTheParent)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><spacer id='c' anchor='fill'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FVector2D Position, Size;
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(640, 480), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(0, 0) && Size == FVector2D(640, 480)));
	}

	TEST_METHOD(Canvas_Slots_AreRefusedOnOtherParents)
	{
		const FSlaterBuildResult InBox = PanelBuild(TEXT("<vbox>\n<spacer anchor='center'/>\n</vbox>"));
		ASSERT_THAT(AreEqual(PanelCheckError(InBox, 2, 9, TEXT("'anchor' has no effect here: the slots of <vbox> do not use it")), FString()));
		const FSlaterBuildResult InGrid = PanelBuild(TEXT("<grid>\n<spacer offset='1, 2'/>\n</grid>"));
		ASSERT_THAT(AreEqual(PanelCheckError(InGrid, 2, 9, TEXT("'offset' has no effect here: the slots of <grid> do not use it")), FString()));
	}

	TEST_METHOD(Canvas_RefusesABadAnchor_AndBadPairs)
	{
		const FSlaterBuildResult Anchor = PanelBuild(TEXT("<canvas>\n<spacer anchor='middle'/>\n</canvas>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Anchor, 2, 9, TEXT("'middle' is not a valid 'anchor'")), FString()));
		const FSlaterBuildResult Pair = PanelBuild(TEXT("<canvas>\n<spacer offset='8'/>\n</canvas>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Pair, 2, 9, TEXT("is not two numbers")), FString()));
		const FSlaterBuildResult Negative = PanelBuild(TEXT("<canvas>\n<spacer size='-1, 4'/>\n</canvas>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Negative, 2, 9, TEXT("'size' must not be negative")), FString()));
	}

	TEST_METHOD(AnElementsOwnSize_StaysItsOwn_InACanvas)
	{
		// `throbber` declares `size` itself; inside a canvas it is still the throbber's size, not the slot's.
		const FSlaterBuildResult Result = PanelBuild(TEXT("<canvas><throbber size='24'/></canvas>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
	}

	TEST_METHOD(Grid_PlacesChildrenInRowsAndColumns_AndASpanCoversTwo)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT(
			"<grid>"
			"<spacer id='a' row='0' column='0' column-span='2' width='100' height='10'/>"
			"<spacer id='b' row='1' column='0' width='30' height='10'/>"
			"<spacer id='c' row='1' column='1' width='30' height='10'/>"
			"</grid>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SGridPanel")));
		FVector2D Position, Size;
		// The 100 wide spanning cell shares its width between two columns, so each is 50.
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("a"), FVector2D(100, 20), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(0, 0) && Size == FVector2D(100, 10)));
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("c"), FVector2D(100, 20), Position, Size)));
		ASSERT_THAT(IsTrue(Position == FVector2D(50, 10)));
	}

	TEST_METHOD(Grid_ColumnFill_SharesTheLeftoverWidth)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT(
			"<grid column-fill='1, 0'>"
			"<spacer id='a' column='0' width='10' height='10'/>"
			"<spacer id='b' column='1' width='10' height='10'/>"
			"</grid>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		FVector2D Position, Size;
		ASSERT_THAT(IsTrue(PanelPlace(Result, TEXT("b"), FVector2D(100, 10), Position, Size)));
		// Column 0 takes everything the second column does not need.
		ASSERT_THAT(IsTrue(Position == FVector2D(90, 0)));
	}

	TEST_METHOD(Grid_Gap_AddsSpaceAroundCells)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<grid gap='4'><spacer width='10' height='10'/><spacer column='1' width='10' height='10'/></grid>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		Result.Root->SlatePrepass(1.f);
		// Two cells of 10 plus 2 on each side of each is 28 wide, 14 high.
		ASSERT_THAT(IsTrue(FVector2D(Result.Root->GetDesiredSize()) == FVector2D(28, 14)));
	}

	TEST_METHOD(Grid_Slots_AreRefusedOnOtherParents_AndBadNumbersAreRefused)
	{
		const FSlaterBuildResult InCanvas = PanelBuild(TEXT("<canvas>\n<spacer row='1'/>\n</canvas>"));
		ASSERT_THAT(AreEqual(PanelCheckError(InCanvas, 2, 9, TEXT("'row' has no effect here: the slots of <canvas> do not use it")), FString()));
		const FSlaterBuildResult InUniform = PanelBuild(TEXT("<uniform-grid columns='2'>\n<spacer column-span='2'/>\n</uniform-grid>"));
		ASSERT_THAT(AreEqual(PanelCheckError(InUniform, 2, 9, TEXT("'column-span' has no effect here")), FString()));
		const FSlaterBuildResult Negative = PanelBuild(TEXT("<grid>\n<spacer row='-1'/>\n</grid>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Negative, 2, 9, TEXT("'row' counts from 0 and must not be negative")), FString()));
		const FSlaterBuildResult Span = PanelBuild(TEXT("<grid>\n<spacer row-span='0'/>\n</grid>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Span, 2, 9, TEXT("'row-span' is the number of cells covered and must be 1 or more")), FString()));
		const FSlaterBuildResult Weights = PanelBuild(TEXT("<grid column-fill='a, b'/>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Weights, 1, 7, TEXT("is not a list of weights")), FString()));
	}

	TEST_METHOD(SafeZone_WrapsItsOneChild)
	{
		const FSlaterBuildResult Result = PanelBuild(TEXT("<safe-zone pad-left='false' title-safe='false'><spacer/></safe-zone>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(AreEqual(Result.Root->GetType(), FName("SSafeZone")));
		ASSERT_THAT(AreEqual(Result.Root->GetChildren()->Num(), 1));
	}

	TEST_METHOD(SafeZone_TakesOneChild_AndNoSlotAttributes)
	{
		const FSlaterBuildResult Two = PanelBuild(TEXT("<safe-zone>\n<spacer/>\n<spacer/>\n</safe-zone>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Two, 3, 1, TEXT("<safe-zone> takes one child")), FString()));
		const FSlaterBuildResult Slot = PanelBuild(TEXT("<safe-zone>\n<spacer margin='4'/>\n</safe-zone>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Slot, 2, 9, TEXT("'margin' has no effect here: the slots of <safe-zone> do not use it")), FString()));
		const FSlaterBuildResult Bad = PanelBuild(TEXT("<safe-zone pad-left='yes'/>"));
		ASSERT_THAT(AreEqual(PanelCheckError(Bad, 1, 12, TEXT("is not a bool")), FString()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
