// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterButton.h"
#include "SlaterCommands.h"
#include "SlaterSurface.h"
#include "SlaterView.h"

#include "Framework/Application/SlateApplication.h"

namespace
{
	const TCHAR* const BtLayout = TEXT(
		"<vbox>\n"
		"\t<state name=\"tab\" default=\"gear\"/>\n"
		"\t<button id=\"A\" role=\"UI.Surface.Button\" on-click=\"Test.Use:Sword\"><text>A</text></button>\n"
		"\t<button id=\"B\" role=\"UI.Surface.Button\" set-state=\"tab=map\"><text>B</text></button>\n"
		"\t<button id=\"Off\" role=\"UI.Surface.Button\" on-click=\"Test.Use\" enabled=\"false\"><text>Off</text></button>\n"
		"</vbox>");

	FKeyEvent BtKey(const FKey& Key)
	{
		return FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0);
	}

	FFocusEvent BtFocus(EFocusCause Cause)
	{
		return FFocusEvent(Cause, 0);
	}

	FPointerEvent BtLeftMouse()
	{
		return FPointerEvent(0, FVector2f::ZeroVector, FVector2f::ZeroVector, TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	}

	/** Presses and releases a key on the button, the way the application sends a confirm key. */
	FReply BtTap(SSlaterButton& Button, const FKey& Key)
	{
		const FGeometry Geometry;
		const FReply Down = Button.OnKeyDown(Geometry, BtKey(Key));
		Button.OnKeyUp(Geometry, BtKey(Key));
		return Down;
	}
}

/** The `button` element (T-524): keyboard and gamepad route over `SButton`, with the surface roles of `surface`. */
TEST_CLASS(SlaterButtonTests, "BRS.SlaterUI.Button")
{
	FSlaterBindFixture Fix;
	int32 Fired = 0;
	FString LastArgument;

	BEFORE_EACH()
	{
		Fix.Init();
		Fix.Write(TEXT("BtLayout.sltr"), BtLayout);
		Fired = 0;
		LastArgument.Reset();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TSharedRef<SSlaterView> MakeView()
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("BtLayout"));
		View->GetCommands().Register(TEXT("Test.Use"), [this](const FSlaterCommandCall& Call)
		{
			++Fired;
			LastArgument = Call.Argument;
		});
		return View;
	}

	TSharedPtr<SSlaterButton> Button(const TSharedRef<SSlaterView>& View, const TCHAR* Id) const
	{
		const TSharedPtr<SWidget> Widget = View->FindById(Id);
		return (Widget.IsValid() && Widget->GetType() == FName(TEXT("SSlaterButton"))) ? StaticCastSharedPtr<SSlaterButton>(Widget) : nullptr;
	}

	TEST_METHOD(AButton_IsBuiltFromMarkup_AndTakesKeyboardFocus)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		ASSERT_THAT(IsTrue(A.IsValid()));
		ASSERT_THAT(IsTrue(A->HasAction()));
		ASSERT_THAT(IsTrue(A->SupportsKeyboardFocus()));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Rest));
		// The face is a surface of the role, around the child.
		ASSERT_THAT(IsTrue(A->GetFace()->GetRole() == FName(TEXT("UI.Surface.Button"))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(A.ToSharedRef())[0] == TEXT("A")));
	}

	TEST_METHOD(AConfirmKey_RunsTheCommandOnce_WithItsArgument)
	{
		ASSERT_THAT(IsTrue(FSlateApplication::IsInitialized()));
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		ASSERT_THAT(IsTrue(A.IsValid()));
		ASSERT_THAT(IsTrue(BtTap(*A, EKeys::Enter).IsEventHandled()));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(LastArgument == TEXT("Sword")));
		BtTap(*A, EKeys::SpaceBar);
		ASSERT_THAT(AreEqual(Fired, 2));
		BtTap(*A, EKeys::Gamepad_FaceButton_Bottom);
		ASSERT_THAT(AreEqual(Fired, 3));
	}

	TEST_METHOD(AnOtherKey_DoesNothing)
	{
		ASSERT_THAT(IsTrue(FSlateApplication::IsInitialized()));
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		ASSERT_THAT(IsTrue(A.IsValid()));
		BtTap(*A, EKeys::A);
		ASSERT_THAT(AreEqual(Fired, 0));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Rest));
	}

	TEST_METHOD(AConfirmKey_AlsoSetsTheViewState)
	{
		ASSERT_THAT(IsTrue(FSlateApplication::IsInitialized()));
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> B = Button(View, TEXT("B"));
		ASSERT_THAT(IsTrue(B.IsValid()));
		BtTap(*B, EKeys::Enter);
		FString Tab;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("tab"), Tab)));
		ASSERT_THAT(IsTrue(Tab == TEXT("map")));
	}

	TEST_METHOD(Focus_MovesBetweenTwoButtons_AndShowsTheFocusedLook)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		const TSharedPtr<SSlaterButton> B = Button(View, TEXT("B"));
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		const FSlaterSurface RestSurface = A->GetFace()->GetSurface();
		const FGeometry Geometry;

		A->OnFocusReceived(Geometry, BtFocus(EFocusCause::Navigation));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Focused));
		ASSERT_THAT(IsTrue(B->GetLook() == ESlaterButtonLook::Rest));
		ASSERT_THAT(IsFalse(A->GetFace()->GetSurface() == RestSurface));

		// Navigation takes focus from A and gives it to B.
		A->OnFocusLost(BtFocus(EFocusCause::Navigation));
		B->OnFocusReceived(Geometry, BtFocus(EFocusCause::Navigation));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Rest));
		ASSERT_THAT(IsTrue(A->GetFace()->GetSurface() == RestSurface));
		ASSERT_THAT(IsTrue(B->GetLook() == ESlaterButtonLook::Focused));

		// The confirm key goes to the focused button's action, not the other's.
		BtTap(*B, EKeys::Enter);
		ASSERT_THAT(AreEqual(Fired, 0));
	}

	TEST_METHOD(FocusFromAClick_LeavesNoFocusedLook)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		ASSERT_THAT(IsTrue(A.IsValid()));
		A->OnFocusReceived(FGeometry(), BtFocus(EFocusCause::Mouse));
		ASSERT_THAT(IsFalse(A->HasNavigationFocus()));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Rest));
	}

	TEST_METHOD(HoverAndPress_ChangeTheLook_AndPressedWinsOverFocus)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> A = Button(View, TEXT("A"));
		ASSERT_THAT(IsTrue(A.IsValid()));
		const FGeometry Geometry;
		A->OnMouseEnter(Geometry, BtLeftMouse());
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Hover));
		A->OnFocusReceived(Geometry, BtFocus(EFocusCause::Navigation));
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Focused));
		A->OnMouseButtonDown(Geometry, BtLeftMouse());
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Pressed));
		A->OnMouseLeave(BtLeftMouse());
		A->OnFocusLost(BtFocus(EFocusCause::Navigation));
		A->OnMouseButtonUp(Geometry, BtLeftMouse());
		ASSERT_THAT(IsTrue(A->GetLook() == ESlaterButtonLook::Rest));
	}

	TEST_METHOD(ADisabledButton_RefusesKeysAndActivation_AndLooksDisabled)
	{
		ASSERT_THAT(IsTrue(FSlateApplication::IsInitialized()));
		const TSharedRef<SSlaterView> View = MakeView();
		const TSharedPtr<SSlaterButton> Off = Button(View, TEXT("Off"));
		ASSERT_THAT(IsTrue(Off.IsValid()));
		ASSERT_THAT(IsFalse(Off->IsEnabled()));
		ASSERT_THAT(IsFalse(BtTap(*Off, EKeys::Enter).IsEventHandled()));
		ASSERT_THAT(IsFalse(BtTap(*Off, EKeys::Gamepad_FaceButton_Bottom).IsEventHandled()));
		ASSERT_THAT(IsFalse(Off->Activate()));
		ASSERT_THAT(AreEqual(Fired, 0));
		Off->UpdateLook();
		ASSERT_THAT(IsTrue(Off->GetLook() == ESlaterButtonLook::Disabled));
		// Enabling it again makes it work.
		Off->SetEnabled(true);
		Off->UpdateLook();
		ASSERT_THAT(IsTrue(Off->GetLook() == ESlaterButtonLook::Rest));
		BtTap(*Off, EKeys::Enter);
		ASSERT_THAT(AreEqual(Fired, 1));
	}

	TEST_METHOD(ASurfaceWithOnClick_StillTakesClicks)
	{
		Fix.Write(TEXT("BtSurface.sltr"), TEXT("<vbox><surface id=\"S\" role=\"UI.Surface.Button\" on-click=\"Test.Use\"><text>S</text></surface></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("BtSurface"));
		View->GetCommands().Register(TEXT("Test.Use"), [this](const FSlaterCommandCall&) { ++Fired; });
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("S")));
		ASSERT_THAT(IsTrue(Surface.IsValid()));
		ASSERT_THAT(IsTrue(Surface->Activate()));
		ASSERT_THAT(AreEqual(Fired, 1));
		// The surface is not a keyboard stop: only the pointer reaches it.
		ASSERT_THAT(IsFalse(Surface->SupportsKeyboardFocus()));
	}

	TEST_METHOD(AButtonWithTwoChildren_OrNoRole_IsRefusedWithItsLine)
	{
		FSlaterBuildResult Result;
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Slater::BuildLayoutFromText(TEXT("<vbox>\n<button role=\"UI.Surface.Button\"><text>a</text><text>b</text></button>\n</vbox>"), TEXT("Layouts/Test.sltr"), Stack, Result);
		ASSERT_THAT(IsTrue(Result.Errors.Num() > 0));
		ASSERT_THAT(AreEqual(Result.Errors[0].Line, 2));

		FSlaterBuildResult NoRole;
		Slater::BuildLayoutFromText(TEXT("<vbox><button><text>a</text></button></vbox>"), TEXT("Layouts/Test.sltr"), Stack, NoRole);
		ASSERT_THAT(IsTrue(NoRole.Errors.Num() > 0));
		ASSERT_THAT(IsTrue(NoRole.Errors[0].Message.Contains(TEXT("role"))));
	}

	TEST_METHOD(AnUndeclaredState_InSetState_IsRefused)
	{
		FSlaterBuildResult Result;
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		Slater::BuildLayoutFromText(TEXT("<vbox><button role=\"UI.Surface.Button\" set-state=\"tab=map\"><text>a</text></button></vbox>"), TEXT("Layouts/Test.sltr"), Stack, Result);
		ASSERT_THAT(IsFalse(Result.Succeeded()));
		ASSERT_THAT(IsTrue(Result.Errors[0].Message.Contains(TEXT("'tab' is not a declared state"))));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
