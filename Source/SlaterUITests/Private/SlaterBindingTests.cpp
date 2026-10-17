// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SlaterBindTestKit.h"
#include "SlaterBindings.h"
#include "SlaterList.h"
#include "SlaterObjectScope.h"
#include "SlaterScope.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Text/STextBlock.h"

/** `{Path}` against a UObject view model: FieldNotify pushes, links that are replaced, paths that fail at load. */
TEST_CLASS(SlaterViewModelBindingTests, "BRS.SlaterUI.Bind.ViewModel")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::ListStats().Reset();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TEST_METHOD(AFieldNotifyChange_UpdatesTheBoundText)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("Hello");
		Root->Player = Fix.MakePlayer(TEXT("Ann"));
		Fix.Write(TEXT("VbText.sltr"), TEXT("<vbox><text>{Title}</text><text>{Player.Name}</text><text>{Player.Health}</text><text>{Player.Ratio}</text></vbox>"));

		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbText"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(AreEqual(Texts.Num(), 4));
		ASSERT_THAT(IsTrue(Texts[0] == TEXT("Hello") && Texts[1] == TEXT("Ann") && Texts[2] == TEXT("100") && Texts[3] == TEXT("0.5")));

		Slater::ListStats().Reset();
		Root->SetTitle(TEXT("Bye"));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Bye")));
		Root->Player->SetHealth(40);
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 2));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[2] == TEXT("40")));
		// Fields nothing binds apply nothing.
		Root->Player->bAlive = false;
		Root->Player->Notify(TEXT("bAlive"));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 2));
	}

	TEST_METHOD(AGetterFunction_IsReadByItsFieldName)
	{
		USlaterTestPlayerVM* Player = Fix.MakePlayer(TEXT("Ann"));
		Fix.Write(TEXT("VbGetter.sltr"), TEXT("<text>{Label}</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbGetter"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Player))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Ann (100)")));
		Player->SetName(TEXT("Bo"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Bo (100)")));
	}

	TEST_METHOD(AReplacedLink_MovesTheSubscriptionToTheNewSource)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		USlaterTestPlayerVM* First = Fix.MakePlayer(TEXT("Ann"));
		USlaterTestPlayerVM* Second = Fix.MakePlayer(TEXT("Bob"));
		Root->Player = First;
		Fix.Write(TEXT("VbLink.sltr"), TEXT("<text>{Player.Name}</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbLink"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));
		ASSERT_THAT(AreEqual(First->NumBoundDelegates(), 1));

		Root->SetPlayer(Second);
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Bob")));
		ASSERT_THAT(AreEqual(First->NumBoundDelegates(), 0));
		ASSERT_THAT(AreEqual(Second->NumBoundDelegates(), 1));

		// The old source no longer reaches the view; the new one does.
		First->SetName(TEXT("Zed"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Bob")));
		Second->SetName(TEXT("Cy"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Cy")));

		// A link set to nothing keeps the last text (the widget has nothing newer to show) and watches for the next source.
		Root->SetPlayer(nullptr);
		ASSERT_THAT(AreEqual(Second->NumBoundDelegates(), 0));
		Root->SetPlayer(First);
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Zed")));
	}

	TEST_METHOD(AnUnresolvableField_FailsTheLoadWithItsLineAndASuggestion)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Fix.Write(TEXT("VbBad.sltr"), TEXT("<vbox>\n\t<text>{Title}</text>\n\t<text>{Titel}</text>\n</vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbBad"));
		ASSERT_THAT(IsFalse(View->HasErrors()));

		ASSERT_THAT(IsFalse(View->SetContext(Slater::MakeObjectScope(Root))));
		// No good tree was built for this context before, so the old one (without data) stays, with the banner on top.
		ASSERT_THAT(IsTrue(View->HasReloadErrors()));
		const FSlaterError& Error = View->GetReloadErrors()[0];
		ASSERT_THAT(AreEqual(Error.Line, 3));
		ASSERT_THAT(IsTrue(Error.ToString().Contains(TEXT("VbBad.sltr(3,"))));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("{Titel}"))));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("did you mean 'Title'"))));
		ASSERT_THAT(AreEqual(View->GetReloadErrors().Num(), 1));
	}

	TEST_METHOD(CheckBindings_ReportsEveryBadPath_WithoutAView)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Items.Add(Fix.MakeItem(TEXT("a"), 1));
		FSlaterStyleStack Stack;
		ASSERT_THAT(IsTrue(Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack)));
		FSlaterBuildResult Result;
		const FString Text = TEXT("<vbox>\n<text>{Nope}</text>\n<text>{Title.Length}</text>\n<repeat source=\"{Itmes}\" key=\"Id\"><text>{Count}</text></repeat>\n<repeat source=\"{Items}\" key=\"Id\"><text>{Cuont}</text></repeat>\n</vbox>");
		ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(Text, TEXT("Layouts/T.sltr"), Stack, Result)));

		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsFalse(Slater::CheckBindings(Result, *Slater::MakeObjectScope(Root), TEXT("Layouts/T.sltr"), Errors)));
		ASSERT_THAT(AreEqual(Errors.Num(), 4));
		ASSERT_THAT(IsTrue(Errors[0].Line == 2 && Errors[0].Message.Contains(TEXT("'Nope' is not a field of SlaterTestRootVM"))));
		ASSERT_THAT(IsTrue(Errors[1].Line == 3 && Errors[1].Message.Contains(TEXT("not an object"))));
		// A repeat source and the paths inside its template are checked too (the template against the list's first item).
		ASSERT_THAT(IsTrue(Errors[2].Line == 4 && Errors[2].Message.Contains(TEXT("<repeat> cannot use {Itmes}")) && Errors[2].Message.Contains(TEXT("did you mean 'Items'"))));
		ASSERT_THAT(IsTrue(Errors[3].Line == 5 && Errors[3].Message.Contains(TEXT("{Cuont}")) && Errors[3].Message.Contains(TEXT("did you mean 'Count'"))));
	}

	TEST_METHOD(ASourceThatIsNotAList_IsRefused)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(TEXT("<repeat source=\"{Title}\" key=\"Id\"><text>x</text></repeat>"), TEXT("Layouts/T.sltr"), Stack, Result)));
		TArray<FSlaterError> Errors;
		ASSERT_THAT(IsFalse(Slater::CheckBindings(Result, *Slater::MakeObjectScope(Root), TEXT("Layouts/T.sltr"), Errors)));
		ASSERT_THAT(IsTrue(Errors[0].Message.Contains(TEXT("not a list of objects"))));
	}

	TEST_METHOD(ASilentChange_IsListedByTheStaleBindingReport)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Player = Fix.MakePlayer(TEXT("Ann"));
		Fix.Write(TEXT("VbStale.sltr"), TEXT("<text>{Player.Name}</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbStale"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));

		auto StaleLines = []()
		{
			TArray<FString> Lines;
			Slater::CollectStaleBindings(Lines);
			return Lines.FilterByPredicate([](const FString& Line) { return Line.Contains(TEXT("VbStale.sltr")); });
		};
		ASSERT_THAT(AreEqual(StaleLines().Num(), 0));

		// A view model that changes a field and forgets to broadcast: the text is stale and nothing says so.
		Root->Player->Name = TEXT("Silent");
		const TArray<FString> Stale = StaleLines();
		ASSERT_THAT(AreEqual(Stale.Num(), 1));
		ASSERT_THAT(IsTrue(Stale[0].Contains(TEXT("{Player.Name}")) && Stale[0].Contains(TEXT("shows 'Ann'")) && Stale[0].Contains(TEXT("'Silent'"))));
		ASSERT_THAT(IsTrue(Stale[0].Contains(TEXT("VbStale.sltr(1,"))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Ann")));

		Root->Player->Notify(TEXT("Name"));
		ASSERT_THAT(AreEqual(StaleLines().Num(), 0));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Silent")));
	}

	TEST_METHOD(AThousandViewsCreatedAndDestroyed_LeaveNoDelegatesBehind)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Title = TEXT("T");
		Root->Player = Fix.MakePlayer(TEXT("Ann"));
		for (int32 I = 0; I < 3; ++I)
		{
			Root->Items.Add(Fix.MakeItem(*FString::Printf(TEXT("i%d"), I), I));
		}
		Fix.Write(TEXT("VbMany.sltr"), TEXT("<vbox><text>{Title}</text><text>{Player.Name}</text><repeat source=\"{Items}\" key=\"Id\" columns=\"1\"><text>{Count}</text></repeat></vbox>"));
		const bool bContentBound = Slater::OnContentChanged().IsBound();

		for (int32 Round = 0; Round < 1000; ++Round)
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbMany"));
			ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));
			if (Round == 0)
			{
				// While the view is alive, the root has the title, the link and the list; each item has its count.
				ASSERT_THAT(AreEqual(Root->NumBoundDelegates(), 3));
				ASSERT_THAT(AreEqual(Root->Player->NumBoundDelegates(), 1));
				ASSERT_THAT(AreEqual(Root->Items[0]->NumBoundDelegates(), 1));
			}
		}

		ASSERT_THAT(AreEqual(Root->NumBoundDelegates(), 0));
		ASSERT_THAT(AreEqual(Root->Player->NumBoundDelegates(), 0));
		for (const TObjectPtr<USlaterTestItemVM>& Item : Root->Items)
		{
			ASSERT_THAT(AreEqual(Item->NumBoundDelegates(), 0));
		}
		ASSERT_THAT(IsTrue(Slater::OnContentChanged().IsBound() == bContentBound));
		TArray<FString> Stale;
		Slater::CollectStaleBindings(Stale);
		ASSERT_THAT(AreEqual(Stale.Num(), 0));
	}

	TEST_METHOD(AThousandViewsOverTheMockModel_LeaveNoModelDelegatesBehind)
	{
		Fix.Write(TEXT("VbMock.sltr"), TEXT("<vbox><text>{Title}</text><repeat source=\"{L}\" key=\"Id\" columns=\"1\"><text>{N}</text></repeat></vbox>"));
		const TSharedRef<FSlaterModel> Model = MakeShared<FSlaterModel>();
		FSlaterError LoadError;
		ASSERT_THAT(IsTrue(Model->LoadMock(TEXT("{\"Title\":\"x\",\"L\":[{\"Id\":\"a\",\"N\":1},{\"Id\":\"b\",\"N\":2}]}"), TEXT("mock"), LoadError)));
		for (int32 Round = 0; Round < 1000; ++Round)
		{
			const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbMock"));
			ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeModelScope(Model))));
			if (Round == 0)
			{
				ASSERT_THAT(IsTrue(Model->OnValueChanged.IsBound() && Model->OnItemFieldChanged.IsBound() && Model->OnItemsChanged.IsBound()));
			}
		}
		ASSERT_THAT(IsFalse(Model->OnValueChanged.IsBound()));
		ASSERT_THAT(IsFalse(Model->OnItemFieldChanged.IsBound()));
		ASSERT_THAT(IsFalse(Model->OnItemsChanged.IsBound()));
	}

	TEST_METHOD(AContextChange_RebindsTheSameLayout)
	{
		USlaterTestPlayerVM* One = Fix.MakePlayer(TEXT("One"));
		USlaterTestPlayerVM* Two = Fix.MakePlayer(TEXT("Two"));
		Fix.Write(TEXT("VbSwap.sltr"), TEXT("<text>{Name}</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("VbSwap"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(One))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("One")));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Two))));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("Two")));
		ASSERT_THAT(AreEqual(One->NumBoundDelegates(), 0));
		ASSERT_THAT(AreEqual(Two->NumBoundDelegates(), 1));
		// No context: the layout shows as written, bound text empty.
		ASSERT_THAT(IsTrue(View->SetContext(nullptr)));
		ASSERT_THAT(AreEqual(Two->NumBoundDelegates(), 0));
	}
};

/** `<repeat>` over an array of view models: the list is diffed by key, an item that changes touches only itself. */
TEST_CLASS(SlaterViewModelListTests, "BRS.SlaterUI.Bind.List")
{
	FSlaterBindFixture Fix;
	USlaterTestRootVM* Root = nullptr;
	TSharedPtr<SSlaterView> View;

	BEFORE_EACH()
	{
		Fix.Init();
		Root = Fix.Make<USlaterTestRootVM>();
		Root->Items.Add(Fix.MakeItem(TEXT("a"), 1));
		Root->Items.Add(Fix.MakeItem(TEXT("b"), 2));
		Root->Items.Add(Fix.MakeItem(TEXT("c"), 3));
		Fix.Write(TEXT("VbList.sltr"), SlaterBindKit::ItemsLayout);
		View = SNew(SSlaterView).LayoutId(TEXT("VbList"));
		View->SetContext(Slater::MakeObjectScope(Root));
		Slater::ListStats().Reset();
	}

	AFTER_EACH()
	{
		View.Reset();
		Fix.Shutdown();
	}

	TEST_METHOD(EveryItemOfTheArray_GetsAnInstance)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		ASSERT_THAT(IsTrue(Repeat.IsValid()));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 3));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("a"))) == TEXT("1")));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("c"))) == TEXT("3")));
	}

	TEST_METHOD(AChangedItem_UpdatesOnlyThatItem)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		const TSharedPtr<SWidget> A = Repeat->FindItemWidget(TEXT("a"));
		const TSharedPtr<SWidget> B = Repeat->FindItemWidget(TEXT("b"));
		const TSharedPtr<SWidget> C = Repeat->FindItemWidget(TEXT("c"));

		Root->Items[1]->SetCount(20);
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 0));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(B) == TEXT("20")));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(A) == TEXT("1") && SlaterBindKit::TextOf(C) == TEXT("3")));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("b")) == B));

		// A field of an item that nothing binds touches nothing.
		Root->Items[0]->SetLocked(true);
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
	}

	TEST_METHOD(AnAddedItem_BuildsExactlyOneInstance)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		const TSharedPtr<SWidget> A = Repeat->FindItemWidget(TEXT("a"));
		Root->AddItem(Fix.MakeItem(TEXT("d"), 4));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 0));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 4));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("d"))) == TEXT("4")));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("a")) == A));
		// The new item follows its own changes too.
		Root->Items[3]->SetCount(40);
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("d"))) == TEXT("40")));
	}

	TEST_METHOD(ARemovedItem_DestroysExactlyOneInstance_AndItsDelegates)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		USlaterTestItemVM* Gone = Root->Items[0];
		const TSharedPtr<SWidget> C = Repeat->FindItemWidget(TEXT("c"));
		ASSERT_THAT(AreEqual(Gone->NumBoundDelegates(), 1));
		Root->RemoveItem(Gone);
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 2));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("a")) == nullptr));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("c")) == C));
		ASSERT_THAT(AreEqual(Gone->NumBoundDelegates(), 0));
		// What was removed no longer reaches the view.
		Gone->SetCount(99);
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
	}

	TEST_METHOD(ABroadcastWithNoDifference_TouchesNothing)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		const TSharedPtr<SWidget> B = Repeat->FindItemWidget(TEXT("b"));
		Root->Notify(TEXT("Items"));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 0));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 0));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("b")) == B));
	}

	TEST_METHOD(ADifferentObjectWithTheSameKey_ReplacesTheInstance)
	{
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		const TSharedPtr<SWidget> Old = Repeat->FindItemWidget(TEXT("b"));
		USlaterTestItemVM* Old_VM = Root->Items[1];
		Root->Items[1] = Fix.MakeItem(TEXT("b"), 7);
		Root->Notify(TEXT("Items"));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 1));
		ASSERT_THAT(IsTrue(Repeat->FindItemWidget(TEXT("b")) != Old));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("b"))) == TEXT("7")));
		ASSERT_THAT(AreEqual(Old_VM->NumBoundDelegates(), 0));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 3));
	}

	TEST_METHOD(TheRepeatSurvivesAReload_AndRebindsToTheSameData)
	{
		Root->Items[0]->SetCount(11);
		ASSERT_THAT(IsTrue(View->Refresh()));
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View.ToSharedRef());
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 3));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("a"))) == TEXT("11")));
		// The old tree took its delegates with it: one per item, not two.
		ASSERT_THAT(AreEqual(Root->Items[0]->NumBoundDelegates(), 1));
		ASSERT_THAT(AreEqual(Root->NumBoundDelegates(), 1));
	}
};

namespace
{
	/** A list source that is neither JSON nor a view model: the third implementation of the interface. */
	class FBgCountList : public ISlaterListSource
	{
	public:
		struct FItem
		{
			FString Key;
			TSharedRef<int32> Count = MakeShared<int32>(0);
			TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		};

		void Add(const FString& Key, int32 Count)
		{
			FItem& Item = Items.AddDefaulted_GetRef();
			Item.Key = Key;
			*Item.Count = Count;
			const TSharedRef<int32> Shared = Item.Count;
			Item.Scope->AddValue(TEXT("Count"), [Shared]() { return FString::FromInt(*Shared); });
			Item.Scope->AddValue(TEXT("Id"), [Key]() { return Key; });
			OnChanged.Broadcast(Key, true);
		}

		void Remove(const FString& Key)
		{
			Items.RemoveAll([&Key](const FItem& Item) { return Item.Key == Key; });
			OnChanged.Broadcast(Key, false);
		}

		void SetCount(const FString& Key, int32 Count)
		{
			FItem* Item = Items.FindByPredicate([&Key](const FItem& I) { return I.Key == Key; });
			*Item->Count = Count;
			Item->Scope->Notify(TEXT("Count"));
		}

		virtual int32 Num() const override { return Items.Num(); }
		virtual FString KeyAt(int32 Index) const override { return Items[Index].Key; }
		virtual TSharedPtr<ISlaterScope> ItemAt(int32 Index) override { return Items[Index].Scope; }
		virtual TSharedPtr<ISlaterScope> FindItem(const FString& Key) override
		{
			const FItem* Item = Items.FindByPredicate([&Key](const FItem& I) { return I.Key == Key; });
			return Item ? TSharedPtr<ISlaterScope>(Item->Scope) : nullptr;
		}

		TArray<FItem> Items;
	};
}

/** Registered getters and a hand-written list source: the scope interface without a view-model class. */
TEST_CLASS(SlaterGetterScopeTests, "BRS.SlaterUI.Bind.Getters")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
		Slater::ListStats().Reset();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TEST_METHOD(ANotifiedGetter_UpdatesTheText_AndASilentOneIsStale)
	{
		int32 Health = 10;
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddValue(TEXT("Health"), [&Health]() { return FString::FromInt(Health); });
		Fix.Write(TEXT("BgGetter.sltr"), TEXT("<text>{Health}</text>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("BgGetter"));
		ASSERT_THAT(IsTrue(View->SetContext(Scope)));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("10")));

		Slater::ListStats().Reset();
		Health = 25;
		Scope->Notify(TEXT("Health"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("25")));
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));

		Health = 30;
		TArray<FString> Stale;
		Slater::CollectStaleBindings(Stale);
		ASSERT_THAT(AreEqual(Stale.FilterByPredicate([](const FString& L) { return L.Contains(TEXT("BgGetter.sltr")); }).Num(), 1));
		Scope->Notify(TEXT("Health"));
		ASSERT_THAT(IsTrue(SlaterBindKit::Texts(View)[0] == TEXT("30")));

		// A notification after the view is gone reaches nothing and breaks nothing.
		View->SetContext(nullptr);
		Scope->Notify(TEXT("Health"));
	}

	TEST_METHOD(APathAsAPlainLiteral_RegistersReadsAndNotifiesLikeTEXT)
	{
		int32 Plain = 1;
		int32 Wide = 2;
		FString Written;
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddValue("Plain", [&Plain]() { return FString::FromInt(Plain); });
		Scope->AddValue(TEXT("Wide"), [&Wide]() { return FString::FromInt(Wide); });
		Scope->AddValue(FString(TEXT("Named")), []() { return FString(TEXT("n")); });
		Scope->AddSetter("Plain", [&Written](const FString& V) { Written = V; return true; });
		Scope->AddList("Things", MakeShared<FBgCountList>());

		FString Out;
		ASSERT_THAT(IsTrue(Scope->Read(TEXT("Plain"), Out) && Out == TEXT("1")));
		ASSERT_THAT(IsTrue(Scope->Read(TEXT("Wide"), Out) && Out == TEXT("2")));
		ASSERT_THAT(IsTrue(Scope->Read(TEXT("Named"), Out) && Out == TEXT("n")));
		FString Why;
		ASSERT_THAT(IsTrue(Scope->Check(TEXT("Things"), true, Why)));
		ASSERT_THAT(IsTrue(Scope->Write(TEXT("Plain"), TEXT("9")) && Written == TEXT("9")));

		int32 Fired = 0;
		const TUniquePtr<ISlaterSubscription> Sub = Scope->Subscribe(TEXT("Plain"), [&Fired]() { ++Fired; });
		Plain = 5;
		Scope->Notify("Plain");
		ASSERT_THAT(AreEqual(Fired, 1));
		Scope->Notify(TEXT("Plain"));
		ASSERT_THAT(AreEqual(Fired, 2));
		ASSERT_THAT(IsTrue(Scope->Read(TEXT("Plain"), Out) && Out == TEXT("5")));
	}

	TEST_METHOD(AnUnregisteredPath_FailsWithASuggestion)
	{
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddValue(TEXT("Health"), []() { return FString(TEXT("1")); });
		Fix.Write(TEXT("BgBad.sltr"), TEXT("<vbox>\n<text>{Helth}</text>\n</vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("BgBad"));
		ASSERT_THAT(IsFalse(View->SetContext(Scope)));
		ASSERT_THAT(AreEqual(View->GetReloadErrors()[0].Line, 2));
		ASSERT_THAT(IsTrue(View->GetReloadErrors()[0].Message.Contains(TEXT("did you mean 'Health'"))));
	}

	TEST_METHOD(AHandWrittenListSource_DrivesARepeat)
	{
		const TSharedRef<FBgCountList> List = MakeShared<FBgCountList>();
		List->Add(TEXT("a"), 1);
		List->Add(TEXT("b"), 2);
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddList(TEXT("Things"), List);
		Fix.Write(TEXT("BgList.sltr"), TEXT("<repeat source=\"{Things}\" key=\"Id\" columns=\"1\"><text>{Count}</text></repeat>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("BgList"));
		ASSERT_THAT(IsTrue(View->SetContext(Scope)));
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View);
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 2));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("b"))) == TEXT("2")));

		Slater::ListStats().Reset();
		List->SetCount(TEXT("b"), 20);
		ASSERT_THAT(AreEqual(Slater::ListStats().Applies, 1));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 0));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("b"))) == TEXT("20")));

		List->Add(TEXT("c"), 3);
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsBuilt, 1));
		ASSERT_THAT(IsTrue(SlaterBindKit::TextOf(Repeat->FindItemWidget(TEXT("c"))) == TEXT("3")));
		List->Remove(TEXT("a"));
		ASSERT_THAT(AreEqual(Slater::ListStats().ItemsDestroyed, 1));
		ASSERT_THAT(AreEqual(Repeat->NumItems(), 2));
	}
};

/** Several scopes read as one (T-850): every call on a path goes to the member that owns it. */
TEST_CLASS(SlaterChainScopeTests, "BRS.SlaterUI.Bind.Chain")
{
	TEST_METHOD(EveryCallOnAPath_GoesToTheFirstMemberThatOwnsIt)
	{
		FString WrittenToFirst;
		FString WrittenToSecond;
		const TSharedRef<FSlaterGetterScope> First = MakeShared<FSlaterGetterScope>();
		First->AddValue(TEXT("Both"), []() { return FString(TEXT("first")); });
		First->AddSetter(TEXT("Both"), [&WrittenToFirst](const FString& V) { WrittenToFirst = V; return true; });
		First->AddSetter(TEXT("OnlySecond"), [&WrittenToFirst](const FString& V) { WrittenToFirst = V; return true; });
		const TSharedRef<FSlaterGetterScope> Second = MakeShared<FSlaterGetterScope>();
		Second->AddValue(TEXT("Both"), []() { return FString(TEXT("second")); });
		Second->AddSetter(TEXT("Both"), [&WrittenToSecond](const FString& V) { WrittenToSecond = V; return true; });
		Second->AddValue(TEXT("OnlySecond"), []() { return FString(TEXT("two")); });
		Second->AddList(TEXT("Things"), MakeShared<FBgCountList>());
		const TSharedRef<ISlaterScope> Chain = MakeShared<FSlaterChainScope>(TArray<TSharedRef<ISlaterScope>>{First, Second});

		FString Out;
		ASSERT_THAT(IsTrue(Chain->Read(TEXT("Both"), Out) && Out == TEXT("first"), *Out));
		ASSERT_THAT(IsTrue(Chain->Read(TEXT("OnlySecond"), Out) && Out == TEXT("two"), *Out));
		ASSERT_THAT(IsFalse(Chain->Read(TEXT("Nope"), Out)));
		ASSERT_THAT(IsTrue(Chain->FindList(TEXT("Things"), TEXT("Id")).IsValid()));
		FString Why;
		ASSERT_THAT(IsTrue(Chain->Check(TEXT("Things"), true, Why)));
		ASSERT_THAT(IsFalse(Chain->Check(TEXT("Both"), true, Why)));

		ASSERT_THAT(IsTrue(Chain->Write(TEXT("Both"), TEXT("1"))));
		ASSERT_THAT(IsTrue(WrittenToFirst == TEXT("1") && WrittenToSecond.IsEmpty()));
		// The second member owns OnlySecond and has no setter for it: the first one's setter is not used instead.
		ASSERT_THAT(IsFalse(Chain->CheckWrite(TEXT("OnlySecond"), Why)));
		ASSERT_THAT(IsFalse(Chain->Write(TEXT("OnlySecond"), TEXT("2"))));
		ASSERT_THAT(IsTrue(WrittenToFirst == TEXT("1")));

		int32 Fired = 0;
		const TUniquePtr<ISlaterSubscription> Sub = Chain->Subscribe(TEXT("Both"), [&Fired]() { ++Fired; });
		Second->Notify(TEXT("Both"));
		ASSERT_THAT(AreEqual(Fired, 0));
		First->Notify(TEXT("Both"));
		ASSERT_THAT(AreEqual(Fired, 1));
		ASSERT_THAT(IsTrue(Chain->Subscribe(TEXT("Nope"), []() {}).IsValid()));
	}

	TEST_METHOD(APathNoMemberOwns_FailsWithTheMemberThatCanSuggestAName)
	{
		const TSharedRef<FSlaterGetterScope> First = MakeShared<FSlaterGetterScope>();
		First->AddValue(TEXT("Health"), []() { return FString(TEXT("1")); });
		const TSharedRef<FSlaterGetterScope> Second = MakeShared<FSlaterGetterScope>();
		Second->AddValue(TEXT("Zzz"), []() { return FString(TEXT("z")); });
		FString Why;
		ASSERT_THAT(IsFalse(FSlaterChainScope({First, Second}).Check(TEXT("Helth"), false, Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("did you mean 'Health'")), *Why));
		ASSERT_THAT(IsFalse(FSlaterChainScope({Second, First}).Check(TEXT("Helth"), false, Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("did you mean 'Health'")), *Why));
		ASSERT_THAT(IsFalse(FSlaterChainScope({}).Check(TEXT("Helth"), false, Why)));
		ASSERT_THAT(IsFalse(Why.IsEmpty()));
	}
};

#endif
