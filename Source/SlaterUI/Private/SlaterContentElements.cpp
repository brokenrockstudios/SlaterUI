// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterElements.h"
#include "SlaterButton.h"
#include "SlaterCommands.h"
#include "SlaterMeter.h"
#include "SlaterOutlet.h"
#include "SlaterSurface.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** State words and the suffix each adds to a surface role; `normal` uses the role itself. */
	const TCHAR* StateSuffix(FName State)
	{
		if (State == TEXT("hover")) return TEXT("Hover");
		if (State == TEXT("pressed")) return TEXT("Pressed");
		if (State == TEXT("disabled")) return TEXT("Disabled");
		return nullptr;
	}

	/** The surface for a role in a state; a state the style does not define resolves to the role through the parent walk. */
	FSlaterSurface ResolveSurface(const ISlaterStyleSource& Style, FName Role, FName State)
	{
		FSlaterSurface Surface;
		Style.FindSurface(Role, Surface);
		if (const TCHAR* Suffix = StateSuffix(State))
		{
			FSlaterSurface Variant;
			if (Style.FindSurface(FName(*FString::Printf(TEXT("%s.%s"), *Role.ToString(), Suffix)), Variant))
			{
				Surface = Variant;
			}
		}
		return Surface;
	}

	TSharedPtr<SWidget> BuildSurface(FSlaterBuildContext& Context)
	{
		static const FName RoleName(TEXT("role"));
		static const FName InsetName(TEXT("inset"));
		// The state variant is already in the role's value (ResolveSurfaceState).
		const FSlaterValue& RoleValue = Context.Values.FindChecked(RoleName);
		const FSlaterSurface& Surface = RoleValue.Surface;
		const FName Role = RoleValue.Word;
		const float InsetValue = Context.GetMetric(InsetName);

		static const FName ShapeName(TEXT("shape"));
		static const FName CutNames[5] = {FName(TEXT("cut")), FName(TEXT("cut-tl")), FName(TEXT("cut-tr")), FName(TEXT("cut-br")), FName(TEXT("cut-bl"))};
		const bool bChamfer = Context.GetEnum(ShapeName, FName(TEXT("rounded"))) == FName(TEXT("chamfer"));
		FSlaterChamfer Chamfer;
		{
			bool bAnyCut = false;
			float Cuts[5] = {};
			for (int32 Index = 0; Index < 5; ++Index)
			{
				const bool bHas = Context.Has(CutNames[Index]);
				bAnyCut |= bHas;
				// A corner attribute that is absent reads -1, so it takes `cut` below.
				Cuts[Index] = bHas ? Context.GetMetric(CutNames[Index]) : (Index == 0 ? 0.f : -1.f);
				if (bHas && Cuts[Index] < 0.f)
				{
					Context.Failure = FString::Printf(TEXT("<surface> %s must not be negative"), *CutNames[Index].ToString());
					return nullptr;
				}
			}
			if (bAnyCut && !bChamfer)
			{
				Context.Failure = TEXT("<surface> cut, cut-tl, cut-tr, cut-br and cut-bl only apply to shape=\"chamfer\"");
				return nullptr;
			}
			Chamfer.TopLeft = Cuts[1] >= 0.f ? Cuts[1] : Cuts[0];
			Chamfer.TopRight = Cuts[2] >= 0.f ? Cuts[2] : Cuts[0];
			Chamfer.BottomRight = Cuts[3] >= 0.f ? Cuts[3] : Cuts[0];
			Chamfer.BottomLeft = Cuts[4] >= 0.f ? Cuts[4] : Cuts[0];
			if (bChamfer && !Chamfer.IsSet())
			{
				Context.Failure = TEXT("<surface shape=\"chamfer\"> needs a cut above 0, for example cut=\"12\" or cut-tl=\"12\"");
				return nullptr;
			}
		}

		// The child's margin and the surface's inset share the border's one padding; they add. Everything goes in through
		// the construction arguments: a setter on a fresh widget only invalidates it.
		const bool bHasChild = Context.Children.Num() > 0;
		const FSlaterBuiltChild* Child = bHasChild ? &Context.Children[0] : nullptr;
		TSharedRef<SSlaterSurface> Widget = SNew(SSlaterSurface)
			.Surface(Surface)
			.Inset(FMargin(InsetValue + (Child ? Child->Slot.Margin : 0.f)))
			[
				Child ? Child->Widget : SNullWidget::NullWidget
			];
		Widget->SetRole(Role);
		if (bChamfer)
		{
			Widget->SetChamfer(Chamfer);
		}
		static const FName OnClickName(TEXT("on-click"));
		static const FName SetStateName(TEXT("set-state"));
		static const FName InteractiveName(TEXT("interactive"));
		static const FName StateName(TEXT("state"));
		const bool bHasCommand = Context.Has(OnClickName);
		if (bHasCommand)
		{
			FString CommandName, CommandArgument;
			Slater::SplitCommandText(Context.GetText(OnClickName), CommandName, CommandArgument);
			Widget->SetCommand(FName(*CommandName), CommandArgument);
		}
		bool bHasAction = bHasCommand;
		FString ClickStateKey, ClickStateValue;
		if (Context.Has(SetStateName) && Slater::ParseStatePair(Context.GetText(SetStateName), ClickStateKey, ClickStateValue))
		{
			Widget->SetStateChange(FName(*ClickStateKey), ClickStateValue);
			bHasAction = true;
		}
		static const FName HoverSetStateName(TEXT("hover-set-state"));
		static const FName LeaveSetStateName(TEXT("leave-set-state"));
		FString PointerKey, PointerValue;
		if (Context.Has(HoverSetStateName) && Slater::ParseStatePair(Context.GetText(HoverSetStateName), PointerKey, PointerValue))
		{
			Widget->SetHoverStateChange(FName(*PointerKey), PointerValue);
		}
		if (Context.Has(LeaveSetStateName) && Slater::ParseStatePair(Context.GetText(LeaveSetStateName), PointerKey, PointerValue))
		{
			Widget->SetLeaveStateChange(FName(*PointerKey), PointerValue);
		}
		// A surface with a command follows the pointer unless `interactive="false"`; `disabled` stays explicit and never reacts.
		const FSlaterValue* StateValue = Context.Values.Find(StateName);
		const bool bDisabled = StateValue && StateValue->Word == TEXT("disabled");
		if (!bDisabled && Context.GetBool(InteractiveName, bHasAction) && Context.Style)
		{
			Widget->SetInteractive(ResolveSurface(*Context.Style, Role, TEXT("hover")), ResolveSurface(*Context.Style, Role, TEXT("pressed")));
		}
		if (Child)
		{
			if (Child->Slot.HAlign.IsSet())
			{
				Widget->SetHAlign(Child->Slot.HAlign.GetValue());
			}
			if (Child->Slot.VAlign.IsSet())
			{
				Widget->SetVAlign(Child->Slot.VAlign.GetValue());
			}
		}
		return Widget;
	}

	TSharedPtr<SWidget> BuildButton(FSlaterBuildContext& Context)
	{
		static const FName RoleName(TEXT("role"));
		static const FName InsetName(TEXT("inset"));
		static const FName OnClickName(TEXT("on-click"));
		static const FName SetStateName(TEXT("set-state"));
		const FSlaterValue& RoleValue = Context.Values.FindChecked(RoleName);
		const FName Role = RoleValue.Word;

		FSlaterButtonLooks Looks;
		Looks.Rest = RoleValue.Surface;
		if (Context.Style)
		{
			Looks.Hover = ResolveSurface(*Context.Style, Role, TEXT("hover"));
			Looks.Pressed = ResolveSurface(*Context.Style, Role, TEXT("pressed"));
			Looks.Disabled = ResolveSurface(*Context.Style, Role, TEXT("disabled"));
			// A style that defines no `.Focused` look shows keyboard focus as hover, the usual gamepad behavior.
			Looks.Focused = Looks.Hover;
			FSlaterSurface Focused;
			if (Context.Style->FindSurface(FName(*FString::Printf(TEXT("%s.Focused"), *Role.ToString())), Focused))
			{
				Looks.Focused = Focused;
			}
		}
		else
		{
			Looks.Hover = Looks.Pressed = Looks.Focused = Looks.Disabled = Looks.Rest;
		}

		const FSlaterBuiltChild* Child = Context.Children.Num() > 0 ? &Context.Children[0] : nullptr;
		const TSharedRef<SSlaterButton> Button = SNew(SSlaterButton)
			.Looks(Looks)
			.Inset(FMargin(Context.GetMetric(InsetName) + (Child ? Child->Slot.Margin : 0.f)))
			[
				Child ? Child->Widget : SNullWidget::NullWidget
			];
		Button->GetFace()->SetRole(Role);
		if (Child)
		{
			if (Child->Slot.HAlign.IsSet())
			{
				Button->GetFace()->SetHAlign(Child->Slot.HAlign.GetValue());
			}
			if (Child->Slot.VAlign.IsSet())
			{
				Button->GetFace()->SetVAlign(Child->Slot.VAlign.GetValue());
			}
		}
		if (Context.Has(OnClickName))
		{
			FString CommandName, CommandArgument;
			Slater::SplitCommandText(Context.GetText(OnClickName), CommandName, CommandArgument);
			Button->SetCommand(FName(*CommandName), CommandArgument);
		}
		FString ClickStateKey, ClickStateValue;
		if (Context.Has(SetStateName) && Slater::ParseStatePair(Context.GetText(SetStateName), ClickStateKey, ClickStateValue))
		{
			Button->SetStateChange(FName(*ClickStateKey), ClickStateValue);
		}
		return Button;
	}

	TSharedPtr<SWidget> BuildOutlet(FSlaterBuildContext& Context)
	{
		const FSlaterValue* Id = Context.Values.Find(TEXT("id"));
		if (!Id || Id->Text.IsEmpty())
		{
			Context.Failure = TEXT("<outlet> needs an id, the name C++ fills it by, for example id=\"Content\"");
			return nullptr;
		}
		const TSharedRef<SWidget> DefaultContent = Context.Children.Num() > 0 ? Context.Children[0].Widget : SNullWidget::NullWidget;
		return SNew(SSlaterOutlet, FName(*Id->Text), DefaultContent);
	}

	TSharedPtr<SWidget> BuildText(FSlaterBuildContext& Context)
	{
		static const FName FontName(TEXT("font"));
		static const FName ColorName(TEXT("color"));
		static const FName WrapName(TEXT("wrap"));
		static const FName JustifyName(TEXT("justify"));
		static const FName Left(TEXT("left"));
		static const FName Center(TEXT("center"));
		static const FName Right(TEXT("right"));
		// Font and color always have a value here: the attribute's, or the style's default (ResolveTextDefaults).
		const FSlaterFont Font = Context.GetFont(FontName);
		const FLinearColor Color = Context.GetColor(ColorName);

		const FName Justify = Context.GetEnum(JustifyName, Left);
		// Bound text (`{Count}`) is written by the binding; until then the text is empty.
		const bool bBound = Slater::IsBindingExpr(Context.Node.Text);
		return SNew(STextBlock)
			.Text(bBound ? FText::GetEmpty() : FText::FromString(Context.Node.Text))
			.Font(FCoreStyle::GetDefaultFontStyle(Font.Role, Font.Size))
			.ColorAndOpacity(FSlateColor(Color))
			.AutoWrapText(Context.GetBool(WrapName, false))
			.Justification(Justify == Center ? ETextJustify::Center : Justify == Right ? ETextJustify::Right : ETextJustify::Left);
	}

	/** Puts the surface the state selects into the role's value, so the builder (once per item) does no token lookups. */
	void ResolveSurfaceState(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		FSlaterValue* RoleValue = Values.Find(TEXT("role"));
		const FSlaterValue* State = Values.Find(TEXT("state"));
		if (RoleValue)
		{
			RoleValue->Surface = ResolveSurface(Style, RoleValue->Word, State ? State->Word : FName(TEXT("normal")));
		}
	}

	/** Fills in the body font and the text color from the style when the attributes are absent. */
	void ResolveTextDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		if (!Values.Contains(TEXT("font")))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Font;
			Style.FindFont(TEXT("UI.Font.Body"), Value.Font);
			Values.Add(TEXT("font"), MoveTemp(Value));
		}
		if (!Values.Contains(TEXT("color")))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Color;
			Value.Color = FLinearColor::White;
			Style.FindColor(TEXT("UI.Color.Text"), Value.Color);
			Values.Add(TEXT("color"), MoveTemp(Value));
		}
	}

	TSharedPtr<SWidget> BuildMeter(FSlaterBuildContext& Context)
	{
		// Value, color and track always have a value here: the attribute's, or the default (ResolveMeterDefaults).
		return SNew(SSlaterMeter)
			.Value(Context.GetMetric(TEXT("value")))
			.FillColor(Context.GetColor(TEXT("color")))
			.TrackColor(Context.GetColor(TEXT("track")));
	}

	TSharedPtr<SWidget> BuildScale(FSlaterBuildContext& Context)
	{
		const FName Stretch = Context.GetEnum(TEXT("stretch"), TEXT("fit"));
		EStretch::Type Mode = EStretch::ScaleToFit;
		if (Stretch == TEXT("none")) Mode = EStretch::None;
		else if (Stretch == TEXT("fill")) Mode = EStretch::ScaleToFill;
		else if (Stretch == TEXT("stretch")) Mode = EStretch::Fill;
		else if (Stretch == TEXT("user")) Mode = EStretch::UserSpecified;
		return SNew(SScaleBox)
			.Stretch(Mode)
			.UserSpecifiedScale(Context.GetMetric(TEXT("scale"), 1.f))
			[
				Context.Children.Num() > 0 ? Context.Children[0].Widget : SNullWidget::NullWidget
			];
	}

	TSharedPtr<SWidget> BuildThrobber(FSlaterBuildContext& Context)
	{
		// The piece circle's radius is half the size; the widget's desired size is the circle plus a piece.
		return SNew(SCircularThrobber)
			.Radius(FMath::Max(Context.GetMetric(TEXT("size")), 1.f) * 0.5f)
			.ColorAndOpacity(FSlateColor(Context.GetColor(TEXT("color"))));
	}

	TSharedPtr<SWidget> BuildRule(FSlaterBuildContext& Context)
	{
		// The width is the parent's to give (a fill alignment stretches it); only the thickness is fixed.
		return SNew(SColorBlock)
			.Color(Context.GetColor(TEXT("color")))
			.Size(FVector2D(1.f, FMath::Max(Context.GetMetric(TEXT("thickness")), 0.f)));
	}

	void AddColorDefault(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values, const TCHAR* Attr, const TCHAR* Token, const FLinearColor& Fallback)
	{
		if (!Values.Contains(Attr))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Color;
			Value.Color = Fallback;
			Style.FindColor(Token, Value.Color);
			Values.Add(Attr, MoveTemp(Value));
		}
	}

	void AddMetricDefault(TMap<FName, FSlaterValue>& Values, const TCHAR* Attr, float Number)
	{
		if (!Values.Contains(Attr))
		{
			FSlaterValue Value;
			Value.Type = ESlaterAttrType::Metric;
			Value.Number = Number;
			Values.Add(Attr, MoveTemp(Value));
		}
	}

	/** The bar takes the accent color on a border-colored track unless the attributes say otherwise. */
	void ResolveMeterDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		AddMetricDefault(Values, TEXT("value"), 0.f);
		AddColorDefault(Style, Values, TEXT("color"), TEXT("UI.Color.Accent"), FLinearColor::White);
		AddColorDefault(Style, Values, TEXT("track"), TEXT("UI.Color.Border"), FLinearColor::Black);
	}

	void ResolveThrobberDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		AddMetricDefault(Values, TEXT("size"), 32.f);
		AddColorDefault(Style, Values, TEXT("color"), TEXT("UI.Color.Accent"), FLinearColor::White);
	}

	void ResolveRuleDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
	{
		AddMetricDefault(Values, TEXT("thickness"), 1.f);
		AddColorDefault(Style, Values, TEXT("color"), TEXT("UI.Color.Border"), FLinearColor::Black);
	}

	FSlaterBindTarget MeterValueBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SSlaterMeter&>(Widget).SetValue(Value.Number);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			// The meter has no Slate attribute for the value; the polled variant exists only to compare cost, so it writes once.
			static_cast<SSlaterMeter&>(Widget).SetValue(Get().Number);
		};
		return Target;
	}

	FSlaterBindTarget SurfaceStateBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource& Style)
		{
			SSlaterSurface& Surface = static_cast<SSlaterSurface&>(Widget);
			Surface.SetSurface(ResolveSurface(Style, Surface.GetRole(), Value.Word));
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource& Style)
		{
			// The brushes of the four states are made once; the getter only picks one, as a bound lambda would.
			SSlaterSurface& Surface = static_cast<SSlaterSurface&>(Widget);
			if (Surface.IsChamfered())
			{
				// A chamfered surface paints itself from its surface values, not from a border brush; only the pushed bind (SetSurface) reaches it.
				Surface.SetSurface(ResolveSurface(Style, Surface.GetRole(), Get().Word));
				return;
			}
			TSharedRef<TMap<FName, FSlateBrush>> Brushes = MakeShared<TMap<FName, FSlateBrush>>();
			for (const TCHAR* State : {TEXT("normal"), TEXT("hover"), TEXT("pressed"), TEXT("disabled")})
			{
				Brushes->Add(State, SSlaterSurface::MakeBrush(ResolveSurface(Style, Surface.GetRole(), State)));
			}
			Surface.SetBorderImage(TAttribute<const FSlateBrush*>::CreateLambda([Brushes, Get = MoveTemp(Get)]() -> const FSlateBrush*
			{
				const FSlateBrush* Brush = Brushes->Find(Get().Word);
				return Brush ? Brush : Brushes->Find(TEXT("normal"));
			}));
		};
		return Target;
	}

	FSlaterBindTarget TextContentBind()
	{
		FSlaterBindTarget Target;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<STextBlock&>(Widget).SetText(FText::FromString(Value.Text));
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<STextBlock&>(Widget).SetText(TAttribute<FText>::CreateLambda([Get = MoveTemp(Get)]() { return FText::FromString(Get().Text); }));
		};
		return Target;
	}

	FSlaterAttrDesc Attr(const TCHAR* Name, ESlaterAttrType Type, std::initializer_list<const TCHAR*> Words = {}, bool bRequired = false)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		for (const TCHAR* Word : Words)
		{
			Desc.EnumValues.Add(Word);
		}
		Desc.bRequired = bRequired;
		return Desc;
	}
}

void Slater::RegisterContentElements(FSlaterElementRegistry& Registry)
{
	FString Error;

	FSlaterElementDesc SurfaceDesc;
	SurfaceDesc.Tag = TEXT("surface");
	SurfaceDesc.Purpose = TEXT("a styled background or button face around one child");
	SurfaceDesc.Attributes = {
		Attr(TEXT("role"), ESlaterAttrType::Surface, {}, true),
		Attr(TEXT("state"), ESlaterAttrType::Enum, {TEXT("normal"), TEXT("hover"), TEXT("pressed"), TEXT("disabled")}),
		Attr(TEXT("inset"), ESlaterAttrType::Metric),
		Attr(TEXT("interactive"), ESlaterAttrType::Bool),
	};
	SurfaceDesc.Attributes[1].Bind = SurfaceStateBind();
	FSlaterAttrDesc OnClick = Attr(TEXT("on-click"), ESlaterAttrType::Text);
	OnClick.CheckText = [](const FString& Text)
	{
		return Slater::CheckCommandText(Text);
	};
	SurfaceDesc.Attributes.Add(MoveTemp(OnClick));
	FSlaterAttrDesc SetState = Attr(TEXT("set-state"), ESlaterAttrType::Text);
	SetState.CheckText = [](const FString& Text)
	{
		return Slater::CheckStatePair(Text, TEXT("set-state"));
	};
	SurfaceDesc.Attributes.Add(MoveTemp(SetState));
	FSlaterAttrDesc HoverSetState = Attr(TEXT("hover-set-state"), ESlaterAttrType::Text);
	HoverSetState.CheckText = [](const FString& Text)
	{
		return Slater::CheckStatePair(Text, TEXT("hover-set-state"));
	};
	SurfaceDesc.Attributes.Add(MoveTemp(HoverSetState));
	FSlaterAttrDesc LeaveSetState = Attr(TEXT("leave-set-state"), ESlaterAttrType::Text);
	LeaveSetState.CheckText = [](const FString& Text)
	{
		return Slater::CheckStatePair(Text, TEXT("leave-set-state"));
	};
	SurfaceDesc.Attributes.Add(MoveTemp(LeaveSetState));
	SurfaceDesc.Attributes.Add(Attr(TEXT("shape"), ESlaterAttrType::Enum, {TEXT("rounded"), TEXT("chamfer")}));
	for (const TCHAR* Cut : {TEXT("cut"), TEXT("cut-tl"), TEXT("cut-tr"), TEXT("cut-br"), TEXT("cut-bl")})
	{
		SurfaceDesc.Attributes.Add(Attr(Cut, ESlaterAttrType::Metric));
	}
	SurfaceDesc.Children = ESlaterChildRule::One;
	SurfaceDesc.SlotAttrs = ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign;
	SurfaceDesc.ResolveDefaults = &ResolveSurfaceState;
	SurfaceDesc.Builder = &BuildSurface;
	const bool bSurface = Registry.Register(MoveTemp(SurfaceDesc), &Error);
	ensureMsgf(bSurface, TEXT("SlaterUI stock element <surface>: %s"), *Error);

	FSlaterElementDesc ButtonDesc;
	ButtonDesc.Tag = TEXT("button");
	ButtonDesc.Purpose = TEXT("a button that takes focus: Enter, Space or the gamepad confirm key press it like a click does");
	ButtonDesc.Attributes = {
		Attr(TEXT("role"), ESlaterAttrType::Surface, {}, true),
		Attr(TEXT("inset"), ESlaterAttrType::Metric),
	};
	FSlaterAttrDesc ButtonOnClick = Attr(TEXT("on-click"), ESlaterAttrType::Text);
	ButtonOnClick.CheckText = [](const FString& Text)
	{
		return Slater::CheckCommandText(Text);
	};
	ButtonDesc.Attributes.Add(MoveTemp(ButtonOnClick));
	FSlaterAttrDesc ButtonSetState = Attr(TEXT("set-state"), ESlaterAttrType::Text);
	ButtonSetState.CheckText = [](const FString& Text)
	{
		return Slater::CheckStatePair(Text, TEXT("set-state"));
	};
	ButtonDesc.Attributes.Add(MoveTemp(ButtonSetState));
	ButtonDesc.Children = ESlaterChildRule::One;
	ButtonDesc.SlotAttrs = ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign;
	ButtonDesc.Builder = &BuildButton;
	const bool bButton = Registry.Register(MoveTemp(ButtonDesc), &Error);
	ensureMsgf(bButton, TEXT("SlaterUI stock element <button>: %s"), *Error);

	FSlaterElementDesc OutletDesc;
	OutletDesc.Tag = TEXT("outlet");
	OutletDesc.Purpose = TEXT("a named place the game code fills with its own widget; the child shows until then");
	OutletDesc.Children = ESlaterChildRule::One;
	OutletDesc.Builder = &BuildOutlet;
	const bool bOutlet = Registry.Register(MoveTemp(OutletDesc), &Error);
	ensureMsgf(bOutlet, TEXT("SlaterUI stock element <outlet>: %s"), *Error);

	FSlaterElementDesc TextDesc;
	TextDesc.Tag = TEXT("text");
	TextDesc.Purpose = TEXT("words and numbers shown on screen, what other toolkits call a label");
	TextDesc.Attributes = {
		Attr(TEXT("font"), ESlaterAttrType::Font),
		Attr(TEXT("color"), ESlaterAttrType::Color),
		Attr(TEXT("wrap"), ESlaterAttrType::Bool),
		Attr(TEXT("justify"), ESlaterAttrType::Enum, {TEXT("left"), TEXT("center"), TEXT("right")}),
	};
	TextDesc.Children = ESlaterChildRule::None;
	TextDesc.SlotAttrs = ESlaterSlotAttrs::None;
	TextDesc.bAllowsText = true;
	TextDesc.TextBind = TextContentBind();
	TextDesc.ResolveDefaults = &ResolveTextDefaults;
	TextDesc.Builder = &BuildText;
	const bool bText = Registry.Register(MoveTemp(TextDesc), &Error);
	ensureMsgf(bText, TEXT("SlaterUI stock element <text>: %s"), *Error);

	FSlaterElementDesc MeterDesc;
	MeterDesc.Tag = TEXT("meter");
	MeterDesc.Purpose = TEXT("a progress or fill bar, filled from the left to a value from 0 to 1");
	MeterDesc.Attributes = {
		Attr(TEXT("value"), ESlaterAttrType::Metric),
		Attr(TEXT("color"), ESlaterAttrType::Color),
		Attr(TEXT("track"), ESlaterAttrType::Color),
	};
	MeterDesc.Attributes[0].Bind = MeterValueBind();
	MeterDesc.Children = ESlaterChildRule::None;
	MeterDesc.ResolveDefaults = &ResolveMeterDefaults;
	MeterDesc.Builder = &BuildMeter;
	const bool bMeter = Registry.Register(MoveTemp(MeterDesc), &Error);
	ensureMsgf(bMeter, TEXT("SlaterUI stock element <meter>: %s"), *Error);

	FSlaterElementDesc ScaleDesc;
	ScaleDesc.Tag = TEXT("scale");
	ScaleDesc.Purpose = TEXT("scales its one child to the room it is given: stretch=none, fit (default), fill, stretch or user (with scale)");
	ScaleDesc.Attributes = {
		Attr(TEXT("stretch"), ESlaterAttrType::Enum, {TEXT("none"), TEXT("fit"), TEXT("fill"), TEXT("stretch"), TEXT("user")}),
		Attr(TEXT("scale"), ESlaterAttrType::Metric),
	};
	ScaleDesc.Children = ESlaterChildRule::One;
	ScaleDesc.Builder = &BuildScale;
	const bool bScale = Registry.Register(MoveTemp(ScaleDesc), &Error);
	ensureMsgf(bScale, TEXT("SlaterUI stock element <scale>: %s"), *Error);

	FSlaterElementDesc ThrobberDesc;
	ThrobberDesc.Tag = TEXT("throbber");
	ThrobberDesc.Purpose = TEXT("a spinning loading indicator");
	ThrobberDesc.Attributes = {
		Attr(TEXT("color"), ESlaterAttrType::Color),
		Attr(TEXT("size"), ESlaterAttrType::Metric),
	};
	ThrobberDesc.Children = ESlaterChildRule::None;
	ThrobberDesc.ResolveDefaults = &ResolveThrobberDefaults;
	ThrobberDesc.Builder = &BuildThrobber;
	const bool bThrobber = Registry.Register(MoveTemp(ThrobberDesc), &Error);
	ensureMsgf(bThrobber, TEXT("SlaterUI stock element <throbber>: %s"), *Error);

	FSlaterElementDesc RuleDesc;
	RuleDesc.Tag = TEXT("rule");
	RuleDesc.Purpose = TEXT("a thin horizontal line, a divider");
	RuleDesc.Attributes = {
		Attr(TEXT("color"), ESlaterAttrType::Color),
		Attr(TEXT("thickness"), ESlaterAttrType::Metric),
	};
	RuleDesc.Children = ESlaterChildRule::None;
	RuleDesc.ResolveDefaults = &ResolveRuleDefaults;
	RuleDesc.Builder = &BuildRule;
	const bool bRule = Registry.Register(MoveTemp(RuleDesc), &Error);
	ensureMsgf(bRule, TEXT("SlaterUI stock element <rule>: %s"), *Error);
}
