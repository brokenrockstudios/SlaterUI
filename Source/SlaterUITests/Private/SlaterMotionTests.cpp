// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "SlaterBindTestKit.h"
#include "SlaterMotion.h"
#include "SlaterState.h"
#include "SlaterSurface.h"
#include "SlaterView.h"

/** The animator and `transition` (T-406): a size that follows a view state over time, driven by a manual clock. */
TEST_CLASS(SlaterMotionTests, "BRS.SlaterUI.Motion")
{
	FSlaterBindFixture Fix;
	FSlaterStyleStack Stack;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
	}

	AFTER_EACH()
	{
		SetScale(1.f);
		Fix.Shutdown();
	}

	void SetScale(float Value) const
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Slater.MotionScale")))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	static const TCHAR* Layout()
	{
		return TEXT("<vbox><state name=\"e\" default=\"true\"/>"
			"<surface id=\"Toggle\" role=\"UI.Surface.Button\" on-click=\"ui.toggle:e\"><text>t</text></surface>"
			"<text id=\"Panel\" width-by=\"e: true=200, false=60\" transition=\"UI.Motion.Panel\">label</text></vbox>");
	}

	TSharedRef<SSlaterView> MakeView(const TCHAR* Text)
	{
		Fix.Write(TEXT("MoTest.sltr"), Text);
		return SNew(SSlaterView).LayoutId(TEXT("MoTest"));
	}

	float Width(const TSharedRef<SSlaterView>& View) const
	{
		View->SlatePrepass(1.f);
		const TSharedPtr<SWidget> Widget = View->FindById(TEXT("Panel"));
		return Widget.IsValid() ? static_cast<float>(Widget->GetDesiredSize().X) : -1.f;
	}

	bool Click(const TSharedRef<SSlaterView>& View) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("Toggle")));
		return Surface.IsValid() && Surface->Activate();
	}

	TEST_METHOD(Animator_StartsMidpointEnd_Linear)
	{
		FSlaterAnimator Animator;
		Animator.Snap(10.f);
		ASSERT_THAT(IsFalse(Animator.IsActive()));
		Animator.Start(30.f, 1.f, ECurveEaseFunction::Linear);
		ASSERT_THAT(IsTrue(Animator.IsActive()));
		ASSERT_THAT(IsNear(Animator.GetValue(), 10.f, 0.001f));
		ASSERT_THAT(IsTrue(Animator.Advance(0.5f)));
		ASSERT_THAT(IsNear(Animator.GetValue(), 20.f, 0.001f));
		ASSERT_THAT(IsFalse(Animator.Advance(0.5f)));
		ASSERT_THAT(IsNear(Animator.GetValue(), 30.f, 0.001f));
		ASSERT_THAT(IsFalse(Animator.IsActive()));
	}

	TEST_METHOD(Animator_Retarget_ContinuesFromTheCurrentValue)
	{
		FSlaterAnimator Animator;
		Animator.Snap(0.f);
		Animator.Start(100.f, 1.f, ECurveEaseFunction::Linear);
		Animator.Advance(0.5f);
		Animator.Start(0.f, 1.f, ECurveEaseFunction::Linear);
		// No jump: the new move starts at 50, where the old one was.
		ASSERT_THAT(IsNear(Animator.GetValue(), 50.f, 0.001f));
		Animator.Advance(0.5f);
		ASSERT_THAT(IsNear(Animator.GetValue(), 25.f, 0.001f));
		Animator.Advance(0.5f);
		ASSERT_THAT(IsNear(Animator.GetValue(), 0.f, 0.001f));
	}

	TEST_METHOD(Animator_ZeroDurationAndNoStartValue_Snap)
	{
		FSlaterAnimator Animator;
		Animator.Start(40.f, 1.f, ECurveEaseFunction::Linear);
		ASSERT_THAT(IsFalse(Animator.IsActive()));
		ASSERT_THAT(IsNear(Animator.GetValue(), 40.f, 0.001f));
		Animator.Start(80.f, 0.f, ECurveEaseFunction::Linear);
		ASSERT_THAT(IsFalse(Animator.IsActive()));
		ASSERT_THAT(IsNear(Animator.GetValue(), 80.f, 0.001f));
	}

	TEST_METHOD(EveryEaseWordOfTheStyle_MapsToAnEngineEase)
	{
		for (const FString& Word : Slater::EaseWords())
		{
			ECurveEaseFunction Ease;
			ASSERT_THAT(IsTrue(Slater::EaseFromWord(FName(*Word), Ease)));
		}
		ECurveEaseFunction Ease;
		ASSERT_THAT(IsFalse(Slater::EaseFromWord(TEXT("Bounce"), Ease)));
	}

	TEST_METHOD(ATransition_MovesTheWidth_StartMidpointEnd)
	{
		const TSharedRef<SSlaterView> View = MakeView(Layout());
		ASSERT_THAT(IsFalse(View->HasErrors()));
		// Opens snapped to its state, nothing moving, no timer needed.
		ASSERT_THAT(IsNear(Width(View), 200.f, 0.01f));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		ASSERT_THAT(IsTrue(Click(View)));
		ASSERT_THAT(IsTrue(View->IsMoving()));
		ASSERT_THAT(IsNear(Width(View), 200.f, 0.01f));
		ASSERT_THAT(IsTrue(View->AdvanceMotion(0.05f)));
		const float Mid = Width(View);
		ASSERT_THAT(IsTrue(Mid < 199.f && Mid > 61.f));
		ASSERT_THAT(IsFalse(View->AdvanceMotion(1.f)));
		ASSERT_THAT(IsNear(Width(View), 60.f, 0.01f));
		ASSERT_THAT(IsFalse(View->IsMoving()));
	}

	TEST_METHOD(AToggleMidFlight_ContinuesFromTheCurrentWidth)
	{
		const TSharedRef<SSlaterView> View = MakeView(Layout());
		ASSERT_THAT(IsTrue(Click(View)));
		View->AdvanceMotion(0.05f);
		const float Mid = Width(View);
		ASSERT_THAT(IsTrue(Click(View)));
		ASSERT_THAT(IsNear(Width(View), Mid, 0.01f));
		ASSERT_THAT(IsTrue(View->IsMoving()));
		View->AdvanceMotion(1.f);
		ASSERT_THAT(IsNear(Width(View), 200.f, 0.01f));
	}

	TEST_METHOD(ARebuild_SnapsToTheKeptState_NoAnimation)
	{
		const TSharedRef<SSlaterView> View = MakeView(Layout());
		ASSERT_THAT(IsTrue(Click(View)));
		View->AdvanceMotion(1.f);
		ASSERT_THAT(IsTrue(View->Reload()));
		ASSERT_THAT(IsNear(Width(View), 60.f, 0.01f));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		// A restyle snaps too.
		ASSERT_THAT(IsTrue(Click(View)));
		ASSERT_THAT(IsTrue(View->SetStyle(TEXT("Dusk"))));
		ASSERT_THAT(IsNear(Width(View), 200.f, 0.01f));
		ASSERT_THAT(IsFalse(View->IsMoving()));
	}

	TEST_METHOD(MotionScaleZero_IsInstant)
	{
		SetScale(0.f);
		const TSharedRef<SSlaterView> View = MakeView(Layout());
		ASSERT_THAT(IsTrue(Click(View)));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		ASSERT_THAT(IsNear(Width(View), 60.f, 0.01f));
	}

	TEST_METHOD(WithoutATransition_TheSizeJumps)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><state name=\"e\" default=\"true\"/>"
			"<surface id=\"Toggle\" role=\"UI.Surface.Button\" on-click=\"ui.toggle:e\"><text>t</text></surface>"
			"<text id=\"Panel\" width-by=\"e: true=200, false=60\">label</text></vbox>"));
		ASSERT_THAT(IsTrue(Click(View)));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		ASSERT_THAT(IsNear(Width(View), 60.f, 0.01f));
	}

	TEST_METHOD(ACaseWithNoValue_ReturnsToContentSize_AndNextMoveSnaps)
	{
		const TSharedRef<SSlaterView> View = MakeView(TEXT(
			"<vbox><state name=\"m\" default=\"a\"/>"
			"<surface id=\"ToB\" role=\"UI.Surface.Button\" set-state=\"m=b\"><text>t</text></surface>"
			"<surface id=\"ToC\" role=\"UI.Surface.Button\" set-state=\"m=c\"><text>t</text></surface>"
			"<text id=\"Panel\" width-by=\"m: a=200, c=90\" transition=\"UI.Motion.Panel\">x</text></vbox>"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterSurface> ToB = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("ToB")));
		const TSharedPtr<SSlaterSurface> ToC = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("ToC")));
		ASSERT_THAT(IsTrue(ToB->Activate()));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		ASSERT_THAT(IsTrue(Width(View) < 100.f));
		ASSERT_THAT(IsTrue(ToC->Activate()));
		ASSERT_THAT(IsFalse(View->IsMoving()));
		ASSERT_THAT(IsNear(Width(View), 90.f, 0.01f));
	}

	TEST_METHOD(BadTransitions_AreRefusedAtTheAttribute)
	{
		auto Build = [&](const TCHAR* Body)
		{
			FSlaterBuildResult Result;
			Slater::BuildLayoutFromText(FString::Printf(TEXT("<vbox>\n<state name=\"e\" default=\"true\"/>\n%s\n</vbox>"), Body), TEXT("Layouts/Test.sltr"), Stack, Result);
			return Result;
		};
		const FSlaterBuildResult Unknown = Build(TEXT("<text width-by=\"e: true=1\" transition=\"UI.Motion.Nope\">x</text>"));
		ASSERT_THAT(IsFalse(Unknown.Succeeded()));
		ASSERT_THAT(IsTrue(Unknown.Errors[0].Message.Contains(TEXT("unknown motion token 'UI.Motion.Nope'"))));
		ASSERT_THAT(AreEqual(Unknown.Errors[0].Line, 3));
		const FSlaterBuildResult Alone = Build(TEXT("<text transition=\"UI.Motion.Panel\">x</text>"));
		ASSERT_THAT(IsFalse(Alone.Succeeded()));
		ASSERT_THAT(IsTrue(Alone.Errors[0].Message.Contains(TEXT("nothing to animate"))));
		ASSERT_THAT(IsFalse(Build(TEXT("<text width-by=\"e: true=1\" transition=\"not a token\">x</text>")).Succeeded()));
		ASSERT_THAT(IsTrue(Build(TEXT("<text width-by=\"e: true=1\" transition=\"UI.Motion.Panel\">x</text>")).Succeeded()));
	}
};

#endif
