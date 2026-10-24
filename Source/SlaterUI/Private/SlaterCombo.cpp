// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterCombo.h"
#include "SlaterAttrNames.h"

#include "SlaterElements.h"
#include "SlaterEvents.h"
#include "SlaterSurface.h"

#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "SlaterList.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterCombo, Log, All);

namespace SlaterComboPrivate
{
	const FName ComboOptionsAttr(TEXT("options"));

	FSlaterAttrDesc ComboAttr(const TCHAR* Name, ESlaterAttrType Type)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		return Desc;
	}

	FSlaterSurface ComboSurface(const FSlaterBuildContext& Context, const TCHAR* Token, const FSlaterSurface& Fallback)
	{
		FSlaterSurface Found;
		return Context.Style && Context.Style->FindSurface(Token, Found) ? Found : Fallback;
	}

	FSlaterSurface MakeComboSurface(const FLinearColor& Fill, const FLinearColor& Border, float BorderWidth, float Radius)
	{
		FSlaterSurface Surface;
		Surface.Fill = Fill;
		Surface.Border = Border;
		Surface.BorderWidth = BorderWidth;
		Surface.CornerRadius = Radius;
		return Surface;
	}

	FLinearColor ComboColor(const FSlaterBuildContext& Context, const TCHAR* Token, const FLinearColor& Fallback)
	{
		FLinearColor Found;
		return Context.Style && Context.Style->FindColor(Token, Found) ? Found : Fallback;
	}

	/** The metadata of an `<option>` widget, looking through the sizing box a common attribute adds around it. */
	TSharedPtr<FSlaterComboOptionMeta> FindOptionMeta(const TSharedRef<SWidget>& Widget)
	{
		if (const TSharedPtr<FSlaterComboOptionMeta> Meta = Widget->GetMetaData<FSlaterComboOptionMeta>())
		{
			return Meta;
		}
		TSharedPtr<FSlaterComboOptionMeta> Found;
		if (FChildren* Children = Widget->GetChildren())
		{
			Children->ForEachWidget([&Found](SWidget& Child)
			{
				if (!Found.IsValid())
				{
					Found = Child.GetMetaData<FSlaterComboOptionMeta>();
				}
			});
		}
		return Found;
	}

	// ---- option ----

	TSharedPtr<SWidget> BuildOption(FSlaterBuildContext& Context)
	{
		const FString Value = Context.GetText(TEXT("value"));
		if (Value.IsEmpty())
		{
			Context.Failure = TEXT("<option> needs a value: the text the data holds when this choice is picked");
			return nullptr;
		}
		const TSharedRef<FSlaterComboOptionMeta> Meta = MakeShared<FSlaterComboOptionMeta>();
		Meta->Option.Value = Value;
		Meta->Option.Label = Context.Node.Text.IsEmpty() ? Value : Context.Node.Text;
		Meta->Option.Icon = Context.GetText(TEXT("icon"));
		// Draws nothing: the parent `combo` reads the metadata. Outside a combo an option is an empty box.
		const TSharedRef<SBox> Box = SNew(SBox);
		Box->AddMetadata(Meta);
		return Box;
	}

	// ---- combo ----

	FSlaterBindTarget ComboSelectedBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SSlaterCombo&>(Widget).SetSelectedValue(Value.Text);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SSlaterCombo&>(Widget).SetSelectedGetter([Get = MoveTemp(Get)]() { return Get().Text; });
		};
		return Target;
	}

	FSlaterBindTarget ComboOptionsBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			TArray<FSlaterComboOption> Parsed;
			FString Error;
			if (SSlaterCombo::ParseOptions(Value.Text, Parsed, Error))
			{
				static_cast<SSlaterCombo&>(Widget).SetOptions(Parsed);
			}
			else
			{
				UE_LOG(LogSlaterCombo, Warning, TEXT("<combo> options \"%s\": %s"), *Value.Text, *Error);
			}
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SSlaterCombo&>(Widget).SetOptionsGetter([Get = MoveTemp(Get)]() { return Get().Text; });
		};
		return Target;
	}

	void ResolveComboDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		if (!Values.Contains(TEXT("font")))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Font;
			Style.FindFont(TEXT("UI.Font.Body"), Value.Font);
			Values.Add(TEXT("font"), MoveTemp(Value));
		}
	}

	TSharedPtr<SWidget> BuildCombo(FSlaterBuildContext& Context)
	{
		TArray<FSlaterComboOption> Options;
		for (const FSlaterBuiltChild& Child : Context.Children)
		{
			const TSharedPtr<FSlaterComboOptionMeta> Meta = FindOptionMeta(Child.Widget);
			if (!Meta.IsValid())
			{
				Context.Failure = FString::Printf(TEXT("<combo> children must be <option> elements (found <%s>)"), Child.Node ? *Child.Node->Tag : TEXT("?"));
				return nullptr;
			}
			Options.Add(Meta->Option);
		}
		if (Context.Has(ComboOptionsAttr))
		{
			if (Options.Num() > 0)
			{
				Context.Failure = TEXT("<combo> has both an options attribute and <option> children; use one of them");
				return nullptr;
			}
			FString Error;
			if (!SSlaterCombo::ParseOptions(Context.GetText(ComboOptionsAttr), Options, Error))
			{
				Context.Failure = FString::Printf(TEXT("<combo> options: %s"), *Error);
				return nullptr;
			}
		}
		else
		{
			TSet<FString> Seen;
			for (const FSlaterComboOption& Option : Options)
			{
				bool bAlready = false;
				Seen.Add(Option.Value, &bAlready);
				if (bAlready)
				{
					Context.Failure = FString::Printf(TEXT("<combo> has two <option>s with the value '%s'; a value names one choice"), *Option.Value);
					return nullptr;
				}
			}
		}
		const FString Selected = Context.GetText(TEXT("selected"));
		if (Context.Has(TEXT("selected")) && !Selected.IsEmpty() && !Options.ContainsByPredicate([&Selected](const FSlaterComboOption& Option) { return Option.Value == Selected; }))
		{
			TArray<FString> Values;
			for (const FSlaterComboOption& Option : Options)
			{
				Values.Add(Option.Value);
			}
			Context.Failure = FString::Printf(TEXT("<combo> selected '%s' is not one of its options (%s)"), *Selected, *FString::Join(Values, TEXT(", ")));
			return nullptr;
		}

		const bool bIconOnly = Context.GetBool(TEXT("icon-only"), false);
		for (const FSlaterComboOption& Option : Options)
		{
			if (bIconOnly && Option.Icon.IsEmpty())
			{
				Context.Failure = FString::Printf(TEXT("<combo icon-only=\"true\"> option '%s' has no icon; every option needs one (icon=\"Home\")"), *Option.Value);
				return nullptr;
			}
			if (!bIconOnly && !Option.Icon.IsEmpty())
			{
				Context.Failure = FString::Printf(TEXT("<option> '%s' has an icon but its <combo> is not icon-only; add icon-only=\"true\" to the combo"), *Option.Value);
				return nullptr;
			}
			if (bIconOnly && Slater::HasImageSource() && !Slater::FindImage(FName(*Option.Icon, FNAME_Find)))
			{
				Context.Failure = FString::Printf(TEXT("<option> '%s' icon '%s' is not a known image"), *Option.Value, *Option.Icon);
				return nullptr;
			}
		}

		// The closed box is the TextBox surface (Hover and Focused swap it) and the menu the dark Menu surface; text is UI.Color.Text.
		const TSharedRef<FComboBoxStyle> BoxStyle = MakeShared<FComboBoxStyle>(FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox"));
		const FSlaterSurface Normal = ComboSurface(Context, TEXT("UI.Surface.TextBox"), MakeComboSurface(FLinearColor::White, FLinearColor(0.55f, 0.58f, 0.63f), 1.f, 4.f));
		const FSlaterSurface Hover = ComboSurface(Context, TEXT("UI.Surface.TextBox.Hover"), Normal);
		const FSlaterSurface Pressed = ComboSurface(Context, TEXT("UI.Surface.TextBox.Focused"), Normal);
		const FSlaterSurface Menu = ComboSurface(Context, TEXT("UI.Surface.Menu"), MakeComboSurface(FLinearColor(0.15f, 0.16f, 0.2f), FLinearColor(0.23f, 0.25f, 0.29f), 1.f, 4.f));
		const FLinearColor TextColor = ComboColor(Context, TEXT("UI.Color.Text"), FLinearColor::Black);
		const FLinearColor MenuTextColor = ComboColor(Context, TEXT("UI.Color.MenuText"), FLinearColor::White);
		FButtonStyle& Button = BoxStyle->ComboButtonStyle.ButtonStyle;
		Button.SetNormal(SSlaterSurface::MakeBrush(Normal));
		Button.SetHovered(SSlaterSurface::MakeBrush(Hover));
		Button.SetPressed(SSlaterSurface::MakeBrush(Pressed));
		Button.SetDisabled(SSlaterSurface::MakeBrush(Normal));
		// The engine tints the arrow with the foreground; the surface brushes carry their own color.
		Button.SetNormalForeground(FSlateColor(TextColor));
		Button.SetHoveredForeground(FSlateColor(TextColor));
		Button.SetPressedForeground(FSlateColor(TextColor));
		Button.SetDisabledForeground(FSlateColor(TextColor));
		BoxStyle->ComboButtonStyle.SetMenuBorderBrush(SSlaterSurface::MakeBrush(Menu));

		const FSlaterFont Font = Context.GetFont(TEXT("font"));
		const TSharedRef<FSlaterEventSink> Sink = MakeShared<FSlaterEventSink>();
		const TSharedRef<SSlaterCombo> Combo = SNew(SSlaterCombo)
			.Style(&*BoxStyle)
			.Font(FCoreStyle::GetDefaultFontStyle(Font.Role, Font.Size))
			.TextColor(TextColor)
			.MenuTextColor(MenuTextColor)
			.Hint(Context.GetText(TEXT("hint")))
			.IconOnly(bIconOnly)
			.IconSize(Context.Has(TEXT("icon-size")) ? Context.GetMetric(TEXT("icon-size")) : 16.f);
		Combo->SetEventSink(Sink);
		Combo->SetOptions(Options);
		Combo->SetSelectedValue(Selected);
		Slater::AttachEventSink(*Combo, Sink);
		const TSharedRef<FSlaterComboLook> Look = MakeShared<FSlaterComboLook>();
		Look->Style = BoxStyle;
		Combo->AddMetadata(Look);
		return Combo;
	}
}
using namespace SlaterComboPrivate;

void SSlaterCombo::Construct(const FArguments& InArgs)
{
	Hint = InArgs._Hint;
	Font = InArgs._Font;
	TextColor = InArgs._TextColor;
	MenuTextColor = InArgs._MenuTextColor;
	bIconOnly = InArgs._IconOnly;
	IconSize = InArgs._IconSize;
	const TSharedRef<SWidget> Shown = bIconOnly
		? StaticCastSharedRef<SWidget>(SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.WidthOverride(IconSize)
			.HeightOverride(IconSize)
			[
				SNew(SImage)
				.Image(this, &SSlaterCombo::GetShownBrush)
				.ColorAndOpacity(FSlateColor(TextColor))
			])
		: StaticCastSharedRef<SWidget>(SNew(STextBlock)
			.Text(this, &SSlaterCombo::GetShownText)
			.Font(Font)
			.ColorAndOpacity(FSlateColor(TextColor)));
	ComboBox = SNew(SComboBox<FOptionPtr>)
		.ComboBoxStyle(InArgs._Style)
		.OptionsSource(&Options)
		.OnGenerateWidget(this, &SSlaterCombo::MakeRow)
		.OnSelectionChanged(this, &SSlaterCombo::HandleSelectionChanged)
		[
			Shown
		];
	// T-439: a menu must flip at the window edge rather than run off it (the engine default is on; say it so a change in the engine shows here).
	ComboBox->SetFitInWindow(true);
	bFitsInWindow = true;
	ChildSlot
	[
		ComboBox.ToSharedRef()
	];
}

TSharedRef<SWidget> SSlaterCombo::MakeRow(FOptionPtr Option) const
{
	if (bIconOnly)
	{
		const FSlateBrush* Brush = Option.IsValid() ? Slater::FindImage(FName(*Option->Icon, FNAME_Find)) : nullptr;
		// The menu stretches a row to its width; an image in it would stretch too, so pin the icon to its size.
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.Padding(FMargin(4.f, 3.f))
			[
				SNew(SBox)
				.WidthOverride(IconSize)
				.HeightOverride(IconSize)
				[
					SNew(SImage)
					.Image(Brush)
					.ColorAndOpacity(FSlateColor(MenuTextColor))
				]
			];
	}
	return SNew(STextBlock)
		.Text(FText::FromString(Option.IsValid() ? Option->Label : FString()))
		.Font(Font)
		.ColorAndOpacity(FSlateColor(MenuTextColor));
}

bool SSlaterCombo::ParseOptions(const FString& Text, TArray<FSlaterComboOption>& OutOptions, FString& OutError)
{
	OutOptions.Reset();
	TArray<FString> Entries;
	Text.ParseIntoArray(Entries, TEXT(","), true);
	TSet<FString> Seen;
	for (FString Entry : Entries)
	{
		Entry.TrimStartAndEndInline();
		if (Entry.IsEmpty())
		{
			continue;
		}
		FSlaterComboOption Option;
		if (!Entry.Split(TEXT("|"), &Option.Value, &Option.Label))
		{
			Option.Value = Entry;
			Option.Label = Entry;
		}
		Option.Value.TrimStartAndEndInline();
		Option.Label.TrimStartAndEndInline();
		if (Option.Value.IsEmpty())
		{
			OutError = FString::Printf(TEXT("the entry '%s' has an empty value before the '|'"), *Entry);
			OutOptions.Reset();
			return false;
		}
		if (Option.Label.IsEmpty())
		{
			Option.Label = Option.Value;
		}
		bool bAlready = false;
		Seen.Add(Option.Value, &bAlready);
		if (bAlready)
		{
			OutError = FString::Printf(TEXT("two entries have the value '%s'; a value names one choice"), *Option.Value);
			OutOptions.Reset();
			return false;
		}
		OutOptions.Add(MoveTemp(Option));
	}
	return true;
}

void SSlaterCombo::SetOptions(const TArray<FSlaterComboOption>& InOptions)
{
	Options.Reset();
	for (const FSlaterComboOption& Option : InOptions)
	{
		Options.Add(MakeShared<FSlaterComboOption>(Option));
	}
	// The remembered value picks its new option up (or finds none and falls back to the hint).
	SetSelectedValue(PendingValue);
	ComboBox->RefreshOptions();
}

void SSlaterCombo::SetSelectedValue(const FString& InValue)
{
	PendingValue = InValue;
	FOptionPtr Found;
	if (!InValue.IsEmpty())
	{
		for (const FOptionPtr& Option : Options)
		{
			if (Option->Value == InValue)
			{
				Found = Option;
				break;
			}
		}
	}
	Selected = Found;
	if (ComboBox.IsValid())
	{
		// The list reports its own selection change; that is this call, not the user.
		TGuardValue<bool> FromCode(bSettingFromCode, true);
		ComboBox->SetSelectedItem(Selected);
	}
}

void SSlaterCombo::SetSelectedGetter(TFunction<FString()> InGetter)
{
	SelectedGetter = MoveTemp(InGetter);
	SetSelectedValue(SelectedGetter());
}

void SSlaterCombo::SetOptionsGetter(TFunction<FString()> InGetter)
{
	OptionsGetter = MoveTemp(InGetter);
	LastOptionsText = OptionsGetter();
	TArray<FSlaterComboOption> Parsed;
	FString Error;
	if (ParseOptions(LastOptionsText, Parsed, Error))
	{
		SetOptions(Parsed);
	}
}

FString SSlaterCombo::GetSelectedValue() const
{
	return Selected.IsValid() ? Selected->Value : FString();
}

bool SSlaterCombo::IsMenuOpen() const
{
	return ComboBox.IsValid() && ComboBox->IsOpen();
}

FText SSlaterCombo::GetShownText() const
{
	return FText::FromString(Selected.IsValid() ? Selected->Label : Hint);
}

const FSlateBrush* SSlaterCombo::GetShownBrush() const
{
	return Selected.IsValid() ? Slater::FindImage(FName(*Selected->Icon, FNAME_Find)) : nullptr;
}

void SSlaterCombo::PickOption(const FString& InValue)
{
	for (const FOptionPtr& Option : Options)
	{
		if (Option->Value == InValue)
		{
			HandleSelectionChanged(Option, ESelectInfo::Direct);
			return;
		}
	}
}

void SSlaterCombo::HandleSelectionChanged(FOptionPtr Picked, ESelectInfo::Type How)
{
	if (bSettingFromCode || !Picked.IsValid())
	{
		return;
	}
	Selected = Picked;
	PendingValue = Picked->Value;
	if (const TSharedPtr<FSlaterEventSink> Pinned = Sink.Pin())
	{
		Pinned->Fire(Slater::Attr::OnChange, Picked->Value);
	}
}

void SSlaterCombo::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (OptionsGetter)
	{
		const FString Text = OptionsGetter();
		if (Text != LastOptionsText)
		{
			LastOptionsText = Text;
			TArray<FSlaterComboOption> Parsed;
			FString Error;
			if (ParseOptions(Text, Parsed, Error))
			{
				SetOptions(Parsed);
			}
		}
	}
	if (SelectedGetter)
	{
		const FString Value = SelectedGetter();
		if (Value != PendingValue)
		{
			SetSelectedValue(Value);
		}
	}
}

TSharedPtr<FSlaterComboLook> Slater::FindComboLook(const SWidget& Widget)
{
	return Widget.GetMetaData<FSlaterComboLook>();
}

void Slater::RegisterComboElements(FSlaterElementRegistry& Registry)
{
	FString Error;

	FSlaterElementDesc OptionDesc;
	OptionDesc.Tag = TEXT("option");
	OptionDesc.Purpose = TEXT("one choice of a combo: value is what the data holds, the text is what the user sees");
	FSlaterAttrDesc OptionValue = ComboAttr(TEXT("value"), ESlaterAttrType::Text);
	OptionValue.bRequired = true;
	OptionDesc.Attributes = {OptionValue, ComboAttr(TEXT("icon"), ESlaterAttrType::Text)};
	OptionDesc.Children = ESlaterChildRule::None;
	OptionDesc.bAllowsText = true;
	OptionDesc.Builder = &BuildOption;
	const bool bOption = Registry.Register(MoveTemp(OptionDesc), &Error);
	ensureMsgf(bOption, TEXT("SlaterUI stock element <option>: %s"), *Error);

	FSlaterElementDesc ComboDesc;
	ComboDesc.Tag = TEXT("combo");
	ComboDesc.Purpose = TEXT("a pick-one list that drops down; its <option> children are the choices, selected holds the chosen value, on-change gets the new value");
	FSlaterAttrDesc Selected = ComboAttr(TEXT("selected"), ESlaterAttrType::Text);
	Selected.Bind = ComboSelectedBind();
	FSlaterAttrDesc OptionsAttr = ComboAttr(TEXT("options"), ESlaterAttrType::Text);
	OptionsAttr.Bind = ComboOptionsBind();
	ComboDesc.Attributes = {
		Selected,
		OptionsAttr,
		ComboAttr(TEXT("hint"), ESlaterAttrType::Text),
		ComboAttr(TEXT("font"), ESlaterAttrType::Font),
		ComboAttr(TEXT("icon-only"), ESlaterAttrType::Bool),
		ComboAttr(TEXT("icon-size"), ESlaterAttrType::Metric),
		Slater::EventAttribute(TEXT("on-change")),
	};
	ComboDesc.Children = ESlaterChildRule::Many;
	ComboDesc.ResolveDefaults = &ResolveComboDefaults;
	ComboDesc.Builder = &BuildCombo;
	const bool bCombo = Registry.Register(MoveTemp(ComboDesc), &Error);
	ensureMsgf(bCombo, TEXT("SlaterUI stock element <combo>: %s"), *Error);
}
