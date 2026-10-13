// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterImages.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Every visible widget of one type under Widget, in tree order; a collapsed widget hides its subtree. */
	void SbCollect(SWidget& Widget, const FName Type, TArray<SWidget*>& Out)
	{
		if (Widget.GetVisibility() == EVisibility::Collapsed)
		{
			return;
		}
		if (Widget.GetType() == Type)
		{
			Out.Add(&Widget);
		}
		Widget.GetChildren()->ForEachWidget([&](SWidget& Child) { SbCollect(Child, Type, Out); });
	}
}

/** The collapsible sidebar sample (T-403): `Layouts/Sidebar.sltr` with its SVG icons from `Content/UI/Icons`. */
TEST_CLASS(SlaterSidebarTests, "BRS.SlaterUI.Sidebar")
{
	BEFORE_EACH()
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
	}

	AFTER_EACH()
	{
		Slater::SetImageSource(nullptr);
	}

	TSharedRef<SSlaterView> MakeView() const
	{
		return SNew(SSlaterView).LayoutId(TEXT("Sidebar"));
	}

	float DesiredWidth(const TSharedRef<SSlaterView>& View) const
	{
		View->SlatePrepass(1.f);
		return View->GetDesiredSize().X;
	}

	float DesiredHeight(const TSharedRef<SSlaterView>& View) const
	{
		View->SlatePrepass(1.f);
		return View->GetDesiredSize().Y;
	}

	TArray<FString> VisibleTexts(const TSharedRef<SSlaterView>& View) const
	{
		TArray<SWidget*> Blocks;
		SbCollect(*View, FName(TEXT("STextBlock")), Blocks);
		TArray<FString> Texts;
		for (SWidget* Block : Blocks)
		{
			Texts.Add(static_cast<STextBlock*>(Block)->GetText().ToString());
		}
		return Texts;
	}

	int32 VisibleImages(const TSharedRef<SSlaterView>& View) const
	{
		TArray<SWidget*> Images;
		SbCollect(*View, FName(TEXT("SImage")), Images);
		return Images.Num();
	}

	bool ClickHamburger(const TSharedRef<SSlaterView>& View) const
	{
		const TSharedPtr<SSlaterSurface> Surface = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("Hamburger")));
		return Surface.IsValid() && Surface->Activate();
	}

	TEST_METHOD(TheShippedLayout_BuildsWithEveryIconInTheSource)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		for (const FSlaterError& Error : View->GetResult().Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsFalse(View->HasErrors()));
		FString Expanded;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("expanded"), Expanded)));
		ASSERT_THAT(IsTrue(Expanded == TEXT("true")));

		// The eight images (hamburger, five items, logout, and the logo at the right of the header) exist, and the source maps every name the layout uses to an SVG brush.
		TArray<SWidget*> Images;
		SbCollect(*View, FName(TEXT("SImage")), Images);
		ASSERT_THAT(AreEqual(Images.Num(), 8));
		for (const TCHAR* Name : {TEXT("Logo"), TEXT("Menu"), TEXT("Home"), TEXT("User"), TEXT("Mail"), TEXT("Folder"), TEXT("Settings"), TEXT("Logout")})
		{
			const FSlateBrush* Brush = Slater::FindImage(FName(Name, FNAME_Find));
			ASSERT_THAT(IsNotNull(Brush));
			ASSERT_THAT(IsTrue(Brush->GetImageType() == ESlateBrushImageType::Vector));
		}
	}

	TEST_METHOD(TheHamburger_CollapsesToARail_AndBack)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const float Wide = DesiredWidth(View);
		const float Tall = DesiredHeight(View);

		ASSERT_THAT(IsTrue(ClickHamburger(View)));
		FString State;
		View->GetState(TEXT("expanded"), State);
		ASSERT_THAT(IsTrue(State == TEXT("false")));
		const float Rail = DesiredWidth(View);
		ASSERT_THAT(IsTrue(Rail < Wide - 100.f));
		// A rail is an icon and its padding, not the header gap.
		ASSERT_THAT(IsTrue(Rail < 80.f));
		// Only the width changes: the rail is as tall as the open panel.
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DesiredHeight(View), Tall)));

		ASSERT_THAT(IsTrue(ClickHamburger(View)));
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(DesiredWidth(View), Wide)));
	}

	TEST_METHOD(Collapsing_HidesLabelsTitleAndLogo_KeepsIcons)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		const TArray<FString> Open = VisibleTexts(View);
		for (const TCHAR* Label : {TEXT("Skybrook"), TEXT("Home"), TEXT("Profile"), TEXT("Messages"), TEXT("Files"), TEXT("Settings"), TEXT("Log out")})
		{
			ASSERT_THAT(IsTrue(Open.Contains(Label)));
		}
		ASSERT_THAT(AreEqual(VisibleImages(View), 8));

		ASSERT_THAT(IsTrue(ClickHamburger(View)));
		ASSERT_THAT(IsTrue(VisibleTexts(View).IsEmpty()));
		// The hamburger and the six item icons stay; the logo goes with the title.
		ASSERT_THAT(AreEqual(VisibleImages(View), 7));
		const TSharedPtr<SWidget> Logo = View->FindById(TEXT("Logo"));
		ASSERT_THAT(IsTrue(Logo.IsValid() && Logo->GetVisibility() == EVisibility::Collapsed));
		const TSharedPtr<SWidget> Title = View->FindById(TEXT("Title"));
		ASSERT_THAT(IsTrue(Title.IsValid() && Title->GetVisibility() == EVisibility::Collapsed));
	}
};

#endif
