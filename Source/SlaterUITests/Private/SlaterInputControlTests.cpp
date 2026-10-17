// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterBindTestKit.h"
#include "SlaterEvents.h"
#include "SlaterObjectScope.h"
#include "SlaterTestViewModels.h"
#include "SlaterView.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"

/** The input controls batch (T-526): `spinbox` over `SSpinBox<float>` and `segmented` over `SSegmentedControl<FString>`; `radio` over `SCheckBox` (T-797). */
TEST_CLASS(SlaterInputControlTests, "BRS.SlaterUI.Controls.Input")
{
	FSlaterBindFixture Fix;

	BEFORE_EACH()
	{
		Fix.Init();
	}

	AFTER_EACH()
	{
		Fix.Shutdown();
	}

	TSharedRef<SSpinBox<float>> Spin(const TSharedRef<SSlaterView>& View, const TCHAR* Id)
	{
		return StaticCastSharedPtr<SSpinBox<float>>(View->FindById(Id)).ToSharedRef();
	}

	TSharedRef<SSegmentedControl<FString>> Segments(const TSharedRef<SSlaterView>& View, const TCHAR* Id)
	{
		return StaticCastSharedPtr<SSegmentedControl<FString>>(View->FindById(Id)).ToSharedRef();
	}

	TEST_METHOD(ASpinBox_Builds_WithItsRangeAndStartValue)
	{
		Fix.Write(TEXT("InSpin.sltr"), TEXT("<vbox><spinbox id=\"N\" min=\"2\" max=\"8\" step=\"2\" value=\"4\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InSpin"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedPtr<SWidget> Widget = View->FindById(TEXT("N"));
		ASSERT_THAT(IsTrue(Widget.IsValid()));
		ASSERT_THAT(AreEqual(Widget->GetType(), FName(TEXT("SSpinBox<float>"))));
		const TSharedRef<SSpinBox<float>> Box = Spin(View, TEXT("N"));
		ASSERT_THAT(IsNear(Box->GetMinValue(), 2.f, 1e-6f));
		ASSERT_THAT(IsNear(Box->GetMaxValue(), 8.f, 1e-6f));
		ASSERT_THAT(IsNear(Box->GetValue(), 4.f, 1e-6f));
	}

	TEST_METHOD(ASpinBox_ValueOutsideTheRange_IsClamped_WrittenOrBound)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Fix.Write(TEXT("InClamp.sltr"), TEXT("<vbox><spinbox id=\"W\" min=\"0\" max=\"10\" value=\"50\"/><spinbox id=\"B\" min=\"1\" max=\"5\" value=\"{Volume}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InClamp"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(IsNear(Spin(View, TEXT("W"))->GetValue(), 10.f, 1e-6f));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		// Volume is 0.5, below the box's min of 1.
		ASSERT_THAT(IsNear(Spin(View, TEXT("B"))->GetValue(), 1.f, 1e-6f));
		Settings->MaxVolume = 100.f;
		Settings->SetVolume(40.f);
		ASSERT_THAT(IsNear(Spin(View, TEXT("B"))->GetValue(), 5.f, 1e-6f));
		Settings->SetVolume(3.f);
		ASSERT_THAT(IsNear(Spin(View, TEXT("B"))->GetValue(), 3.f, 1e-6f));
	}

	TEST_METHOD(ASpinBox_Edits_WriteBackThroughTheSetter_AndAreClamped)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->MaxVolume = 100.f;
		Fix.Write(TEXT("InWrite.sltr"), TEXT("<vbox><spinbox id=\"V\" min=\"0\" max=\"10\" value=\"{Volume}\" on-change=\"{Volume}\"/><text>{Volume}</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InWrite"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedRef<SSpinBox<float>> Box = Spin(View, TEXT("V"));
		ASSERT_THAT(IsNear(Box->GetValue(), 0.5f, 1e-6f));

		// The engine's commit path (a typed value or a drag): the setter gets the value, the text follows.
		Box->SetValue(7.f, true);
		ASSERT_THAT(IsNear(Settings->Volume, 7.f, 1e-6f));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View).Last(), FString(TEXT("7"))));

		// Above the range: the data gets the max, never the typed number.
		Box->SetValue(500.f, true);
		ASSERT_THAT(IsNear(Settings->Volume, 10.f, 1e-6f));
		ASSERT_THAT(IsNear(Box->GetValue(), 10.f, 1e-6f));

		// A change from code moves the box and writes nothing back.
		const int32 SetsBefore = Settings->VolumeSets;
		Settings->SetVolume(2.f);
		ASSERT_THAT(IsNear(Box->GetValue(), 2.f, 1e-6f));
		ASSERT_THAT(AreEqual(Settings->VolumeSets, SetsBefore + 1));
	}

	/** The first error's text when the layout is refused, or empty when it built. */
	FString Refusal(const TCHAR* Layout)
	{
		FSlaterStyleStack Stack;
		Slater::StyleLibrary().BuildStack(TEXT("Base"), Stack);
		FSlaterBuildResult Result;
		if (Slater::BuildLayoutFromText(Layout, TEXT("In"), Stack, Result))
		{
			return FString();
		}
		return Result.Errors.IsEmpty() ? FString(TEXT("(no error text)")) : Result.Errors[0].Message;
	}

	TEST_METHOD(ASpinBox_WithABadRange_IsRefused)
	{
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><spinbox min=\"5\" max=\"5\"/></vbox>")).Contains(TEXT("max (5) must be above min (5)"))));
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><spinbox step=\"0\"/></vbox>")).Contains(TEXT("step must be above 0"))));
	}

	TEST_METHOD(ASegmentedControl_Builds_OneSegmentPerOption_WithTheSelectedOne)
	{
		Fix.Write(TEXT("InSeg.sltr"), TEXT("<vbox><segmented id=\"Q\" options=\"Low, Medium,High, ,Medium\" selected=\"High\"/><segmented id=\"D\" options=\"A,B\" selected=\"Nope\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InSeg"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		ASSERT_THAT(AreEqual(View->FindById(TEXT("Q"))->GetType(), FName(TEXT("SSegmentedControl<FString>"))));
		const TSharedRef<SSegmentedControl<FString>> Quality = Segments(View, TEXT("Q"));
		// Blank and repeated entries are dropped.
		ASSERT_THAT(AreEqual(Quality->NumSlots(), 3));
		ASSERT_THAT(AreEqual(Quality->GetValue(), FString(TEXT("High"))));
		// A selected value that is not an option falls back to the first.
		ASSERT_THAT(AreEqual(Segments(View, TEXT("D"))->GetValue(), FString(TEXT("A"))));
	}

	/** Reaches the protected brush attribute of an SImage (it has no public getter). */
	struct FImagePeek : SImage
	{
		using SImage::GetImageAttribute;
	};

	/** Every SImage brush under Widget, in tree order. */
	static void CollectImages(SWidget& Widget, TArray<const FSlateBrush*>& Out)
	{
		if (Widget.GetType() == FName(TEXT("SImage")))
		{
			Out.Add(static_cast<FImagePeek&>(static_cast<SImage&>(Widget)).GetImageAttribute().Get());
		}
		Widget.GetChildren()->ForEachWidget([&Out](SWidget& Child) { CollectImages(Child, Out); });
	}

	TEST_METHOD(ASegmentedControl_Icons_AreShownPerOption_FromTheEngineStyle)
	{
		Fix.Write(TEXT("InSegIcons.sltr"), TEXT("<vbox><segmented id=\"S\" options=\"Box, Sphere\" icons=\"Icons.box-perspective, Icons.sphere\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InSegIcons"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TSharedRef<SSegmentedControl<FString>> Control = Segments(View, TEXT("S"));
		ASSERT_THAT(AreEqual(Control->NumSlots(), 2));
		TArray<const FSlateBrush*> Shown;
		CollectImages(*Control, Shown);
		ASSERT_THAT(AreEqual(Shown.Num(), 2));
		ASSERT_THAT(IsTrue(Shown[0] == FCoreStyle::Get().GetBrush("Icons.box-perspective")));
		ASSERT_THAT(IsTrue(Shown[1] == FCoreStyle::Get().GetBrush("Icons.sphere")));
	}

	TEST_METHOD(ASegmentedControl_WithBadIcons_IsRefused)
	{
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><segmented options=\"A,B\" icons=\"Icons.sphere\"/></vbox>")).Contains(TEXT("one icon per option"))));
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><segmented options=\"A\" icons=\"Icons.NoSuchThing\"/></vbox>")).Contains(TEXT("Icons.NoSuchThing"))));
	}

	TEST_METHOD(ASegmentedControl_Selection_FollowsTheData_AndWritesBack)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->Nickname = TEXT("Medium");
		Fix.Write(TEXT("InSegBind.sltr"), TEXT("<vbox><segmented id=\"Q\" options=\"Low,Medium,High\" selected=\"{Nickname}\" on-change=\"{Nickname}\"/><text>{Nickname}</text></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InSegBind"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TSharedRef<SSegmentedControl<FString>> Control = Segments(View, TEXT("Q"));
		ASSERT_THAT(AreEqual(Control->GetValue(), FString(TEXT("Medium"))));

		Settings->SetNickname(TEXT("Low"));
		ASSERT_THAT(AreEqual(Control->GetValue(), FString(TEXT("Low"))));

		// The user picks a segment: the sink gets the option, the setter the text.
		const TSharedPtr<FSlaterEventSink> Sink = Slater::FindEventSink(*Control);
		ASSERT_THAT(IsTrue(Sink.IsValid()));
		const int32 SetsBefore = Settings->NicknameSets;
		Sink->Fire(TEXT("on-change"), TEXT("High"));
		ASSERT_THAT(AreEqual(Settings->NicknameSets, SetsBefore + 1));
		ASSERT_THAT(AreEqual(Settings->Nickname, FString(TEXT("High"))));
		ASSERT_THAT(AreEqual(Control->GetValue(), FString(TEXT("High"))));
		ASSERT_THAT(AreEqual(SlaterBindKit::Texts(View).Last(), FString(TEXT("High"))));
	}

	TEST_METHOD(ASegmentedControl_WithNoOptions_IsRefused)
	{
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><segmented options=\" , \"/></vbox>")).Contains(TEXT("needs at least one choice"))));
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><segmented/></vbox>")).Contains(TEXT("options"))));
	}

	/** The radio group's round boxes, in order. */
	TArray<TSharedPtr<SCheckBox>> RadioBoxes(const TSharedRef<SSlaterView>& View, const TCHAR* Id)
	{
		TArray<TSharedPtr<SCheckBox>> Boxes;
		TFunction<void(SWidget&)> Collect = [&](SWidget& Widget)
		{
			if (Widget.GetType() == FName(TEXT("SCheckBox")))
			{
				Boxes.Add(StaticCastSharedRef<SCheckBox>(Widget.AsShared()));
				return;
			}
			Widget.GetChildren()->ForEachWidget([&](SWidget& Child) { Collect(Child); });
		};
		Collect(*View->FindById(Id));
		return Boxes;
	}

	TEST_METHOD(ARadio_Builds_OneRoundBoxPerOption_WithTheSelectedOneChecked)
	{
		Fix.Write(TEXT("InRadio.sltr"), TEXT("<vbox><radio id=\"R\" options=\"Low, Medium,High, ,Medium\" selected=\"High\"/><radio id=\"D\" options=\"A,B\" selected=\"Nope\" direction=\"horizontal\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InRadio"));
		ASSERT_THAT(IsFalse(View->HasErrors()));
		const TArray<TSharedPtr<SCheckBox>> Boxes = RadioBoxes(View, TEXT("R"));
		ASSERT_THAT(AreEqual(Boxes.Num(), 3));
		ASSERT_THAT(IsTrue(!Boxes[0]->IsChecked() && !Boxes[1]->IsChecked() && Boxes[2]->IsChecked()));
		// A selected value that is not an option falls back to the first.
		const TArray<TSharedPtr<SCheckBox>> Other = RadioBoxes(View, TEXT("D"));
		ASSERT_THAT(AreEqual(Other.Num(), 2));
		ASSERT_THAT(IsTrue(Other[0]->IsChecked() && !Other[1]->IsChecked()));
		ASSERT_THAT(AreEqual(View->FindById(TEXT("D"))->GetType(), FName(TEXT("SHorizontalBox"))));
		ASSERT_THAT(AreEqual(View->FindById(TEXT("R"))->GetType(), FName(TEXT("SVerticalBox"))));
	}

	TEST_METHOD(ARadio_ChoosingAnOption_MovesTheChoice_AndNeverUnchecksIt)
	{
		Fix.Write(TEXT("InRadioPick.sltr"), TEXT("<vbox><radio id=\"R\" options=\"A,B,C\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InRadioPick"));
		const TArray<TSharedPtr<SCheckBox>> Boxes = RadioBoxes(View, TEXT("R"));
		ASSERT_THAT(AreEqual(Boxes.Num(), 3));
		Boxes[2]->ToggleCheckedState();
		ASSERT_THAT(IsTrue(!Boxes[0]->IsChecked() && !Boxes[1]->IsChecked() && Boxes[2]->IsChecked()));
		// Pressing the chosen one again keeps it chosen.
		Boxes[2]->ToggleCheckedState();
		ASSERT_THAT(IsTrue(Boxes[2]->IsChecked()));
		Boxes[1]->ToggleCheckedState();
		ASSERT_THAT(IsTrue(!Boxes[0]->IsChecked() && Boxes[1]->IsChecked() && !Boxes[2]->IsChecked()));
	}

	TEST_METHOD(ARadio_Choice_FollowsTheData_AndWritesBack)
	{
		USlaterTestSettingsVM* Settings = Fix.Make<USlaterTestSettingsVM>();
		Settings->Nickname = TEXT("B");
		Fix.Write(TEXT("InRadioBind.sltr"), TEXT("<vbox><radio id=\"R\" options=\"A,B,C\" selected=\"{Nickname}\" on-change=\"{Nickname}\"/></vbox>"));
		const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("InRadioBind"));
		ASSERT_THAT(IsTrue(View->SetContext(Slater::MakeObjectScope(Settings))));
		const TArray<TSharedPtr<SCheckBox>> Boxes = RadioBoxes(View, TEXT("R"));
		ASSERT_THAT(IsTrue(Boxes[1]->IsChecked() && !Boxes[0]->IsChecked()));

		Settings->SetNickname(TEXT("C"));
		ASSERT_THAT(IsTrue(Boxes[2]->IsChecked() && !Boxes[1]->IsChecked()));

		const int32 SetsBefore = Settings->NicknameSets;
		Boxes[0]->ToggleCheckedState();
		ASSERT_THAT(AreEqual(Settings->NicknameSets, SetsBefore + 1));
		ASSERT_THAT(AreEqual(Settings->Nickname, FString(TEXT("A"))));
		ASSERT_THAT(IsTrue(Boxes[0]->IsChecked() && !Boxes[2]->IsChecked()));
	}

	TEST_METHOD(ARadio_WithNoOptions_IsRefused)
	{
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><radio options=\" , \"/></vbox>")).Contains(TEXT("needs at least one choice"))));
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><radio/></vbox>")).Contains(TEXT("options"))));
		ASSERT_THAT(IsTrue(Refusal(TEXT("<vbox><radio options=\"A\" direction=\"diagonal\"/></vbox>")).Contains(TEXT("direction"))));
	}};

#endif
