// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterImages.h"
#include "SlaterStyle.h"
#include "SlaterSurface.h"
#include "SlaterView.h"
#include "Widgets/Images/SImage.h"

namespace SlaterAccountTestsPrivate
{
	/** SImage keeps its brush and tint in protected attributes; read them through a derived view (adds no data, nothing is constructed). */
	struct FAccountPeekImage : SImage
	{
		static const FSlateBrush* BrushOf(const SWidget& Widget)
		{
			return static_cast<const FAccountPeekImage&>(Widget).GetImageAttribute().Get();
		}
		static FLinearColor TintOf(const SWidget& Widget)
		{
			return static_cast<const FAccountPeekImage&>(Widget).GetColorAndOpacityAttribute().Get().GetSpecifiedColor();
		}
	};
}
using namespace SlaterAccountTestsPrivate;

/** The account dashboard sample (T-408): `Layouts/Account.sltr` in the `Skyline` style, with `Account.mock.json` bound. */
TEST_CLASS(SlaterAccountTests, "BRS.SlaterUI.Account")
{
	BEFORE_EACH()
	{
		Slater::SetImageSource(Slater::MakePluginIconSource());
	}

	AFTER_EACH()
	{
		Slater::SetImageSource(nullptr);
	}

	TSharedRef<SSlaterView> MakeView(const TCHAR* Style = TEXT("Skyline")) const
	{
		return SNew(SSlaterView).LayoutId(TEXT("Account")).StyleName(Style).UseMockContext(true);
	}

	TEST_METHOD(TheShippedLayout_BuildsInSkyline_WithItsMockDataBound)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		for (const FSlaterError& Error : View->GetResult().Errors)
		{
			AddError(Error.ToString());
		}
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsTrue(View->GetActiveContext().IsValid()));

		const TArray<FString> Texts = SlaterBindKit::Texts(View);
		for (const TCHAR* Expected : {TEXT("Tom Gorman"), TEXT("@tom.gorman"), TEXT("14 Mar 2019"), TEXT("US"), TEXT("1,284"), TEXT("4%"), TEXT("Handbook"), TEXT("Servers"), TEXT("Rewards"), TEXT("Subscribers"), TEXT("Open dossier")})
		{
			if (!Texts.Contains(Expected))
			{
				AddError(FString::Printf(TEXT("text '%s' is not on the page"), Expected));
			}
		}
	}

	TEST_METHOD(TheCreditRows_FollowTheMockList)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SSlaterRepeat> Credits = SlaterBindKit::Repeat(View, 0);
		ASSERT_THAT(IsTrue(Credits.IsValid()));
		ASSERT_THAT(AreEqual(Credits->NumItems(), 3));
		const TArray<FString> Texts = SlaterBindKit::Texts(View);
		for (const TCHAR* Expected : {TEXT("Level design"), TEXT("Skybrook"), TEXT("Sound"), TEXT("Hollow Quarry"), TEXT("Community"), TEXT("Broken Rock")})
		{
			if (!Texts.Contains(Expected))
			{
				AddError(FString::Printf(TEXT("credit text '%s' is not on the page"), Expected));
			}
		}
	}

	TEST_METHOD(EveryIconTheLayoutNames_IsInTheSource)
	{
		for (const TCHAR* Name : {TEXT("Frog"), TEXT("Book"), TEXT("Globe"), TEXT("Gift"), TEXT("External")})
		{
			if (Slater::FindImage(FName(Name, FNAME_Find)) == nullptr)
			{
				AddError(FString::Printf(TEXT("icon '%s' is not in the plugin icon source"), Name));
			}
		}
	}

	TEST_METHOD(TheCards_TakeHover_AndTheDossierButtonFiresItsCommand)
	{
		const TSharedRef<SSlaterView> View = MakeView();
		ASSERT_THAT(IsFalse(View->HasErrors()));
		int32 Fired = 0;
		View->GetCommands().Register(TEXT("Account.Dossier"), [&Fired](const FSlaterCommandCall&) { ++Fired; });
		const TSharedPtr<SSlaterSurface> Dossier = StaticCastSharedPtr<SSlaterSurface>(View->FindById(TEXT("Dossier")));
		ASSERT_THAT(IsTrue(Dossier.IsValid() && Dossier->Activate()));
		ASSERT_THAT(AreEqual(Fired, 1));
		// The wide cards light on hover without being buttons.
		for (const TCHAR* Id : {TEXT("Subscribers"), TEXT("Progress")})
		{
			ASSERT_THAT(IsTrue(View->FindById(Id).IsValid()));
		}
	}

	TEST_METHOD(TheWhiteLineIcons_TakeTheStyleColors_AndTheAvatarKeepsItsOwn)
	{
		// The line icons are white SVGs: untinted they vanish on Base's white cards (T-544).
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FLinearColor Text, Accent;
		ASSERT_THAT(IsTrue(Stack.FindColor(TEXT("UI.Color.Text"), Text) && Stack.FindColor(TEXT("UI.Color.Accent"), Accent)));
		const TMap<FString, FLinearColor> Expected = {
			{TEXT("Book"), Text}, {TEXT("Globe"), Text}, {TEXT("Gift"), Text}, {TEXT("External"), Accent}, {TEXT("Frog"), FLinearColor::White}};

		const TSharedRef<SSlaterView> View = MakeView(TEXT("Base"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		TMap<FString, int32> Seen;
		TArray<TSharedRef<SWidget>> Pending = {View};
		while (!Pending.IsEmpty())
		{
			const TSharedRef<SWidget> Widget = Pending.Pop();
			Widget->GetChildren()->ForEachWidget([&](SWidget& Child) { Pending.Add(Child.AsShared()); });
			if (Widget->GetType() != FName(TEXT("SImage")))
			{
				continue;
			}
						for (const TPair<FString, FLinearColor>& Pair : Expected)
			{
				if (FAccountPeekImage::BrushOf(*Widget) == Slater::FindImage(FName(*Pair.Key, FNAME_Find)))
				{
					++Seen.FindOrAdd(Pair.Key);
					const FLinearColor Tint = FAccountPeekImage::TintOf(*Widget);
					if (!Tint.Equals(Pair.Value))
					{
						AddError(FString::Printf(TEXT("icon '%s' is tinted %s, expected %s"), *Pair.Key, *Tint.ToString(), *Pair.Value.ToString()));
					}
				}
			}
		}
		// Each card icon once, the External arrow on all three cards, one avatar.
		ASSERT_THAT(IsTrue(Seen.FindRef(TEXT("Book")) == 1 && Seen.FindRef(TEXT("Globe")) == 1 && Seen.FindRef(TEXT("Gift")) == 1));
		ASSERT_THAT(IsTrue(Seen.FindRef(TEXT("External")) == 3 && Seen.FindRef(TEXT("Frog")) == 1));
	}

	TEST_METHOD(TheLayout_AlsoBuildsInBaseAndDusk)
	{
		for (const TCHAR* Style : {TEXT("Base"), TEXT("Dusk")})
		{
			const TSharedRef<SSlaterView> View = MakeView(Style);
			for (const FSlaterError& Error : View->GetResult().Errors)
			{
				AddError(FString::Printf(TEXT("%s: %s"), Style, *Error.ToString()));
			}
			ASSERT_THAT(IsFalse(View->HasErrors()));
		}
	}
};

#endif
