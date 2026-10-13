// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterElements.h"
#include "SlaterCommands.h"
#include "SlaterOutlet.h"
#include "SlaterSurface.h"

#include "Styling/CoreStyle.h"
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
	SurfaceDesc.Children = ESlaterChildRule::One;
	SurfaceDesc.SlotAttrs = ESlaterSlotAttrs::Margin | ESlaterSlotAttrs::HAlign | ESlaterSlotAttrs::VAlign;
	SurfaceDesc.ResolveDefaults = &ResolveSurfaceState;
	SurfaceDesc.Builder = &BuildSurface;
	const bool bSurface = Registry.Register(MoveTemp(SurfaceDesc), &Error);
	ensureMsgf(bSurface, TEXT("SlaterUI stock element <surface>: %s"), *Error);

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
}
