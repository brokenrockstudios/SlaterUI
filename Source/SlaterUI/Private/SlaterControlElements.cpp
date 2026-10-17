// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterControls.h"
#include "SlaterAttrNames.h"
#include "SlaterElements.h"
#include "SlaterEvents.h"
#include "SlaterList.h"
#include "SlaterSurface.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SNullWidget.h"

/**
 * The stock controls (T-381): engine Slate widgets whose change delegates fire element events. Each builder makes an
 * `FSlaterEventSink`, wires the widget's delegate to it and attaches it; the layout build gives the sink what the
 * event attributes say. Their look (T-514) is the engine's own style struct (`FSliderStyle`, `FCheckBoxStyle`,
 * `FEditableTextBoxStyle`) copied from the core style and filled with rounded-box brushes made from surface and color
 * tokens, so a style file themes them like any other element. A token a style lacks falls back to a fixed look.
 */
namespace
{
	const FName ControlOnCommit(TEXT("on-commit"));

	FSlaterAttrDesc ControlAttr(const TCHAR* Name, ESlaterAttrType Type)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		return Desc;
	}

	FSlaterAttrDesc ControlEnumAttr(const TCHAR* Name, std::initializer_list<const TCHAR*> Words)
	{
		FSlaterAttrDesc Desc = ControlAttr(Name, ESlaterAttrType::Enum);
		for (const TCHAR* Word : Words)
		{
			Desc.EnumValues.Add(Word);
		}
		return Desc;
	}

	FSlaterAttrDesc ControlWithBind(FSlaterAttrDesc Desc, FSlaterBindTarget Bind)
	{
		Desc.Bind = MoveTemp(Bind);
		return Desc;
	}

	void AddControlMetricDefault(TMap<FName, FSlaterValue>& Values, const TCHAR* Name, float Number)
	{
		if (!Values.Contains(Name))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Metric;
			Value.Number = Number;
			Values.Add(Name, MoveTemp(Value));
		}
	}

	/** A surface token from the layout's style, or the fallback when the style (or its stack) has none. */
	FSlaterSurface ControlSurface(const FSlaterBuildContext& Context, const TCHAR* Token, const FSlaterSurface& Fallback)
	{
		FSlaterSurface Found;
		return Context.Style && Context.Style->FindSurface(Token, Found) ? Found : Fallback;
	}

	FSlaterSurface MakeControlSurface(const FLinearColor& Fill, const FLinearColor& Border, float BorderWidth, float Radius)
	{
		FSlaterSurface Surface;
		Surface.Fill = Fill;
		Surface.Border = Border;
		Surface.BorderWidth = BorderWidth;
		Surface.CornerRadius = Radius;
		return Surface;
	}

	FLinearColor ControlColor(const FSlaterBuildContext& Context, const TCHAR* Token, const FLinearColor& Fallback)
	{
		FLinearColor Found;
		return Context.Style && Context.Style->FindColor(Token, Found) ? Found : Fallback;
	}

	FSlateBrush ControlBrush(const FSlaterSurface& Surface, const FVector2f& Size)
	{
		FSlateBrush Brush = SSlaterSurface::MakeBrush(Surface);
		Brush.ImageSize = Size;
		return Brush;
	}

	void KeepControlLook(SWidget& Widget, const TSharedRef<FSlaterControlLook>& Look)
	{
		Widget.AddMetadata(Look);
	}
	// ---- slider ----

	FSlaterBindTarget SliderValueBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SSlider&>(Widget).SetValue(Value.Number);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SSlider&>(Widget).SetValue(TAttribute<float>::CreateLambda([Get = MoveTemp(Get)]() { return Get().Number; }));
		};
		return Target;
	}

	void ResolveSliderDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		AddControlMetricDefault(Values, TEXT("min"), 0.f);
		AddControlMetricDefault(Values, TEXT("max"), 1.f);
	}

	TSharedPtr<SWidget> BuildSlider(FSlaterBuildContext& Context)
	{
		const float Min = Context.GetMetric(TEXT("min"));
		const float Max = Context.GetMetric(TEXT("max"));
		if (Max <= Min)
		{
			Context.Failure = FString::Printf(TEXT("<slider> max (%s) must be above min (%s)"), *Slater::NumberText(Max), *Slater::NumberText(Min));
			return nullptr;
		}
		const bool bHasStep = Context.Has(TEXT("step"));
		const float Step = Context.GetMetric(TEXT("step"), 0.01f);
		if (Step <= 0.f)
		{
			Context.Failure = TEXT("<slider> step must be above 0");
			return nullptr;
		}
		// The bar is the Track surface and the thumb a round Accent one; `color` and `track`, when written, replace their fills.
		const TSharedRef<FSliderStyle> SliderStyle = MakeShared<FSliderStyle>(FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"));
		FSlaterSurface Bar = ControlSurface(Context, TEXT("UI.Surface.Slider.Bar"), MakeControlSurface(FLinearColor(0.88f, 0.9f, 0.92f), FLinearColor::Transparent, 0.f, 2.f));
		FSlaterSurface Thumb = ControlSurface(Context, TEXT("UI.Surface.Slider.Thumb"), MakeControlSurface(FLinearColor(0.18f, 0.43f, 0.92f), FLinearColor::Transparent, 0.f, 8.f));
		FSlaterSurface ThumbHover = ControlSurface(Context, TEXT("UI.Surface.Slider.Thumb.Hover"), Thumb);
		if (Context.Has(TEXT("track")))
		{
			Bar.Fill = Context.GetColor(TEXT("track"));
		}
		if (Context.Has(TEXT("color")))
		{
			Thumb.Fill = Context.GetColor(TEXT("color"));
			ThumbHover.Fill = Thumb.Fill;
		}
		SliderStyle->SetBarThickness(4.f);
		SliderStyle->SetNormalBarImage(ControlBrush(Bar, FVector2f(4.f, 4.f)));
		SliderStyle->SetHoveredBarImage(ControlBrush(Bar, FVector2f(4.f, 4.f)));
		SliderStyle->SetDisabledBarImage(ControlBrush(Bar, FVector2f(4.f, 4.f)));
		SliderStyle->SetNormalThumbImage(ControlBrush(Thumb, FVector2f(16.f, 16.f)));
		SliderStyle->SetHoveredThumbImage(ControlBrush(ThumbHover, FVector2f(16.f, 16.f)));
		SliderStyle->SetDisabledThumbImage(ControlBrush(Thumb, FVector2f(16.f, 16.f)));
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		// A bound value is written by the binding; until then the slider rests at min.
		const TSharedRef<SSlider> Slider = SNew(SSlider)
			.MinValue(Min)
			.MaxValue(Max)
			.Value(FMath::Clamp(Context.GetMetric(TEXT("value"), Min), Min, Max))
			.StepSize(Step)
			.MouseUsesStep(bHasStep)
			.Style(&*SliderStyle)
			.OnValueChanged_Lambda([Sink](float NewValue) { Sink->Fire(Slater::Attr::OnChange, Slater::NumberText(NewValue)); });
		Slater::AttachEventSink(*Slider, Sink);
		const TSharedRef<FSlaterControlLook> Look = MakeShared<FSlaterControlLook>();
		Look->Slider = SliderStyle;
		KeepControlLook(*Slider, Look);
		return Slider;
	}

	// ---- spinbox ----

	FSlaterBindTarget SpinBoxValueBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SSpinBox<float>&>(Widget).SetValue(Value.Number);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SSpinBox<float>&>(Widget).SetValue(TAttribute<float>::CreateLambda([Get = MoveTemp(Get)]() { return Get().Number; }));
		};
		return Target;
	}

	void ResolveSpinBoxDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		AddControlMetricDefault(Values, TEXT("min"), 0.f);
		AddControlMetricDefault(Values, TEXT("max"), 100.f);
		AddControlMetricDefault(Values, TEXT("step"), 1.f);
	}

	TSharedPtr<SWidget> BuildSpinBox(FSlaterBuildContext& Context)
	{
		const float Min = Context.GetMetric(TEXT("min"));
		const float Max = Context.GetMetric(TEXT("max"));
		if (Max <= Min)
		{
			Context.Failure = FString::Printf(TEXT("<spinbox> max (%s) must be above min (%s)"), *Slater::NumberText(Max), *Slater::NumberText(Min));
			return nullptr;
		}
		const float Step = Context.GetMetric(TEXT("step"));
		if (Step <= 0.f)
		{
			Context.Failure = TEXT("<spinbox> step must be above 0");
			return nullptr;
		}
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		// The engine's spin box clamps a typed, dragged or set value to min and max, so an event never carries more.
		const TSharedRef<SSpinBox<float>> Box = SNew(SSpinBox<float>)
			.MinValue(Min)
			.MaxValue(Max)
			.MinSliderValue(Min)
			.MaxSliderValue(Max)
			.Delta(Step)
			.Value(FMath::Clamp(Context.GetMetric(TEXT("value"), Min), Min, Max))
			.OnValueChanged_Lambda([Sink](float NewValue) { Sink->Fire(Slater::Attr::OnChange, Slater::NumberText(NewValue)); })
			.OnValueCommitted_Lambda([Sink](float NewValue, ETextCommit::Type) { Sink->Fire(ControlOnCommit, Slater::NumberText(NewValue)); });
		Slater::AttachEventSink(*Box, Sink);
		return Box;
	}

	// ---- segmented ----

	/** `options="Low, Medium, High"`: the words between commas, trimmed, empty and repeated ones dropped. */
	TArray<FString> SegmentedOptions(const FString& Text)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		TArray<FString> Options;
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty() && !Options.Contains(Part))
			{
				Options.Add(MoveTemp(Part));
			}
		}
		return Options;
	}

	FSlaterBindTarget SegmentedSelectedBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SSegmentedControl<FString>&>(Widget).SetValue(Value.Text);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SSegmentedControl<FString>&>(Widget).SetValue(TAttribute<FString>::CreateLambda([Get = MoveTemp(Get)]() { return Get().Text; }));
		};
		return Target;
	}

	TSharedPtr<SWidget> BuildSegmented(FSlaterBuildContext& Context)
	{
		const TArray<FString> Options = SegmentedOptions(Context.GetText(TEXT("options")));
		if (Options.IsEmpty())
		{
			Context.Failure = TEXT("<segmented> needs at least one choice in options=\"A, B, C\"");
			return nullptr;
		}
		// `icons` runs parallel to options: a Slater image name, or an engine style brush such as Icons.cylinder.
		TArray<const FSlateBrush*> Icons;
		const FString IconsText = Context.GetText(TEXT("icons"));
		if (!IconsText.IsEmpty())
		{
			TArray<FString> Names;
			IconsText.ParseIntoArray(Names, TEXT(","), false);
			if (Names.Num() != Options.Num())
			{
				Context.Failure = FString::Printf(TEXT("<segmented> icons has %d names but options has %d; give one icon per option"), Names.Num(), Options.Num());
				return nullptr;
			}
			for (FString& Name : Names)
			{
				Name.TrimStartAndEndInline();
				const FSlateBrush* Brush = Name.IsEmpty() ? nullptr : Slater::FindImage(FName(*Name, FNAME_Find));
				if (!Brush && !Name.IsEmpty())
				{
					Brush = FCoreStyle::Get().GetOptionalBrush(FName(*Name), nullptr, nullptr);
				}
				if (!Brush && !Name.IsEmpty())
				{
					Context.Failure = FString::Printf(TEXT("<segmented> icon '%s' is not a known image or engine style brush"), *Name);
					return nullptr;
				}
				Icons.Add(Brush);
			}
		}
		const FString Selected = Context.GetText(TEXT("selected"));
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		const TSharedRef<SSegmentedControl<FString>> Control = SNew(SSegmentedControl<FString>)
			.Value(Options.Contains(Selected) ? Selected : Options[0])
			.OnValueChanged_Lambda([Sink](FString NewValue) { Sink->Fire(Slater::Attr::OnChange, NewValue); });
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			const FSlateBrush* Icon = Icons.IsValidIndex(Index) ? Icons[Index] : nullptr;
			Control->AddSlot(Options[Index], false).Text(FText::FromString(Options[Index])).Icon(Icon);
		}
		Control->RebuildChildren();
		Slater::AttachEventSink(*Control, Sink);
		return Control;
	}

	// ---- checkbox ----

	FSlaterBindTarget CheckBoxCheckedBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SCheckBox&>(Widget).SetIsChecked(Value.bBool ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SCheckBox&>(Widget).SetIsChecked(TAttribute<ECheckBoxState>::CreateLambda([Get = MoveTemp(Get)]()
			{
				return Get().bBool ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}));
		};
		return Target;
	}

	TSharedPtr<SWidget> BuildCheckBox(FSlaterBuildContext& Context)
	{
		// The box is an outlined Control surface that fills with Accent when ticked (no tick glyph: the fill is the mark).
		const TSharedRef<FCheckBoxStyle> BoxStyle = MakeShared<FCheckBoxStyle>(FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"));
		const FSlaterSurface Box = ControlSurface(Context, TEXT("UI.Surface.CheckBox"), MakeControlSurface(FLinearColor::White, FLinearColor(0.55f, 0.58f, 0.63f), 1.f, 4.f));
		const FSlaterSurface BoxHover = ControlSurface(Context, TEXT("UI.Surface.CheckBox.Hover"), Box);
		const FSlaterSurface BoxChecked = ControlSurface(Context, TEXT("UI.Surface.CheckBox.Checked"), MakeControlSurface(FLinearColor(0.18f, 0.43f, 0.92f), FLinearColor(0.18f, 0.43f, 0.92f), 1.f, 4.f));
		const FSlaterSurface BoxCheckedHover = ControlSurface(Context, TEXT("UI.Surface.CheckBox.Checked.Hover"), BoxChecked);
		const FVector2f BoxSize(16.f, 16.f);
		BoxStyle->SetUncheckedImage(ControlBrush(Box, BoxSize));
		BoxStyle->SetUncheckedHoveredImage(ControlBrush(BoxHover, BoxSize));
		BoxStyle->SetUncheckedPressedImage(ControlBrush(BoxHover, BoxSize));
		BoxStyle->SetCheckedImage(ControlBrush(BoxChecked, BoxSize));
		BoxStyle->SetCheckedHoveredImage(ControlBrush(BoxCheckedHover, BoxSize));
		BoxStyle->SetCheckedPressedImage(ControlBrush(BoxCheckedHover, BoxSize));
		BoxStyle->SetUndeterminedImage(ControlBrush(BoxChecked, BoxSize));
		BoxStyle->SetUndeterminedHoveredImage(ControlBrush(BoxCheckedHover, BoxSize));
		BoxStyle->SetUndeterminedPressedImage(ControlBrush(BoxCheckedHover, BoxSize));
		// The engine tints the box brushes with the foreground colors, so they stay white and the brushes carry the color.
		const FSlateColor Untinted(FLinearColor::White);
		BoxStyle->SetForegroundColor(Untinted);
		BoxStyle->SetHoveredForegroundColor(Untinted);
		BoxStyle->SetPressedForegroundColor(Untinted);
		BoxStyle->SetCheckedForegroundColor(Untinted);
		BoxStyle->SetCheckedHoveredForegroundColor(Untinted);
		BoxStyle->SetCheckedPressedForegroundColor(Untinted);
		BoxStyle->SetUndeterminedForegroundColor(Untinted);
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		const FSlaterBuiltChild* Child = Context.Children.Num() > 0 ? &Context.Children[0] : nullptr;
		const TSharedRef<SCheckBox> CheckBox = SNew(SCheckBox)
			.Style(&*BoxStyle)
			.IsChecked(Context.GetBool(TEXT("checked")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.Padding(FMargin(Child ? Child->Slot.Margin : 0.f, 0.f, 0.f, 0.f))
			.OnCheckStateChanged_Lambda([Sink](ECheckBoxState State)
			{
				Sink->Fire(Slater::Attr::OnChange, State == ECheckBoxState::Checked ? TEXT("true") : TEXT("false"));
			})
			[
				Child ? Child->Widget : SNullWidget::NullWidget
			];
		Slater::AttachEventSink(*CheckBox, Sink);
		const TSharedRef<FSlaterControlLook> Look = MakeShared<FSlaterControlLook>();
		Look->CheckBox = BoxStyle;
		KeepControlLook(*CheckBox, Look);
		return CheckBox;
	}

	// ---- radio ----

	/** What a `radio` group shows as chosen. Held as metadata on the group widget so the bind targets can reach it. */
	class FSlaterRadioState : public ISlateMetaData
	{
	public:
		SLATE_METADATA_TYPE(FSlaterRadioState, ISlateMetaData)

		TAttribute<FString> Selected;
		/** True once a polled bind owns Selected, so a click leaves it to the data. */
		bool bPolled = false;
	};

	FSlaterBindTarget RadioSelectedBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			const TSharedPtr<FSlaterRadioState> State = Widget.GetMetaData<FSlaterRadioState>();
			State->Selected = Value.Text;
			State->bPolled = false;
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			const TSharedPtr<FSlaterRadioState> State = Widget.GetMetaData<FSlaterRadioState>();
			State->Selected = TAttribute<FString>::CreateLambda([Get = MoveTemp(Get)]() { return Get().Text; });
			State->bPolled = true;
		};
		return Target;
	}

	TSharedPtr<SWidget> BuildRadio(FSlaterBuildContext& Context)
	{
		const TArray<FString> Options = SegmentedOptions(Context.GetText(TEXT("options")));
		if (Options.IsEmpty())
		{
			Context.Failure = TEXT("<radio> needs at least one choice in options=\"A, B, C\"");
			return nullptr;
		}
		// A round box: a ring in the Accent color around a white middle when chosen, an empty ring otherwise.
		const TSharedRef<FCheckBoxStyle> DotStyle = MakeShared<FCheckBoxStyle>(FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"));
		const FLinearColor AccentColor(0.18f, 0.43f, 0.92f);
		const FLinearColor RingFill = ControlColor(Context, TEXT("UI.Color.Control"), FLinearColor::White);
		const FLinearColor RingLine = ControlColor(Context, TEXT("UI.Color.ControlLine"), FLinearColor(0.55f, 0.58f, 0.63f));
		const FLinearColor LabelColor = ControlColor(Context, TEXT("UI.Color.Text"), FLinearColor::Black);
		const FSlaterSurface Ring = ControlSurface(Context, TEXT("UI.Surface.Radio"), MakeControlSurface(RingFill, RingLine, 1.f, 8.f));
		const FSlaterSurface RingHover = ControlSurface(Context, TEXT("UI.Surface.Radio.Hover"), Ring);
		const FSlaterSurface RingChosen = ControlSurface(Context, TEXT("UI.Surface.Radio.Checked"), MakeControlSurface(FLinearColor::White, AccentColor, 5.f, 8.f));
		const FSlaterSurface RingChosenHover = ControlSurface(Context, TEXT("UI.Surface.Radio.Checked.Hover"), RingChosen);
		const FVector2f DotSize(16.f, 16.f);
		DotStyle->SetUncheckedImage(ControlBrush(Ring, DotSize));
		DotStyle->SetUncheckedHoveredImage(ControlBrush(RingHover, DotSize));
		DotStyle->SetUncheckedPressedImage(ControlBrush(RingHover, DotSize));
		DotStyle->SetCheckedImage(ControlBrush(RingChosen, DotSize));
		DotStyle->SetCheckedHoveredImage(ControlBrush(RingChosenHover, DotSize));
		DotStyle->SetCheckedPressedImage(ControlBrush(RingChosenHover, DotSize));
		DotStyle->SetUndeterminedImage(ControlBrush(RingChosen, DotSize));
		DotStyle->SetUndeterminedHoveredImage(ControlBrush(RingChosenHover, DotSize));
		DotStyle->SetUndeterminedPressedImage(ControlBrush(RingChosenHover, DotSize));
		const FSlateColor Untinted(FLinearColor::White);
		DotStyle->SetForegroundColor(Untinted);
		DotStyle->SetHoveredForegroundColor(Untinted);
		DotStyle->SetPressedForegroundColor(Untinted);
		DotStyle->SetCheckedForegroundColor(Untinted);
		DotStyle->SetCheckedHoveredForegroundColor(Untinted);
		DotStyle->SetCheckedPressedForegroundColor(Untinted);
		DotStyle->SetUndeterminedForegroundColor(Untinted);

		const TSharedRef<FSlaterRadioState> State = MakeShared<FSlaterRadioState>();
		const FString Selected = Context.GetText(TEXT("selected"));
		State->Selected = Options.Contains(Selected) ? Selected : Options[0];
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		const bool bHorizontal = Context.GetEnum(TEXT("direction")) == FName(TEXT("horizontal"));
		const TSharedRef<SPanel> Group = bHorizontal ? StaticCastSharedRef<SPanel>(SNew(SHorizontalBox)) : StaticCastSharedRef<SPanel>(SNew(SVerticalBox));
		for (const FString& Option : Options)
		{
			const TSharedRef<SWidget> Row = SNew(SCheckBox)
				.Style(&*DotStyle)
				.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				.IsChecked_Lambda([State, Option]() { return State->Selected.Get() == Option ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([State, Sink, Option](ECheckBoxState)
				{
					// A radio never unticks itself: pressing the chosen one keeps it.
					if (!State->bPolled)
					{
						State->Selected = Option;
					}
					Sink->Fire(Slater::Attr::OnChange, Option);
				})
				[
					SNew(STextBlock).Text(FText::FromString(Option)).ColorAndOpacity(FSlateColor(LabelColor))
				];
			if (bHorizontal)
			{
				StaticCastSharedRef<SHorizontalBox>(Group)->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)[Row];
			}
			else
			{
				StaticCastSharedRef<SVerticalBox>(Group)->AddSlot().AutoHeight().Padding(0.f, 2.f)[Row];
			}
		}
		Group->AddMetadata(State);
		Slater::AttachEventSink(*Group, Sink);
		const TSharedRef<FSlaterControlLook> Look = MakeShared<FSlaterControlLook>();
		Look->CheckBox = DotStyle;
		KeepControlLook(*Group, Look);
		return Group;
	}

	// ---- textbox ----

	bool IsMultiLineBox(const SWidget& Widget)
	{
		const TSharedPtr<FSlaterControlLook> Look = Widget.GetMetaData<FSlaterControlLook>();
		return Look.IsValid() && Look->bMultiLine;
	}

	FSlaterBindTarget TextBoxTextBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			if (IsMultiLineBox(Widget))
			{
				static_cast<SMultiLineEditableTextBox&>(Widget).SetText(FText::FromString(Value.Text));
				return;
			}
			static_cast<SEditableTextBox&>(Widget).SetText(FText::FromString(Value.Text));
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			const TAttribute<FText> Text = TAttribute<FText>::CreateLambda([Get = MoveTemp(Get)]() { return FText::FromString(Get().Text); });
			if (IsMultiLineBox(Widget))
			{
				static_cast<SMultiLineEditableTextBox&>(Widget).SetText(Text);
				return;
			}
			static_cast<SEditableTextBox&>(Widget).SetText(Text);
		};
		return Target;
	}

	void ResolveTextBoxDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		if (!Values.Contains(TEXT("font")))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Font;
			Style.FindFont(TEXT("UI.Font.Body"), Value.Font);
			Values.Add(TEXT("font"), MoveTemp(Value));
		}
	}

	TSharedPtr<SWidget> BuildTextBox(FSlaterBuildContext& Context)
	{
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		const FSlaterFont Font = Context.GetFont(TEXT("font"));
		// Background and outline are the TextBox surfaces (Hover and Focused swap the outline); text is UI.Color.Text.
		const TSharedRef<FEditableTextBoxStyle> BoxStyle = MakeShared<FEditableTextBoxStyle>(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"));
		const FSlaterSurface Normal = ControlSurface(Context, TEXT("UI.Surface.TextBox"), MakeControlSurface(FLinearColor::White, FLinearColor(0.55f, 0.58f, 0.63f), 1.f, 4.f));
		const FSlaterSurface Hover = ControlSurface(Context, TEXT("UI.Surface.TextBox.Hover"), Normal);
		const FSlaterSurface Focused = ControlSurface(Context, TEXT("UI.Surface.TextBox.Focused"), Normal);
		const FLinearColor TextColor = ControlColor(Context, TEXT("UI.Color.Text"), FLinearColor::Black);
		BoxStyle->SetBackgroundImageNormal(SSlaterSurface::MakeBrush(Normal));
		BoxStyle->SetBackgroundImageHovered(SSlaterSurface::MakeBrush(Hover));
		BoxStyle->SetBackgroundImageFocused(SSlaterSurface::MakeBrush(Focused));
		BoxStyle->SetBackgroundImageReadOnly(SSlaterSurface::MakeBrush(Normal));
		BoxStyle->SetBackgroundColor(FSlateColor(FLinearColor::White));
		BoxStyle->SetForegroundColor(FSlateColor(TextColor));
		BoxStyle->SetReadOnlyForegroundColor(FSlateColor(TextColor));
		BoxStyle->SetFocusedForegroundColor(FSlateColor(TextColor));
		BoxStyle->TextStyle.SetColorAndOpacity(FSlateColor(TextColor));
		const bool bMultiLine = Context.GetBool(TEXT("multiline"));
		if (bMultiLine && Context.GetBool(TEXT("password")))
		{
			Context.Failure = TEXT("<textbox> password and multiline cannot be combined: a password is one line");
			return nullptr;
		}
		const FName Justify = Context.GetEnum(TEXT("justify"), TEXT("left"));
		const ETextJustify::Type Justification = Justify == TEXT("center") ? ETextJustify::Center : Justify == TEXT("right") ? ETextJustify::Right : ETextJustify::Left;
		const bool bReadOnly = Context.GetBool(TEXT("readonly"));
		const bool bSelectAll = Context.GetBool(TEXT("select-all-on-focus"));
		const bool bRevert = Context.GetBool(TEXT("revert-on-escape"));
		const auto OnChanged = [Sink](const FText& Text) { Sink->Fire(Slater::Attr::OnChange, Text.ToString()); };
		// In a multi-line box Enter makes a new line, so on-commit fires only when focus leaves.
		const auto OnCommitted = [Sink](const FText& Text, ETextCommit::Type How)
		{
			// Enter or leaving the box keeps the text; Escape (OnCleared) puts the old text back, which is not a commit.
			if (How == ETextCommit::OnEnter || How == ETextCommit::OnUserMovedFocus)
			{
				Sink->Fire(ControlOnCommit, Text.ToString());
			}
		};
		TSharedPtr<SWidget> Result;
		if (bMultiLine)
		{
			Result = SNew(SMultiLineEditableTextBox)
				.Style(&*BoxStyle)
				.Text(Slater::MakeLiteralText(Context.GetText(TEXT("text")), Context.GetText(TEXT("loc"))))
				.HintText(Slater::MakeLiteralText(Context.GetText(TEXT("hint")), Context.GetText(TEXT("loc")), TEXT(".hint")))
				.Font(FCoreStyle::GetDefaultFontStyle(Font.Role, Font.Size))
				.IsReadOnly(bReadOnly)
				.SelectAllTextWhenFocused(bSelectAll)
				.RevertTextOnEscape(bRevert)
				.Justification(Justification)
				.AutoWrapText(true)
				.OnTextChanged_Lambda(OnChanged)
				.OnTextCommitted_Lambda(OnCommitted);
		}
		else
		{
			Result = SNew(SEditableTextBox)
				.Style(&*BoxStyle)
				.Text(Slater::MakeLiteralText(Context.GetText(TEXT("text")), Context.GetText(TEXT("loc"))))
				.HintText(Slater::MakeLiteralText(Context.GetText(TEXT("hint")), Context.GetText(TEXT("loc")), TEXT(".hint")))
				.Font(FCoreStyle::GetDefaultFontStyle(Font.Role, Font.Size))
				.IsReadOnly(bReadOnly)
				.IsPassword(Context.GetBool(TEXT("password")))
				.SelectAllTextWhenFocused(bSelectAll)
				.RevertTextOnEscape(bRevert)
				.Justification(Justification)
				.OnTextChanged_Lambda(OnChanged)
				.OnTextCommitted_Lambda(OnCommitted);
		}
		Slater::AttachEventSink(*Result, Sink);
		const TSharedRef<FSlaterControlLook> Look = MakeShared<FSlaterControlLook>();
		Look->TextBox = BoxStyle;
		Look->bMultiLine = bMultiLine;
		KeepControlLook(*Result, Look);
		return Result;
	}
}

void Slater::RegisterControlElements(FSlaterElementRegistry& Registry)
{
	FString Error;

	FSlaterElementDesc SliderDesc;
	SliderDesc.Tag = TEXT("slider");
	SliderDesc.Purpose = TEXT("a draggable value between min and max; on-change gets the new value");
	SliderDesc.Attributes = {
		ControlWithBind(ControlAttr(TEXT("value"), ESlaterAttrType::Metric), SliderValueBind()),
		ControlAttr(TEXT("min"), ESlaterAttrType::Metric),
		ControlAttr(TEXT("max"), ESlaterAttrType::Metric),
		ControlAttr(TEXT("step"), ESlaterAttrType::Metric),
		ControlAttr(TEXT("color"), ESlaterAttrType::Color),
		ControlAttr(TEXT("track"), ESlaterAttrType::Color),
		EventAttribute(TEXT("on-change")),
	};
	SliderDesc.Children = ESlaterChildRule::None;
	SliderDesc.ResolveDefaults = &ResolveSliderDefaults;
	SliderDesc.Builder = &BuildSlider;
	const bool bSlider = Registry.Register(MoveTemp(SliderDesc), &Error);
	ensureMsgf(bSlider, TEXT("SlaterUI stock element <slider>: %s"), *Error);

	FSlaterElementDesc SpinBoxDesc;
	SpinBoxDesc.Tag = TEXT("spinbox");
	SpinBoxDesc.Purpose = TEXT("a number to type or drag between min and max; on-change gets every edit, on-commit the value when it is set");
	SpinBoxDesc.Attributes = {
		ControlWithBind(ControlAttr(TEXT("value"), ESlaterAttrType::Metric), SpinBoxValueBind()),
		ControlAttr(TEXT("min"), ESlaterAttrType::Metric),
		ControlAttr(TEXT("max"), ESlaterAttrType::Metric),
		ControlAttr(TEXT("step"), ESlaterAttrType::Metric),
		EventAttribute(TEXT("on-change")),
		EventAttribute(TEXT("on-commit")),
	};
	SpinBoxDesc.Children = ESlaterChildRule::None;
	SpinBoxDesc.ResolveDefaults = &ResolveSpinBoxDefaults;
	SpinBoxDesc.Builder = &BuildSpinBox;
	const bool bSpinBox = Registry.Register(MoveTemp(SpinBoxDesc), &Error);
	ensureMsgf(bSpinBox, TEXT("SlaterUI stock element <spinbox>: %s"), *Error);

	FSlaterAttrDesc OptionsAttr = ControlAttr(TEXT("options"), ESlaterAttrType::Text);
	OptionsAttr.bRequired = true;
	FSlaterElementDesc SegmentedDesc;
	SegmentedDesc.Tag = TEXT("segmented");
	SegmentedDesc.Purpose = TEXT("a row of choices with one selected; options is a comma list, on-change gets the chosen option");
	SegmentedDesc.Attributes = {
		OptionsAttr,
		ControlAttr(TEXT("icons"), ESlaterAttrType::Text),
		ControlWithBind(ControlAttr(TEXT("selected"), ESlaterAttrType::Text), SegmentedSelectedBind()),
		EventAttribute(TEXT("on-change")),
	};
	SegmentedDesc.Children = ESlaterChildRule::None;
	SegmentedDesc.Builder = &BuildSegmented;
	const bool bSegmented = Registry.Register(MoveTemp(SegmentedDesc), &Error);
	ensureMsgf(bSegmented, TEXT("SlaterUI stock element <segmented>: %s"), *Error);

	FSlaterElementDesc CheckBoxDesc;
	CheckBoxDesc.Tag = TEXT("checkbox");
	CheckBoxDesc.Purpose = TEXT("a box to tick, on or off, with an optional label child; on-change gets true or false");
	CheckBoxDesc.Attributes = {
		ControlWithBind(ControlAttr(TEXT("checked"), ESlaterAttrType::Bool), CheckBoxCheckedBind()),
		EventAttribute(TEXT("on-change")),
	};
	CheckBoxDesc.Children = ESlaterChildRule::One;
	CheckBoxDesc.SlotAttrs = ESlaterSlotAttrs::Margin;
	CheckBoxDesc.Builder = &BuildCheckBox;
	const bool bCheckBox = Registry.Register(MoveTemp(CheckBoxDesc), &Error);
	ensureMsgf(bCheckBox, TEXT("SlaterUI stock element <checkbox>: %s"), *Error);

	FSlaterElementDesc RadioDesc;
	RadioDesc.Tag = TEXT("radio");
	RadioDesc.Purpose = TEXT("a group of round boxes with one chosen; options is a comma list, on-change gets the chosen option");
	RadioDesc.Attributes = {
		OptionsAttr,
		ControlWithBind(ControlAttr(TEXT("selected"), ESlaterAttrType::Text), RadioSelectedBind()),
		ControlEnumAttr(TEXT("direction"), {TEXT("vertical"), TEXT("horizontal")}),
		EventAttribute(TEXT("on-change")),
	};
	RadioDesc.Children = ESlaterChildRule::None;
	RadioDesc.Builder = &BuildRadio;
	const bool bRadio = Registry.Register(MoveTemp(RadioDesc), &Error);
	ensureMsgf(bRadio, TEXT("SlaterUI stock element <radio>: %s"), *Error);

	FSlaterElementDesc TextBoxDesc;
	TextBoxDesc.Tag = TEXT("textbox");
	TextBoxDesc.Purpose = TEXT("a box to type text in; on-change gets every edit, on-commit the text on Enter or when focus leaves");
	TextBoxDesc.Attributes = {
		ControlWithBind(ControlAttr(TEXT("text"), ESlaterAttrType::Text), TextBoxTextBind()),
		ControlAttr(TEXT("hint"), ESlaterAttrType::Text),
		ControlAttr(TEXT("font"), ESlaterAttrType::Font),
		ControlAttr(TEXT("password"), ESlaterAttrType::Bool),
		ControlAttr(TEXT("readonly"), ESlaterAttrType::Bool),
		ControlAttr(TEXT("multiline"), ESlaterAttrType::Bool),
		ControlAttr(TEXT("select-all-on-focus"), ESlaterAttrType::Bool),
		ControlAttr(TEXT("revert-on-escape"), ESlaterAttrType::Bool),
		ControlEnumAttr(TEXT("justify"), {TEXT("left"), TEXT("center"), TEXT("right")}),
		EventAttribute(TEXT("on-change")),
		EventAttribute(TEXT("on-commit")),
	};
	TextBoxDesc.Children = ESlaterChildRule::None;
	TextBoxDesc.ResolveDefaults = &ResolveTextBoxDefaults;
	TextBoxDesc.Builder = &BuildTextBox;
	const bool bTextBox = Registry.Register(MoveTemp(TextBoxDesc), &Error);
	ensureMsgf(bTextBox, TEXT("SlaterUI stock element <textbox>: %s"), *Error);
}

TSharedPtr<FSlaterControlLook> Slater::FindControlLook(const SWidget& Widget)
{
	return Widget.GetMetaData<FSlaterControlLook>();
}
