// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterControls.h"
#include "SlaterListTestKit.h"
#include "SlaterText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

namespace SlaterTextAttrTestsPrivate
{
	FSlaterBuildResult TxBuild(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		Slater::BuildLayoutFromText(Layout, TEXT("Layouts/Test.sltr"), Stack, Result);
		return Result;
	}

	TSharedPtr<SSlaterText> TxText(const FSlaterBuildResult& Result)
	{
		return Result.Succeeded() ? StaticCastSharedPtr<SSlaterText>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("STextBlock"))) : nullptr;
	}

	TSharedPtr<SEditableTextBox> TxBox(const FSlaterBuildResult& Result)
	{
		return Result.Succeeded() ? StaticCastSharedPtr<SEditableTextBox>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SEditableTextBox"))) : nullptr;
	}

	TSharedPtr<SMultiLineEditableTextBox> TxMulti(const FSlaterBuildResult& Result)
	{
		return Result.Succeeded() ? StaticCastSharedPtr<SMultiLineEditableTextBox>(SlaterListKit::Find(Result.Root.ToSharedRef(), TEXT("SMultiLineEditableTextBox"))) : nullptr;
	}
}
using namespace SlaterTextAttrTestsPrivate;

/** The `text` and `textbox` attribute batch (T-533). */
TEST_CLASS(SlaterTextAttrTests, "BRS.SlaterUI.TextAttrs")
{
	TEST_METHOD(Text_Defaults_AreClipNoLimitNoShadow)
	{
		const FSlaterBuildResult Result = TxBuild(TEXT("<vbox><text>Hi</text></vbox>"));
		const TSharedPtr<SSlaterText> Text = TxText(Result);
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsTrue(Text->GetOverflow() == ETextOverflowPolicy::Clip));
		ASSERT_THAT(AreEqual(Text->GetMaxLines(), 0));
		ASSERT_THAT(IsNear(Text->GetLineHeight(), 1.f, 1e-6f));
		ASSERT_THAT(IsTrue(Text->GetShadowOffset().IsZero()));
		ASSERT_THAT(IsTrue(Text->GetTransformPolicyImpl() == ETextTransformPolicy::None));
		ASSERT_THAT(IsNear(Text->GetMinDesiredWidth(), 0.f, 1e-6f));
	}

	TEST_METHOD(Text_Overflow_Ellipsis_ReachesTheWidget)
	{
		const TSharedPtr<SSlaterText> Text = TxText(TxBuild(TEXT("<vbox><text overflow='ellipsis'>Hi</text></vbox>")));
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsTrue(Text->GetOverflow() == ETextOverflowPolicy::Ellipsis));
	}

	TEST_METHOD(Text_LineHeight_ReachesTheWidget)
	{
		const TSharedPtr<SSlaterText> Text = TxText(TxBuild(TEXT("<vbox><text line-height='1.5'>Hi</text></vbox>")));
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsNear(Text->GetLineHeight(), 1.5f, 1e-6f));
	}

	TEST_METHOD(Text_Shadow_SetsOffsetAndColor)
	{
		const TSharedPtr<SSlaterText> Text = TxText(TxBuild(TEXT("<vbox><text shadow='2' shadow-color='#FF0000'>Hi</text></vbox>")));
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsNear(Text->GetShadowOffset().X, 2.f, 1e-6f));
		ASSERT_THAT(IsNear(Text->GetShadowOffset().Y, 2.f, 1e-6f));
		ASSERT_THAT(IsNear(Text->GetShadowColorAndOpacity().R, 1.f, 1e-3f));
		ASSERT_THAT(IsNear(Text->GetShadowColorAndOpacity().G, 0.f, 1e-3f));
	}

	TEST_METHOD(Text_ShadowWithoutColor_IsHalfBlack)
	{
		const TSharedPtr<SSlaterText> Text = TxText(TxBuild(TEXT("<vbox><text shadow='1'>Hi</text></vbox>")));
		ASSERT_THAT(IsTrue(Text.IsValid()));
		ASSERT_THAT(IsNear(Text->GetShadowColorAndOpacity().A, 0.5f, 1e-3f));
	}

	TEST_METHOD(Text_ShadowColorWithoutShadow_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><text shadow-color='#FF0000'>Hi</text></vbox>")).Succeeded()));
	}

	TEST_METHOD(Text_Transform_UpperAndLower_ReachTheWidget)
	{
		const TSharedPtr<SSlaterText> Upper = TxText(TxBuild(TEXT("<vbox><text transform='upper'>Hi</text></vbox>")));
		const TSharedPtr<SSlaterText> Lower = TxText(TxBuild(TEXT("<vbox><text transform='lower'>Hi</text></vbox>")));
		ASSERT_THAT(IsTrue(Upper.IsValid() && Lower.IsValid()));
		ASSERT_THAT(IsTrue(Upper->GetTransformPolicyImpl() == ETextTransformPolicy::ToUpper));
		ASSERT_THAT(IsTrue(Lower->GetTransformPolicyImpl() == ETextTransformPolicy::ToLower));
	}

	TEST_METHOD(Text_MinWidth_IsTheCommonAttribute_AndWidensTheElement)
	{
		const FSlaterBuildResult Result = TxBuild(TEXT("<vbox><text min-width='200'>Hi</text></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		Result.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Result.Root->GetDesiredSize().X >= 200.f));
	}

	TEST_METHOD(Text_MaxLines_ReachesTheWidget_AndCapsTheHeight)
	{
		const FSlaterBuildResult One = TxBuild(TEXT("<vbox><text wrap='true'>Hi</text></vbox>"));
		const FSlaterBuildResult Capped = TxBuild(TEXT("<vbox><text wrap='true' max-lines='1'>one two three four five six seven eight nine ten eleven twelve</text></vbox>"));
		const TSharedPtr<SSlaterText> Reference = TxText(One);
		const TSharedPtr<SSlaterText> Text = TxText(Capped);
		ASSERT_THAT(IsTrue(Reference.IsValid() && Text.IsValid()));
		ASSERT_THAT(AreEqual(Text->GetMaxLines(), 1));
		// Wrapped at 60 units the long text would take many lines; the cap holds it to the height of one.
		Reference->SlatePrepass(1.f);
		Text->SetWrapTextAt(60.f);
		Text->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Text->GetDesiredSize().Y <= Reference->GetDesiredSize().Y + 0.5f));
	}

	TEST_METHOD(Text_MaxLinesBelowOne_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><text max-lines='0'>Hi</text></vbox>")).Succeeded()));
	}

	TEST_METHOD(Text_LineHeightZeroOrLess_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><text line-height='0'>Hi</text></vbox>")).Succeeded()));
	}

	TEST_METHOD(Text_BadOverflowOrTransformWord_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><text overflow='fade'>Hi</text></vbox>")).Succeeded()));
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><text transform='title'>Hi</text></vbox>")).Succeeded()));
	}

	TEST_METHOD(TextBox_Defaults_AreOneLineEditablePlain)
	{
		const TSharedPtr<SEditableTextBox> Box = TxBox(TxBuild(TEXT("<vbox><textbox/></vbox>")));
		ASSERT_THAT(IsTrue(Box.IsValid()));
		ASSERT_THAT(IsFalse(Box->IsPassword()));
		ASSERT_THAT(IsFalse(Box->IsReadOnly()));
	}

	TEST_METHOD(TextBox_PasswordAndReadonly_ReachTheWidget)
	{
		const TSharedPtr<SEditableTextBox> Box = TxBox(TxBuild(TEXT("<vbox><textbox password='true' readonly='true'/></vbox>")));
		ASSERT_THAT(IsTrue(Box.IsValid()));
		ASSERT_THAT(IsTrue(Box->IsPassword()));
		ASSERT_THAT(IsTrue(Box->IsReadOnly()));
	}

	TEST_METHOD(TextBox_MinWidth_IsTheCommonAttribute_AlsoForMultiline)
	{
		const FSlaterBuildResult Single = TxBuild(TEXT("<vbox><textbox min-width='240'/></vbox>"));
		const FSlaterBuildResult Multi = TxBuild(TEXT("<vbox><textbox multiline='true' min-width='240'/></vbox>"));
		ASSERT_THAT(IsTrue(Single.Succeeded() && Multi.Succeeded()));
		Single.Root->SlatePrepass(1.f);
		Multi.Root->SlatePrepass(1.f);
		ASSERT_THAT(IsTrue(Single.Root->GetDesiredSize().X >= 240.f));
		ASSERT_THAT(IsTrue(Multi.Root->GetDesiredSize().X >= 240.f));
	}

	TEST_METHOD(TextBox_SelectAllAndRevert_Build)
	{
		const FSlaterBuildResult Result = TxBuild(TEXT("<vbox><textbox select-all-on-focus='true' revert-on-escape='true'/></vbox>"));
		ASSERT_THAT(IsTrue(Result.Succeeded()));
		ASSERT_THAT(IsTrue(TxBox(Result).IsValid()));
	}

	TEST_METHOD(TextBox_Multiline_BuildsTheMultiLineBox_WithItsLook)
	{
		const FSlaterBuildResult Result = TxBuild(TEXT("<vbox><textbox multiline='true' text='a'/></vbox>"));
		const TSharedPtr<SMultiLineEditableTextBox> Box = TxMulti(Result);
		ASSERT_THAT(IsTrue(Box.IsValid()));
		ASSERT_THAT(AreEqual(Box->GetText().ToString(), FString(TEXT("a"))));
		const TSharedPtr<FSlaterControlLook> Look = Slater::FindControlLook(*Box);
		ASSERT_THAT(IsTrue(Look.IsValid() && Look->bMultiLine && Look->TextBox.IsValid()));
	}

	TEST_METHOD(TextBox_MultilineWithOnCommit_IsAllowed)
	{
		// Enter makes a new line in a multi-line box, so on-commit fires only when focus leaves (said in the cheat sheet).
		ASSERT_THAT(IsTrue(TxBuild(TEXT("<vbox><textbox multiline='true' on-commit='Save'/></vbox>")).Succeeded()));
	}

	TEST_METHOD(TextBox_PasswordWithMultiline_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><textbox multiline='true' password='true'/></vbox>")).Succeeded()));
	}

	TEST_METHOD(TextBox_BadJustifyWord_IsRefused)
	{
		ASSERT_THAT(IsFalse(TxBuild(TEXT("<vbox><textbox justify='full'/></vbox>")).Succeeded()));
		ASSERT_THAT(IsTrue(TxBuild(TEXT("<vbox><textbox justify='center'/></vbox>")).Succeeded()));
	}
};

#endif
