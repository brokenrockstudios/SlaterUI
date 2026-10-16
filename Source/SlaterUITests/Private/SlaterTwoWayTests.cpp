// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "SlaterBindTestKit.h"
#include "SlaterBindings.h"
#include "SlaterCommands.h"
#include "SlaterEvents.h"
#include "SlaterList.h"
#include "SlaterModel.h"
#include "SlaterObjectScope.h"
#include "SlaterScope.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SSpacer.h"

namespace
{
	/** A slider press goes through the engine's own path: pointer position, `PositionToValue`, `CommitValue`, `OnValueChanged`. The track is 200 px wide, so 0 is min, 200 is max and 100 the middle whatever the thumb's size. */
	void TwPressSlider(const TSharedPtr<SWidget>& Slider, float X)
	{
		const FGeometry Geometry = FGeometry::MakeRoot(FVector2f(200.f, 20.f), FSlateLayoutTransform());
		const FPointerEvent Press(0, FVector2f(X, 10.f), FVector2f(X, 10.f), TSet<FKey>(), EKeys::LeftMouseButton, 0.f, FModifierKeysState());
		static_cast<SWidget&>(*Slider).OnMouseButtonDown(Geometry, Press);
	}

	float TwSliderValue(const TSharedPtr<SWidget>& Slider)
	{
		return StaticCastSharedPtr<SSlider>(Slider)->GetValue();
	}

	TSharedPtr<SWidget> TwFindType(SWidget& Root, FName Type)
	{
		if (Root.GetType() == Type)
		{
			return Root.AsShared();
		}
		TSharedPtr<SWidget> Found;
		Root.GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (!Found.IsValid())
			{
				Found = TwFindType(Child, Type);
			}
		});
		return Found;
	}

	/** A control whose setter reports every value it is given as a change: the worst case for an echo. */
	void TwRegisterEcho(FSlaterElementRegistry& Registry)
	{
		FSlaterElementDesc Echo;
		Echo.Tag = TEXT("echo");
		FSlaterAttrDesc Value;
		Value.Name = TEXT("value");
		Value.Type = ESlaterAttrType::Metric;
		Value.Bind.Apply = [](SWidget& Widget, const FSlaterValue& In, const ISlaterStyleSource&)
		{
			Slater::FindEventSink(Widget)->Fire(TEXT("on-change"), Slater::NumberText(In.Number));
		};
		Value.Bind.ApplyPolled = [](SWidget&, TFunction<FSlaterValue()>, const ISlaterStyleSource&) {};
		Echo.Attributes = {Value, Slater::EventAttribute(TEXT("on-change"))};
		Echo.Builder = [](FSlaterBuildContext&) -> TSharedPtr<SWidget>
		{
			const TSharedRef<SSpacer> Widget = SNew(SSpacer);
			Slater::AttachEventSink(*Widget, MakeShared<FSlaterEventSink>());
			return Widget;
		};
		Registry.Register(MoveTemp(Echo));
	}
}

/** Element events that write back to a UObject view model the engine's MVVM way (T-381). */
TEST_CLASS(SlaterTwoWayViewModelTests, "BRS.SlaterUI.TwoWay.ViewModel")
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

	TEST_METHOD(ASliderPress_WritesThroughTheSetter_AndEveryBindingFollows)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Fix.Write(TEXT("TwSlider.sltr"), TEXT("<vbox><slider id=\"Vol\" value=\"{Volume}\" on-change=\"{Volume}\"/><text>{Volume}</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwSlider"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SWidget> Slider = View->FindById(TEXT("Vol"));
		ASSERT_THAT(AreEqual(Slider->GetType(), FName(TEXT("SSlider"))));
		ASSERT_THAT(IsNear(TwSliderValue(Slider), 0.5f, 1e-6f));

		TwPressSlider(Slider, 200.f);
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 1));
		ASSERT_THAT(IsNear(Settings->Volume, 1.f, 1e-6f));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View)[0], FString(TEXT("1"))));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 1));

		TwPressSlider(Slider, 100.f);
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 2));
		ASSERT_THAT(IsNear(Settings->Volume, 0.5f, 1e-6f));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View)[0], FString(TEXT("0.5"))));

		// A change from code still flows the other way, and writes nothing back.
		Settings->SetVolume(0.25f);
		ASSERT_THAT(IsNear(TwSliderValue(Slider), 0.25f, 1e-6f));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View)[0], FString(TEXT("0.25"))));
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 3));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 2));
	}

	TEST_METHOD(TheModelChangingTheValueBack_MovesTheControlBack_EvenWithoutABroadcast)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->MaxVolume = 0.8f;
		Fix.Write(TEXT("TwClamp.sltr"), TEXT("<vbox><slider id=\"Vol\" value=\"{Volume}\" on-change=\"{Volume}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwClamp"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SWidget> Slider = View->FindById(TEXT("Vol"));

		// The setter clamps 1 to 0.8 and broadcasts the change: the slider follows the data, not the pointer.
		TwPressSlider(Slider, 200.f);
		ASSERT_THAT(IsNear(Settings->Volume, 0.8f, 1e-6f));
		ASSERT_THAT(IsNear(TwSliderValue(Slider), 0.8f, 1e-6f));

		// Again: the field stays 0.8, so the view model broadcasts nothing; the write-back still puts the slider back.
		TwPressSlider(Slider, 200.f);
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 2));
		ASSERT_THAT(IsNear(Settings->Volume, 0.8f, 1e-6f));
		ASSERT_THAT(IsNear(TwSliderValue(Slider), 0.8f, 1e-6f));
	}

	TEST_METHOD(NoEchoLoop_WhenTheControlReportsEverySet_AndTheModelAlwaysBroadcasts)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterElementRegistry Registry;
		Slater::RegisterLayoutElements(Registry);
		Slater::RegisterContentElements(Registry);
		Slater::RegisterControlElements(Registry);
		TwRegisterEcho(Registry);
		FSlaterBuildResult Result;
		ASSERT_THAT(IsTrue(Slater::BuildLayoutFromText(TEXT("<vbox><echo value=\"{Volume}\" on-change=\"{Volume}\"/><slider value=\"{Volume}\" on-change=\"{Volume}\"/></vbox>"), TEXT("Tw"), Stack, Result, FSlaterBudget(), Registry)));
		ASSERT_THAT(AreEqual(Result.Events.Num(), 2));

		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->bAlwaysBroadcast = true;
		const TSharedRef<FSlaterBinder> Binder = Slater::Bind(Result, Slater::MakeObjectScope(Settings), ESlaterBindMode::Pushed, Stack);
		const TSharedPtr<FSlaterEventSink> Sink = Slater::FindEventSink(*Result.Events[0].Widget.Pin());
		ASSERT_THAT(IsTrue(Sink.IsValid()));
		// Binding wrote the value in once; the echo it fired was dropped, nothing was written.
		ASSERT_THAT(AreEqual(Sink->NumDropped(), 1));
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 0));

		// A change from code: pushed in, echoed, dropped.
		Settings->SetVolume(0.3f);
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 1));
		ASSERT_THAT(AreEqual(Sink->NumDropped(), 2));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 0));

		// The user's change: one write, one setter call; the broadcast and the resync each push in and echo, and both echoes are dropped.
		Sink->Fire(TEXT("on-change"), TEXT("0.9"));
		ASSERT_THAT(AreEqual(Sink->NumHandled(), 1));
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 2));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 1));
		ASSERT_THAT(AreEqual(Sink->NumDropped(), 4));
		ASSERT_THAT(IsNear(Settings->Volume, 0.9f, 1e-6f));
		ASSERT_THAT(IsNear(TwSliderValue(Result.Events[1].Widget.Pin()), 0.9f, 1e-6f));

		// A value the data already holds writes nothing.
		Sink->Fire(TEXT("on-change"), TEXT("0.9"));
		ASSERT_THAT(AreEqual(Settings->VolumeSets, 2));
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 1));
	}

	TEST_METHOD(AMissingSetter_FailsTheLoadAtTheAttribute)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Fix.Write(TEXT("TwNoSetter.sltr"), TEXT("<vbox>\n  <slider value=\"{Level}\" on-change=\"{Level}\"/>\n</vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwNoSetter"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsFalse(View->SetContext(Slater::MakeObjectScope(Settings))));
		ASSERT_THAT(AreEqual(View->GetReloadErrors().Num(), 1));
		const FSlaterError& Error = View->GetReloadErrors()[0];
		ASSERT_THAT(IsTrue(Error.ToString().Contains(TEXT("TwNoSetter.sltr(2,27)"))));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("on-change=\"{Level}\" cannot write to the data"))));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("has no setter"))));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("SetLevel"))));
	}

	TEST_METHOD(AWrongFieldOrAnUnwritableType_FailsTheLoad_WithWhy)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Fix.Write(TEXT("TwTypo.sltr"), TEXT("<vbox>\n<slider on-change=\"{Volum}\"/>\n<checkbox on-change=\"{Target}\"/>\n</vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwTypo"));
		ASSERT_THAT(IsFalse(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TArray<FSlaterError>& Errors = View->GetReloadErrors();
		ASSERT_THAT(AreEqual(Errors.Num(), 2));
		ASSERT_THAT(AreEqual(Errors[0].Line, 2));
		ASSERT_THAT(IsTrue(Errors[0].Message.Contains(TEXT("did you mean 'Volume'"))));
		ASSERT_THAT(AreEqual(Errors[1].Line, 3));
		ASSERT_THAT(IsTrue(Errors[1].Message.Contains(TEXT("a control writes text, a number"))));
	}

	TEST_METHOD(ACheckBox_WritesABlueprintReadWriteProperty_AndTheScopeBroadcastsIt)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Fix.Write(TEXT("TwCheckBox.sltr"), TEXT("<vbox><checkbox id=\"Mute\" checked=\"{bMuted}\" on-change=\"{bMuted}\"><text>Mute</text></checkbox><text>{bMuted}</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwCheckBox"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SCheckBox> CheckBox = StaticCastSharedPtr<SCheckBox>(View->FindById(TEXT("Mute")));
		ASSERT_THAT(AreEqual(CheckBox->GetType(), FName(TEXT("SCheckBox"))));
		ASSERT_THAT(IsFalse(CheckBox->IsChecked()));

		CheckBox->ToggleCheckedState();
		ASSERT_THAT(IsTrue(Settings->bMuted));
		// Nothing of the view model ran, so the scope broadcast the field: the other binding shows it.
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View)[1], FString(TEXT("true"))));
		ASSERT_THAT(IsTrue(CheckBox->IsChecked()));

		Settings->bMuted = false;
		Settings->Notify(TEXT("bMuted"));
		ASSERT_THAT(IsFalse(CheckBox->IsChecked()));
	}

	TEST_METHOD(ATextBoxCommit_GoesThroughTheSetter_AndShowsWhatTheSetterKept)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->Nickname = TEXT("Ann");
		Fix.Write(TEXT("TwText.sltr"), TEXT("<vbox><textbox id=\"Nick\" text=\"{Nickname}\" on-commit=\"{Nickname}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwText"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedPtr<SEditableTextBox> Box = StaticCastSharedPtr<SEditableTextBox>(View->FindById(TEXT("Nick")));
		ASSERT_THAT(AreEqual(Box->GetText().ToString(), FString(TEXT("Ann"))));
		const TSharedPtr<SEditableText> Inner = StaticCastSharedPtr<SEditableText>(TwFindType(*Box, TEXT("SEditableText")));
		ASSERT_THAT(IsTrue(Inner.IsValid()));

		// Typing is not a commit.
		Inner->SetEditableText(FText::FromString(TEXT("  Frog  ")));
		ASSERT_THAT(AreEqual(Settings->NicknameSets, 0));

		const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
		static_cast<SWidget&>(*Inner).OnKeyDown(FGeometry(), Enter);
		ASSERT_THAT(AreEqual(Settings->NicknameSets, 1));
		ASSERT_THAT(AreEqual(Settings->Nickname, FString(TEXT("Frog"))));
		ASSERT_THAT(AreEqual(Box->GetText().ToString(), FString(TEXT("Frog"))));
	}

	TEST_METHOD(AControlInARepeatedItem_WritesThatItem)
	{
		USlaterTestRootVM* Root = Fix.Make<USlaterTestRootVM>();
		Root->Items.Add(Fix.MakeItem(TEXT("a"), 1));
		Root->Items.Add(Fix.MakeItem(TEXT("b"), 2));
		Fix.Write(TEXT("TwItems.sltr"), TEXT("<vbox><repeat source=\"{Items}\" key=\"Id\" columns=\"1\"><slider value=\"{Count}\" on-change=\"{Count}\" max=\"10\"/></repeat></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwItems"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Root))));
		const TSharedPtr<SSlaterRepeat> Repeat = SlaterBindKit::Repeat(View);
		ASSERT_THAT(IsTrue(Repeat.IsValid()));
		const TSharedPtr<SWidget> Second = Repeat->FindItemWidget(TEXT("b"));
		ASSERT_THAT(IsNear(TwSliderValue(Second), 2.f, 1e-6f));

		TwPressSlider(Second, 200.f);
		ASSERT_THAT(AreEqual(Root->Items[1]->Count, 10));
		ASSERT_THAT(AreEqual(Root->Items[0]->Count, 1));
		ASSERT_THAT(IsNear(TwSliderValue(Second), 10.f, 1e-6f));
		ASSERT_THAT(IsNear(TwSliderValue(Repeat->FindItemWidget(TEXT("a"))), 1.f, 1e-6f));
	}

	TEST_METHOD(ACommandEvent_GetsTheValueAndItsArgument_WithNoContext)
	{
		Fix.Write(TEXT("TwCommand.sltr"), TEXT("<vbox><slider id=\"S\" on-change=\"Audio.SetVolume:Music\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwCommand"));
		FSlaterCommandCall Got;
		int32 Calls = 0;
		ASSERT_THAT(IsTrue(View->GetCommands().Register(TEXT("Audio.SetVolume"), [&](const FSlaterCommandCall& Call) { Got = Call; ++Calls; })));

		TwPressSlider(View->FindById(TEXT("S")), 200.f);
		ASSERT_THAT(AreEqual(Calls, 1));
		ASSERT_THAT(AreEqual(Got.Value, FString(TEXT("1"))));
		ASSERT_THAT(AreEqual(Got.Argument, FString(TEXT("Music"))));
		ASSERT_THAT(IsTrue(Got.Source == View->FindById(TEXT("S"))));
	}

	TEST_METHOD(AWriteBackWithNoData_WarnsOnce)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("has no data to write to"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		Fix.Write(TEXT("TwNoData.sltr"), TEXT("<vbox><slider id=\"S\" on-change=\"{Volume}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwNoData"));
		TwPressSlider(View->FindById(TEXT("S")), 200.f);
		TwPressSlider(View->FindById(TEXT("S")), 0.f);
		ASSERT_THAT(AreEqual(Slater::ListStats().Writes, 0));
	}
};

/** The other scopes take writes too, and the event attributes are checked like any other (T-381). */
TEST_CLASS(SlaterTwoWayScopeTests, "BRS.SlaterUI.TwoWay.Scopes")
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

	TEST_METHOD(TheMockModel_TakesWrites_AtTheRootAndInAnItem_KeepingNumbers)
	{
		const TSharedRef<FSlaterModel> Model = MakeShared<FSlaterModel>();
		FSlaterError LoadError;
		ASSERT_THAT(IsTrue(Model->LoadMock(TEXT("{\"Volume\":0.5,\"Items\":[{\"Id\":\"a\",\"Count\":1}]}"), TEXT("mock"), LoadError)));
		Fix.Write(TEXT("TwMock.sltr"), TEXT("<vbox><slider id=\"V\" value=\"{Volume}\" on-change=\"{Volume}\"/><repeat source=\"{Items}\" key=\"Id\" columns=\"1\"><slider value=\"{Count}\" on-change=\"{Count}\" max=\"10\"/></repeat></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwMock"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeModelScope(Model))));

		TwPressSlider(View->FindById(TEXT("V")), 200.f);
		FString Volume;
		ASSERT_THAT(IsTrue(Model->GetString(TEXT("Volume"), Volume)));
		ASSERT_THAT(AreEqual(Volume, FString(TEXT("1"))));

		TwPressSlider(SlaterBindKit::Repeat(View)->FindItemWidget(TEXT("a")), 200.f);
		const TSharedPtr<FJsonObject> Item = Model->FindItem(TEXT("Items"), TEXT("Id"), TEXT("a"));
		ASSERT_THAT(IsTrue(Item.IsValid()));
		const TSharedPtr<FJsonValue> Count = Item->TryGetField(TEXT("Count"));
		ASSERT_THAT(IsTrue(Count.IsValid() && Count->Type == EJson::Number));
		ASSERT_THAT(IsTrue(Count->AsNumber() == 10.0));
	}

	TEST_METHOD(AGetterScope_WritesThroughItsRegisteredSetter_AndRefusesAPathWithNone)
	{
		float Volume = 0.5f;
		const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
		Scope->AddValue(TEXT("Volume"), [&Volume]() { return Slater::NumberText(Volume); });
		Scope->AddSetter(TEXT("Volume"), [&Volume, &Getters = *Scope](const FString& Text)
		{
			Volume = FCString::Atof(*Text);
			Getters.Notify(TEXT("Volume"));
			return true;
		});
		Fix.Write(TEXT("TwGetter.sltr"), TEXT("<vbox><slider id=\"V\" value=\"{Volume}\" on-change=\"{Volume}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwGetter"));
		ASSERT_THAT(IsTrue(View->SetContext(Scope)));
		TwPressSlider(View->FindById(TEXT("V")), 200.f);
		ASSERT_THAT(IsNear(Volume, 1.f, 1e-6f));

		FString Why;
		ASSERT_THAT(IsFalse(Scope->CheckWrite(TEXT("Volme"), Why)));
		ASSERT_THAT(IsTrue(Why.Contains(TEXT("did you mean 'Volume'"))));
	}

	TEST_METHOD(TheShippedSettingsLayout_EditsItsMockData)
	{
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Settings")).UseMockContext(true);
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<ISlaterScope> Mock = View->GetActiveContext();
		ASSERT_THAT(IsTrue(Mock.IsValid()));
		FString Text;

		TwPressSlider(TwFindType(*View, TEXT("SSlider")), 200.f);
		ASSERT_THAT(IsTrue(Mock->Read(TEXT("Volume"), Text) && Text == TEXT("1")));
		StaticCastSharedPtr<SCheckBox>(TwFindType(*View, TEXT("SCheckBox")))->ToggleCheckedState();
		ASSERT_THAT(IsTrue(Mock->Read(TEXT("Muted"), Text) && Text == TEXT("true")));
		// The muted texts read the same fields, so they show what the controls wrote.
		const TArray<FString> Texts = SlaterBindKit::Texts(View);
		ASSERT_THAT(IsTrue(Texts.Contains(TEXT("1")) && Texts.Contains(TEXT("true")) && Texts.Contains(TEXT("Tom"))));
	}

	TEST_METHOD(AnEventIsAPathOrACommand_AndAStockToggleNamesADeclaredState)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Bad;
		ASSERT_THAT(IsFalse(Slater::BuildLayoutFromText(TEXT("<vbox>\n<slider on-change=\"Bad Name\"/>\n</vbox>"), TEXT("Tw"), Stack, Bad)));
		ASSERT_THAT(AreEqual(Bad.Errors.Num(), 1));
		ASSERT_THAT(AreEqual(Bad.Errors[0].Line, 2));
		ASSERT_THAT(AreEqual(Bad.Errors[0].Column, 9));
		ASSERT_THAT(IsTrue(Bad.Errors[0].Message.Contains(TEXT("on-change takes {Path}"))));

		FSlaterBuildResult Undeclared;
		ASSERT_THAT(IsFalse(Slater::BuildLayoutFromText(TEXT("<vbox><state name=\"m\" default=\"false\"/><checkbox on-change=\"ui.toggle:nope\"/></vbox>"), TEXT("Tw"), Stack, Undeclared)));
		ASSERT_THAT(IsTrue(Undeclared.Errors[0].Message.Contains(TEXT("'nope' is not a declared state"))));

		FSlaterBuildResult Range;
		ASSERT_THAT(IsFalse(Slater::BuildLayoutFromText(TEXT("<slider min=\"1\" max=\"1\"/>"), TEXT("Tw"), Stack, Range)));
		ASSERT_THAT(IsTrue(Range.Errors[0].Message.Contains(TEXT("max (1) must be above min (1)"))));

		// A stock command from an event runs on the view, as from a click.
		Fix.Write(TEXT("TwState.sltr"), TEXT("<vbox><state name=\"m\" default=\"false\"/><checkbox id=\"T\" on-change=\"ui.toggle:m\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("TwState"));
		StaticCastSharedPtr<SCheckBox>(View->FindById(TEXT("T")))->ToggleCheckedState();
		FString Value;
		ASSERT_THAT(IsTrue(View->GetState(TEXT("m"), Value)));
		ASSERT_THAT(AreEqual(Value, FString(TEXT("true"))));
	}
};

#endif
