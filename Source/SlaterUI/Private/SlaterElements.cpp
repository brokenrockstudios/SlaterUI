// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterElements.h"
#include "SlaterAttrNames.h"
#include "SlaterTemplates.h"

#include "SlaterCombo.h"
#include "SlaterCommands.h"
#include "SlaterEvents.h"
#include "SlaterOutlet.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"

const TCHAR* LexToString(ESlaterAttrType Type)
{
	switch (Type)
	{
	case ESlaterAttrType::Metric: return TEXT("metric");
	case ESlaterAttrType::Color: return TEXT("color");
	case ESlaterAttrType::Font: return TEXT("font");
	case ESlaterAttrType::Surface: return TEXT("surface");
	case ESlaterAttrType::Enum: return TEXT("enum");
	case ESlaterAttrType::Bool: return TEXT("bool");
	case ESlaterAttrType::Int: return TEXT("int");
	case ESlaterAttrType::Text: return TEXT("text");
	}
	return TEXT("value");
}

float FSlaterBuildContext::GetMetric(FName Name, float Default) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Number : Default;
}

FLinearColor FSlaterBuildContext::GetColor(FName Name, const FLinearColor& Default) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Color : Default;
}

FSlaterFont FSlaterBuildContext::GetFont(FName Name) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Font : FSlaterFont();
}

FSlaterSurface FSlaterBuildContext::GetSurface(FName Name) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Surface : FSlaterSurface();
}

bool FSlaterBuildContext::GetBool(FName Name, bool Default) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->bBool : Default;
}

int32 FSlaterBuildContext::GetInt(FName Name, int32 Default) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Integer : Default;
}

FName FSlaterBuildContext::GetEnum(FName Name, FName Default) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Word : Default;
}

FString FSlaterBuildContext::GetText(FName Name) const
{
	const FSlaterValue* Value = Values.Find(Name);
	return Value ? Value->Text : FString();
}

/** One element after its attributes were checked: everything an instance needs except the widgets. */
struct FSlaterCompiledTemplate::FNode
{
	/** Points into the registry for a layout build, at OwnedDesc for a template (a copy, so a registry that changes cannot dangle it). */
	const FSlaterElementDesc* Desc = nullptr;
	TSharedPtr<const FSlaterElementDesc> OwnedDesc;
	TMap<FName, FSlaterValue> Values;
	FSlaterSlotValues Slot;
	FName IdName;
	TArray<TSharedRef<const FSlaterBindingSpec>> Bindings;
	/** Element events (T-381), handed to the widget's event sink per instance. */
	TArray<TSharedRef<const FSlaterEventSpec>> Events;
	bool bNeedsBox = false;
	/** `width-by` / `height-by`, resolved (T-405); the box is set per instance. */
	TArray<FSlaterStateSize> SizeBy;
	/** Kept only for a template; the same order as the node's children. */
	TArray<TSharedRef<FNode>> Children;
};

FSlaterCompiledTemplate::FSlaterCompiledTemplate() = default;
FSlaterCompiledTemplate::~FSlaterCompiledTemplate() = default;

namespace
{
	using FResolvedNode = FSlaterCompiledTemplate::FNode;

	const FName AttrId(TEXT("id"));
	const FName AttrWidth(TEXT("width"));
	const FName AttrHeight(TEXT("height"));
	const FName AttrMinWidth(TEXT("min-width"));
	const FName AttrMinHeight(TEXT("min-height"));
	const FName AttrMaxWidth(TEXT("max-width"));
	const FName AttrMaxHeight(TEXT("max-height"));
	const FName AttrPadding(TEXT("padding"));
	const FName AttrVisible(TEXT("visible"));
	const FName AttrEnabled(TEXT("enabled"));
	const FName AttrVisibleWhen(TEXT("visible-when"));
	const FName AttrWidthBy(TEXT("width-by"));
	const FName AttrHeightBy(TEXT("height-by"));
	const FName AttrClip(TEXT("clip"));
	const FName AttrTransition(TEXT("transition"));
	const FName AttrFill(TEXT("fill"));
	const FName AttrMargin(TEXT("margin"));
	const FName AttrHAlign(TEXT("halign"));
	const FName AttrVAlign(TEXT("valign"));

	FSlaterAttrDesc MakeAttr(const TCHAR* Name, ESlaterAttrType Type, std::initializer_list<const TCHAR*> EnumWords = {})
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		for (const TCHAR* Word : EnumWords)
		{
			Desc.EnumValues.Add(Word);
		}
		return Desc;
	}

	FSlaterAttrDesc WithBind(FSlaterAttrDesc Desc, FSlaterBindTarget Bind)
	{
		Desc.Bind = MoveTemp(Bind);
		return Desc;
	}

	/** Lowercase letters, digits and hyphens, starting with a letter. */
	bool IsMarkupWord(const FString& Name)
	{
		if (Name.IsEmpty() || !(Name[0] >= 'a' && Name[0] <= 'z'))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsSlotAttr(FName Name)
	{
		return Name == AttrFill || Name == AttrMargin || Name == AttrHAlign || Name == AttrVAlign;
	}

	ESlaterSlotAttrs SlotFlag(FName Name)
	{
		if (Name == AttrFill) return ESlaterSlotAttrs::Fill;
		if (Name == AttrMargin) return ESlaterSlotAttrs::Margin;
		if (Name == AttrHAlign) return ESlaterSlotAttrs::HAlign;
		return ESlaterSlotAttrs::VAlign;
	}

	FString JoinNames(const TArray<FString>& Names)
	{
		return FString::Join(Names, TEXT(", "));
	}

	/** ` (kind), for example name="value"` for a required attribute, so the fix is a copy away. */
	FString ExampleFor(const FSlaterAttrDesc& D)
	{
		FString Value;
		const TCHAR* Kind = TEXT("text");
		switch (D.Type)
		{
		case ESlaterAttrType::Metric: Value = TEXT("8"); Kind = TEXT("a number or a metric token such as UI.Space.M"); break;
		case ESlaterAttrType::Color: Value = TEXT("#336699"); Kind = TEXT("#RRGGBB or a color token"); break;
		case ESlaterAttrType::Font: Value = TEXT("UI.Font.Body"); Kind = TEXT("a font token"); break;
		case ESlaterAttrType::Surface: Value = TEXT("UI.Surface.Button"); Kind = TEXT("a surface token"); break;
		case ESlaterAttrType::Enum:
			Value = D.EnumValues.Num() > 0 ? D.EnumValues[0].ToString() : FString();
			Kind = TEXT("one of the allowed words");
			break;
		case ESlaterAttrType::Bool: Value = TEXT("true"); Kind = TEXT("true or false"); break;
		case ESlaterAttrType::Int: Value = TEXT("3"); Kind = TEXT("a whole number"); break;
		default: Value = TEXT("Name"); break;
		}
		return FString::Printf(TEXT(" (%s), for example %s=\"%s\""), Kind, *D.Name.ToString(), *Value);
	}

	FString Suggest(const FString& Name, const TArray<FString>& Candidates)
	{
		const FString Nearest = Slater::SuggestNearest(Name, Candidates);
		return Nearest.IsEmpty() ? FString() : FString::Printf(TEXT("; did you mean '%s'?"), *Nearest);
	}

	bool ParseHexColor(const FString& Text, FLinearColor& Out)
	{
		if (Text.Len() != 7 && Text.Len() != 9)
		{
			return false;
		}
		uint32 Digits[8] = {};
		for (int32 I = 1; I < Text.Len(); ++I)
		{
			const TCHAR C = Text[I];
			if (C >= '0' && C <= '9') Digits[I - 1] = C - '0';
			else if (C >= 'a' && C <= 'f') Digits[I - 1] = 10 + C - 'a';
			else if (C >= 'A' && C <= 'F') Digits[I - 1] = 10 + C - 'A';
			else return false;
		}
		const int32 Pairs = (Text.Len() - 1) / 2;
		uint8 Bytes[4] = {0, 0, 0, 255};
		for (int32 I = 0; I < Pairs; ++I)
		{
			Bytes[I] = static_cast<uint8>(Digits[I * 2] * 16 + Digits[I * 2 + 1]);
		}
		Out = FLinearColor::FromSRGBColor(FColor(Bytes[0], Bytes[1], Bytes[2], Bytes[3]));
		return true;
	}

	bool ParseInteger(const FString& Text, int32& Out)
	{
		if (Text.IsEmpty() || Text.Len() > 9)
		{
			return false;
		}
		int32 Start = (Text[0] == '-') ? 1 : 0;
		if (Start == Text.Len())
		{
			return false;
		}
		for (int32 I = Start; I < Text.Len(); ++I)
		{
			if (!FChar::IsDigit(Text[I]))
			{
				return false;
			}
		}
		Out = FCString::Atoi(*Text);
		return true;
	}

	/** The visibility a widget has when `visible` is true, kept on the widget so a bound `visible` can bring it back. */
	class FSlaterShownVisibility : public ISlateMetaData
	{
	public:
		SLATE_METADATA_TYPE(FSlaterShownVisibility, ISlateMetaData)
		explicit FSlaterShownVisibility(EVisibility InShown) : Shown(InShown) {}
		EVisibility Shown;
	};

	EVisibility ShownVisibility(const SWidget& Widget)
	{
		const TSharedPtr<FSlaterShownVisibility> Meta = Widget.GetMetaData<FSlaterShownVisibility>();
		return Meta.IsValid() ? Meta->Shown : EVisibility::Visible;
	}

	FSlaterBindTarget VisibleBind()
	{
		FSlaterBindTarget Target;
		Target.bOutermost = true;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			Widget.SetVisibility(Value.bBool ? ShownVisibility(Widget) : EVisibility::Collapsed);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			const EVisibility Shown = ShownVisibility(Widget);
			Widget.SetVisibility(TAttribute<EVisibility>::CreateLambda([Get = MoveTemp(Get), Shown]() { return Get().bBool ? Shown : EVisibility::Collapsed; }));
		};
		return Target;
	}

	FSlaterAttrDesc MakeVisibleWhenAttr()
	{
		FSlaterAttrDesc Desc = MakeAttr(TEXT("visible-when"), ESlaterAttrType::Text);
		Desc.CheckText = [](const FString& Text) { return Slater::CheckStatePair(Text, TEXT("visible-when")); };
		return Desc;
	}

	FSlaterAttrDesc MakeTransitionAttr()
	{
		FSlaterAttrDesc Desc = MakeAttr(TEXT("transition"), ESlaterAttrType::Text);
		Desc.CheckText = [](const FString& Text)
		{
			return Slater::IsValidTokenName(Text) ? FString() : FString::Printf(TEXT("'%s' is not a motion token name: write a dotted name such as UI.Motion.Panel"), *Text);
		};
		return Desc;
	}

	FSlaterAttrDesc MakeSizeByAttr(const TCHAR* Name)
	{
		FSlaterAttrDesc Desc = MakeAttr(Name, ESlaterAttrType::Text);
		Desc.CheckText = [Name](const FString& Text)
		{
			FString Key;
			TArray<Slater::FSizeByCase> Cases;
			return Slater::ParseSizeBy(Text, Name, Key, Cases);
		};
		return Desc;
	}

	FSlaterBindTarget EnabledBind()
	{
		FSlaterBindTarget Target;
		Target.bOutermost = true;
		Target.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			Widget.SetEnabled(Value.bBool);
		};
		Target.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			Widget.SetEnabled(TAttribute<bool>::CreateLambda([Get = MoveTemp(Get)]() { return Get().bBool; }));
		};
		return Target;
	}

	class FLayoutBuilder
	{
	public:
		FLayoutBuilder(const FString& InPath, const ISlaterStyleSource& InStyle, const FSlaterElementRegistry& InRegistry, FSlaterBuildResult& InOut)
			: Path(InPath), Style(InStyle), Registry(InRegistry), Out(InOut)
		{
		}

		/** Checks Node, builds it, and returns what it resolved in OutResolved (kept whole, children included, only inside a template). */
		TSharedPtr<SWidget> Build(const FSlaterNode& Node, const FSlaterElementDesc* ParentDesc, FSlaterSlotValues& OutSlot, TSharedPtr<FResolvedNode>* OutResolved = nullptr);

		/** Builds an instance of a resolved node (and its children) without checking anything again. */
		TSharedPtr<SWidget> Instantiate(const FResolvedNode& Resolved, const FSlaterNode& Node);

		/** 1 while checking a template (a `repeat`'s child). */
		int32 TemplateDepth = 0;

		bool ParseBound(const FSlaterNode& Node, const FSlaterAttribute& Attr, const FSlaterAttrDesc& Desc, FSlaterValue& OutValue)
		{
			return ParseValue(Node, Attr, Desc, OutValue);
		}

	private:
		const FString& Path;
		const ISlaterStyleSource& Style;
		const FSlaterElementRegistry& Registry;
		FSlaterBuildResult& Out;
		TMap<FName, FSlaterPos> Ids;

		void Error(const FSlaterPos& Pos, const FString& Message)
		{
			FSlaterError& Error = Out.Errors.AddDefaulted_GetRef();
			// An element of an imported template is reported under its own file.
			Error.Path = Pos.File.IsNone() ? Path : Pos.File.ToString();
			Error.Line = Pos.Line;
			Error.Column = Pos.Column;
			Error.Message = Message;
		}

		bool ParseValue(const FSlaterNode& Node, const FSlaterAttribute& Attr, const FSlaterAttrDesc& Desc, FSlaterValue& OutValue);
		bool ParseMetric(const FSlaterAttribute& Attr, FSlaterValue& OutValue);
		bool ParseColor(const FSlaterAttribute& Attr, FSlaterValue& OutValue);
		bool ParseToken(const FSlaterAttribute& Attr, ESlaterTokenKind Kind, FSlaterValue& OutValue);
		void CheckRanges(const FSlaterNode& Node, const FSlaterBuildContext& Context);
		/** The part both paths share: run the builder, record the bindings, add the sizing wrapper and the id. */
		TSharedPtr<SWidget> Finish(const FResolvedNode& Resolved, const FSlaterNode& Node, FSlaterBuildContext& Context);
	};

	bool FLayoutBuilder::ParseMetric(const FSlaterAttribute& Attr, FSlaterValue& OutValue)
	{
		if (FCString::IsNumeric(*Attr.Value))
		{
			OutValue.Number = FCString::Atof(*Attr.Value);
			return true;
		}
		return ParseToken(Attr, ESlaterTokenKind::Metric, OutValue);
	}

	bool FLayoutBuilder::ParseColor(const FSlaterAttribute& Attr, FSlaterValue& OutValue)
	{
		if (Attr.Value.StartsWith(TEXT("#")))
		{
			if (ParseHexColor(Attr.Value, OutValue.Color))
			{
				return true;
			}
			Error(Attr.Pos, FString::Printf(TEXT("'%s' is not a color: a literal is #RRGGBB or #RRGGBBAA"), *Attr.Value));
			return false;
		}
		return ParseToken(Attr, ESlaterTokenKind::Color, OutValue);
	}

	bool FLayoutBuilder::ParseToken(const FSlaterAttribute& Attr, ESlaterTokenKind Kind, FSlaterValue& OutValue)
	{
		if (!Slater::IsValidTokenName(Attr.Value))
		{
			Error(Attr.Pos, FString::Printf(TEXT("'%s' is not a %s: write %s"), *Attr.Value, LexToString(Kind),
				Kind == ESlaterTokenKind::Metric ? TEXT("a number or a metric token name") : Kind == ESlaterTokenKind::Color ? TEXT("#RRGGBB or a color token name") : TEXT("a token name")));
			return false;
		}
		const FName Token(*Attr.Value);
		bool bFound = false;
		switch (Kind)
		{
		case ESlaterTokenKind::Color: bFound = Style.FindColor(Token, OutValue.Color); break;
		case ESlaterTokenKind::Metric: bFound = Style.FindMetric(Token, OutValue.Number); break;
		case ESlaterTokenKind::Font: bFound = Style.FindFont(Token, OutValue.Font); break;
		case ESlaterTokenKind::Surface: bFound = Style.FindSurface(Token, OutValue.Surface); break;
		case ESlaterTokenKind::Motion: break; // `transition` looks its motion up itself (it has no value type)
		}
		if (!bFound)
		{
			Error(Attr.Pos, FString::Printf(TEXT("unknown %s token '%s'%s"), LexToString(Kind), *Attr.Value, *Slater::DidYouMean(Style, Kind, Attr.Value)));
			return false;
		}
		OutValue.Word = Token;
		return true;
	}

	bool FLayoutBuilder::ParseValue(const FSlaterNode& Node, const FSlaterAttribute& Attr, const FSlaterAttrDesc& Desc, FSlaterValue& OutValue)
	{
		OutValue.Type = Desc.Type;
		switch (Desc.Type)
		{
		case ESlaterAttrType::Metric:
			return ParseMetric(Attr, OutValue);
		case ESlaterAttrType::Color:
			return ParseColor(Attr, OutValue);
		case ESlaterAttrType::Font:
			return ParseToken(Attr, ESlaterTokenKind::Font, OutValue);
		case ESlaterAttrType::Surface:
			return ParseToken(Attr, ESlaterTokenKind::Surface, OutValue);
		case ESlaterAttrType::Enum:
		{
			for (const FName Word : Desc.EnumValues)
			{
				if (Attr.Value.Equals(Word.ToString(), ESearchCase::CaseSensitive))
				{
					OutValue.Word = Word;
					return true;
				}
			}
			TArray<FString> Words;
			for (const FName Word : Desc.EnumValues)
			{
				Words.Add(Word.ToString());
			}
			Error(Attr.Pos, FString::Printf(TEXT("'%s' is not a valid '%s' on <%s>%s; allowed: %s"), *Attr.Value, *Desc.Name.ToString(), *Node.Tag, *Suggest(Attr.Value, Words), *JoinNames(Words)));
			return false;
		}
		case ESlaterAttrType::Bool:
			if (Attr.Value == TEXT("true") || Attr.Value == TEXT("false"))
			{
				OutValue.bBool = Attr.Value == TEXT("true");
				return true;
			}
			Error(Attr.Pos, FString::Printf(TEXT("'%s' is not a bool: write true or false"), *Attr.Value));
			return false;
		case ESlaterAttrType::Int:
			if (ParseInteger(Attr.Value, OutValue.Integer))
			{
				return true;
			}
			Error(Attr.Pos, FString::Printf(TEXT("'%s' is not an integer"), *Attr.Value));
			return false;
		case ESlaterAttrType::Text:
			if (Desc.CheckText)
			{
				const FString Problem = Desc.CheckText(Attr.Value);
				if (!Problem.IsEmpty())
				{
					Error(Attr.Pos, Problem);
					return false;
				}
			}
			OutValue.Text = Attr.Value;
			return true;
		}
		return false;
	}

	void FLayoutBuilder::CheckRanges(const FSlaterNode& Node, const FSlaterBuildContext& Context)
	{
		static const FName NonNegative[] = {AttrWidth, AttrHeight, AttrMinWidth, AttrMinHeight, AttrMaxWidth, AttrMaxHeight};
		for (const FName Name : NonNegative)
		{
			const FSlaterValue* Value = Context.Values.Find(Name);
			if (Value && Value->Number < 0.f)
			{
				Error(Node.FindAttribute(Name.ToString())->Pos, FString::Printf(TEXT("'%s' must not be negative"), *Name.ToString()));
			}
		}
	}

	TSharedPtr<SWidget> FLayoutBuilder::Build(const FSlaterNode& Node, const FSlaterElementDesc* ParentDesc, FSlaterSlotValues& OutSlot, TSharedPtr<FResolvedNode>* OutResolved)
	{
		const FSlaterElementDesc* Desc = Registry.Find(FName(*Node.Tag, FNAME_Find));
		if (!Desc)
		{
			TArray<FString> Tags;
			Registry.GetTags(Tags);
			FString Purposes;
			for (const FString& Tag : Tags)
			{
				const FSlaterElementDesc* Known = Registry.Find(FName(*Tag, FNAME_Find));
				if (Known && Known->Purpose && *Known->Purpose)
				{
					Purposes += FString::Printf(TEXT("%s%s = %s"), Purposes.IsEmpty() ? TEXT("") : TEXT("; "), *Tag, Known->Purpose);
				}
			}
			const FString PurposeNote = Purposes.IsEmpty() ? FString() : FString::Printf(TEXT(". What they are for: %s"), *Purposes);
			Error(Node.Pos, FString::Printf(TEXT("unknown element <%s>%s; known: %s%s"), *Node.Tag, *Suggest(Node.Tag, Tags), *JoinNames(Tags), *PurposeNote));
			return nullptr;
		}

		const int32 ErrorsBefore = Out.Errors.Num();
		const bool bInTemplate = TemplateDepth > 0;
		const TSharedRef<FResolvedNode> Resolved = MakeShared<FResolvedNode>();
		Resolved->Desc = Desc;
		if (bInTemplate)
		{
			Resolved->OwnedDesc = MakeShared<const FSlaterElementDesc>(*Desc);
			Resolved->Desc = Resolved->OwnedDesc.Get();
		}
		FSlaterBuildContext Context(Node, Resolved->Values);
		Context.Style = &Style;
		Context.Registry = &Registry;
		FName& IdName = Resolved->IdName;
		TSet<FName> BoundNames;
		FString BoundPathForText;
		FSlaterMotion TransitionMotion;
		bool bHasTransition = false;

		if ((Desc->bTemplateChildren || Desc->TemplateSlots.Num() > 0) && TemplateDepth > 0)
		{
			Error(Node.Pos, FString::Printf(TEXT("<%s> inside the template of another element that repeats it is not supported yet"), *Node.Tag));
			return nullptr;
		}

		for (const FSlaterAttribute& Attr : Node.Attributes)
		{
			const FName AttrName(*Attr.Name, FNAME_Find);
			const FSlaterAttrDesc* AttrDesc = Desc->Attributes.FindByPredicate([&](const FSlaterAttrDesc& D) { return D.Name == AttrName; });
			if (!AttrDesc)
			{
				AttrDesc = Slater::CommonAttributes().FindByPredicate([&](const FSlaterAttrDesc& D) { return D.Name == AttrName; });
			}
			if (!AttrDesc && !AttrName.IsNone() && IsSlotAttr(AttrName))
			{
				if (!ParentDesc)
				{
					Error(Attr.Pos, FString::Printf(TEXT("'%s' is a slot attribute, and the root element <%s> has no parent slot"), *Attr.Name, *Node.Tag));
					continue;
				}
				if (!EnumHasAnyFlags(ParentDesc->SlotAttrs, SlotFlag(AttrName)))
				{
					Error(Attr.Pos, FString::Printf(TEXT("'%s' has no effect here: the slots of <%s> do not use it"), *Attr.Name, *ParentDesc->Tag.ToString()));
					continue;
				}
				AttrDesc = Slater::SlotAttributes().FindByPredicate([&](const FSlaterAttrDesc& D) { return D.Name == AttrName; });
			}
			if (!AttrDesc)
			{
				TArray<FString> Allowed;
				for (const FSlaterAttrDesc& D : Desc->Attributes) Allowed.Add(D.Name.ToString());
				for (const FSlaterAttrDesc& D : Slater::CommonAttributes()) Allowed.Add(D.Name.ToString());
				if (ParentDesc)
				{
					for (const FSlaterAttrDesc& D : Slater::SlotAttributes())
					{
						if (EnumHasAnyFlags(ParentDesc->SlotAttrs, SlotFlag(D.Name))) Allowed.Add(D.Name.ToString());
					}
				}
				Error(Attr.Pos, FString::Printf(TEXT("unknown attribute '%s' on <%s>%s; allowed: %s"), *Attr.Name, *Node.Tag, *Suggest(Attr.Name, Allowed), *JoinNames(Allowed)));
				continue;
			}

			if (AttrDesc->bEvent)
			{
				// `{Path}` writes the control's value back; anything else must be a command (the attribute's check says so).
				const FString Problem = AttrDesc->CheckText ? AttrDesc->CheckText(Attr.Value) : FString();
				if (!Problem.IsEmpty())
				{
					Error(Attr.Pos, Problem);
					continue;
				}
				const TSharedRef<FSlaterEventSpec> Event = MakeShared<FSlaterEventSpec>();
				Event->Event = AttrName;
				Event->Pos = Attr.Pos;
				if (!Slater::IsBindingExpr(Attr.Value, &Event->Path))
				{
					FString CommandName;
					Slater::SplitCommandText(Attr.Value, CommandName, Event->Argument);
					Event->Command = FName(*CommandName);
				}
				Resolved->Events.Add(Event);
				continue;
			}

			FString BoundPath;
			if (Slater::IsBindingExpr(Attr.Value, &BoundPath))
			{
				if (AttrDesc->Bind.IsSet())
				{
					const TSharedRef<FSlaterBindingSpec> Bound = MakeShared<FSlaterBindingSpec>();
					Bound->Attr = AttrName;
					Bound->Path = BoundPath;
					Bound->Desc = *AttrDesc;
					Bound->Pos = Attr.Pos;
					Resolved->Bindings.Add(Bound);
					BoundNames.Add(AttrName);
					continue;
				}
				if (AttrDesc->Type != ESlaterAttrType::Text)
				{
					Error(Attr.Pos, FString::Printf(TEXT("'%s' on <%s> cannot be bound to data (it is read once, when the layout is built)"), *Attr.Name, *Node.Tag));
					continue;
				}
			}

			FSlaterValue Value;
			if (!ParseValue(Node, Attr, *AttrDesc, Value))
			{
				continue;
			}
			if (AttrName == AttrId && TemplateDepth > 0)
			{
				Error(Attr.Pos, TEXT("'id' is not allowed inside a template: every item would repeat it"));
				continue;
			}
			if (AttrName == AttrVisibleWhen && TemplateDepth > 0)
			{
				Error(Attr.Pos, TEXT("'visible-when' is not supported inside a template yet: each item would need its own wiring to the view's state; put it on an element outside the repeat"));
				continue;
			}
			if ((AttrName == Slater::Attr::OpenWhen || AttrName == Slater::Attr::CloseState) && TemplateDepth > 0)
			{
				Error(Attr.Pos, FString::Printf(TEXT("'%s' is not supported inside a template yet: each item would need its own wiring to the view's state; put the popup outside the repeat, or open it by a click on its anchor"), *Attr.Name));
				continue;
			}
			if (AttrName == AttrWidthBy || AttrName == AttrHeightBy)
			{
				if (TemplateDepth > 0)
				{
					Error(Attr.Pos, FString::Printf(TEXT("'%s' is not supported inside a template yet: each item would need its own wiring to the view's state; put it on an element outside the repeat"), *Attr.Name));
					continue;
				}
				FString SizeKey;
				TArray<Slater::FSizeByCase> Cases;
				Slater::ParseSizeBy(Value.Text, *Attr.Name, SizeKey, Cases);
				FSlaterStateSize Size;
				Size.Key = FName(*SizeKey);
				Size.bHeight = AttrName == AttrHeightBy;
				bool bCasesOk = true;
				for (const Slater::FSizeByCase& Case : Cases)
				{
					FSlaterAttribute MetricAttr = Attr;
					MetricAttr.Value = Case.Metric;
					FSlaterValue Metric;
					Metric.Type = ESlaterAttrType::Metric;
					if (!ParseMetric(MetricAttr, Metric))
					{
						bCasesOk = false;
					}
					else if (Metric.Number < 0.f)
					{
						Error(Attr.Pos, FString::Printf(TEXT("'%s' must not be negative (value '%s')"), *Attr.Name, *Case.Value));
						bCasesOk = false;
					}
					else
					{
						Size.Values.Add(Case.Value);
						Size.Sizes.Add(Metric.Number);
					}
				}
				if (bCasesOk)
				{
					Resolved->SizeBy.Add(MoveTemp(Size));
				}
				continue;
			}
			if (AttrName == AttrTransition)
			{
				if (TemplateDepth > 0)
				{
					Error(Attr.Pos, TEXT("'transition' is not supported inside a template yet: it animates a state-driven size, which needs the view's state; put it on an element outside the repeat"));
					continue;
				}
				if (!Style.FindMotion(FName(*Attr.Value), TransitionMotion))
				{
					Error(Attr.Pos, FString::Printf(TEXT("unknown motion token '%s'%s"), *Attr.Value, *Slater::DidYouMean(Style, ESlaterTokenKind::Motion, Attr.Value)));
					continue;
				}
				bHasTransition = true;
				continue;
			}
			if (AttrName == AttrFill && Value.Number <= 0.f)
			{
				Error(Attr.Pos, TEXT("'fill' is a weight and must be above 0; remove it, or use a weight such as 1"));
				continue;
			}
			if (AttrName == AttrId)
			{
				IdName = FName(*Attr.Value);
				if (const FSlaterPos* First = Ids.Find(IdName))
				{
					Error(Attr.Pos, FString::Printf(TEXT("duplicate id '%s' (first used at %d,%d)"), *Attr.Value, First->Line, First->Column));
					IdName = NAME_None;
					continue;
				}
				Ids.Add(IdName, Attr.Pos);
			}
			if (IsSlotAttr(AttrName))
			{
				if (AttrName == AttrFill) OutSlot.Fill = Value.Number;
				else if (AttrName == AttrMargin) OutSlot.Margin = Value.Number;
				else if (AttrName == AttrHAlign) OutSlot.HAlign = Value.Word == TEXT("left") ? HAlign_Left : Value.Word == TEXT("center") ? HAlign_Center : Value.Word == TEXT("right") ? HAlign_Right : HAlign_Fill;
				else OutSlot.VAlign = Value.Word == TEXT("top") ? VAlign_Top : Value.Word == TEXT("center") ? VAlign_Center : Value.Word == TEXT("bottom") ? VAlign_Bottom : VAlign_Fill;
			}
			else
			{
				Resolved->Values.Add(AttrName, MoveTemp(Value));
			}
		}

		if (Resolved->Values.Contains(AttrVisibleWhen) && (Resolved->Values.Contains(AttrVisible) || BoundNames.Contains(AttrVisible)))
		{
			Error(Node.FindAttribute(AttrVisibleWhen.ToString())->Pos, TEXT("'visible-when' and 'visible' on one element would fight over its visibility; use one of them"));
		}

		if (bHasTransition)
		{
			if (!Node.FindAttribute(AttrWidthBy.ToString()) && !Node.FindAttribute(AttrHeightBy.ToString()))
			{
				Error(Node.FindAttribute(AttrTransition.ToString())->Pos, TEXT("'transition' has nothing to animate here: it moves a state-driven size, so add width-by or height-by to the same element"));
			}
			for (FSlaterStateSize& Size : Resolved->SizeBy)
			{
				Size.bAnimate = true;
				Size.Motion = TransitionMotion;
			}
		}

		for (const FSlaterStateSize& Size : Resolved->SizeBy)
		{
			const FName Fixed = Size.bHeight ? AttrHeight : AttrWidth;
			if (Resolved->Values.Contains(Fixed))
			{
				const FName By = Size.bHeight ? AttrHeightBy : AttrWidthBy;
				Error(Node.FindAttribute(By.ToString())->Pos, FString::Printf(TEXT("'%s' and '%s' on one element would fight over its size; use one of them"), *By.ToString(), *Fixed.ToString()));
			}
		}

		for (const FSlaterAttrDesc& D : Desc->Attributes)
		{
			if (D.bRequired && !Resolved->Values.Contains(D.Name) && !BoundNames.Contains(D.Name))
			{
				Error(Node.Pos, FString::Printf(TEXT("<%s> needs the attribute '%s'%s"), *Node.Tag, *D.Name.ToString(), *ExampleFor(D)));
			}
		}
		CheckRanges(Node, Context);
		if (Desc->ResolveDefaults && Out.Errors.Num() == ErrorsBefore)
		{
			Desc->ResolveDefaults(Style, Resolved->Values);
		}

		if (!Desc->bAllowsText && !Node.Text.IsEmpty())
		{
			Error(Node.TextPos, FString::Printf(TEXT("<%s> cannot contain text"), *Node.Tag));
		}
		else if (Desc->CheckContent && !Node.Text.IsEmpty() && !Slater::IsBindingExpr(Node.Text))
		{
			TArray<FSlaterTextProblem> Problems;
			Desc->CheckContent(Node.Text, Style, Problems);
			for (const FSlaterTextProblem& Problem : Problems)
			{
				FSlaterPos Pos = Node.TextPos;
				Pos.Column = Problem.Line == 1 ? Node.TextPos.Column + Problem.Column - 1 : Problem.Column;
				Pos.Line = Node.TextPos.Line + Problem.Line - 1;
				Error(Pos, Problem.Message);
			}
		}
		else if (Desc->TextBind.IsSet() && Slater::IsBindingExpr(Node.Text, &BoundPathForText))
		{
			const TSharedRef<FSlaterBindingSpec> Bound = MakeShared<FSlaterBindingSpec>();
			Bound->Attr = TEXT("text");
			Bound->Path = BoundPathForText;
			Bound->Desc.Name = TEXT("text");
			Bound->Desc.Type = ESlaterAttrType::Text;
			Bound->Desc.Bind = Desc->TextBind;
			Bound->Pos = Node.TextPos;
			Resolved->Bindings.Add(Bound);
		}

		bool bChildFailed = false;
		const int32 BindingsBefore = Out.Bindings.Num();
		const int32 EventsBefore = Out.Events.Num();
		TSharedPtr<FResolvedNode> TemplateChild;
		if (Desc->bTemplateChildren)
		{
			++TemplateDepth;
		}
		if (Desc->TemplateSlots.Num() > 0)
		{
			TSet<FName> Seen;
			TArray<FString> SlotNames;
			for (const FSlaterTemplateSlotDesc& Slot : Desc->TemplateSlots)
			{
				SlotNames.Add(Slot.Name.ToString());
			}
			for (const FSlaterNode& SlotNode : Node.Children)
			{
				const FSlaterTemplateSlotDesc* Slot = Desc->TemplateSlots.FindByPredicate([&](const FSlaterTemplateSlotDesc& S) { return S.Name.ToString() == SlotNode.Tag; });
				if (!Slot)
				{
					Error(SlotNode.Pos, FString::Printf(TEXT("<%s> has no slot <%s>%s; its slots are: %s"), *Node.Tag, *SlotNode.Tag, *Suggest(SlotNode.Tag, SlotNames), *JoinNames(SlotNames)));
					bChildFailed = true;
					continue;
				}
				if (Seen.Contains(Slot->Name))
				{
					Error(SlotNode.Pos, FString::Printf(TEXT("<%s> has <%s> twice"), *Node.Tag, *SlotNode.Tag));
					bChildFailed = true;
					continue;
				}
				Seen.Add(Slot->Name);
				if (SlotNode.Attributes.Num() > 0)
				{
					Error(SlotNode.Attributes[0].Pos, FString::Printf(TEXT("<%s> takes no attributes"), *SlotNode.Tag));
					bChildFailed = true;
				}
				if (!SlotNode.Text.IsEmpty())
				{
					Error(SlotNode.TextPos, FString::Printf(TEXT("<%s> holds one child element, not text"), *SlotNode.Tag));
					bChildFailed = true;
				}
				if (SlotNode.Children.Num() != 1)
				{
					Error(SlotNode.Pos, FString::Printf(TEXT("<%s> needs exactly one child element (the template of one item); this has %d"), *SlotNode.Tag, SlotNode.Children.Num()));
					bChildFailed = true;
					continue;
				}
				FSlaterSlotValues SlotValues;
				TSharedPtr<FResolvedNode> SlotResolved;
				const int32 SlotBindingsBefore = Out.Bindings.Num();
				const int32 SlotEventsBefore = Out.Events.Num();
				++TemplateDepth;
				const TSharedPtr<SWidget> Checked = Build(SlotNode.Children[0], nullptr, SlotValues, &SlotResolved);
				--TemplateDepth;
				// Built only to check it; the host makes the instances and their bindings.
				Out.Bindings.SetNum(SlotBindingsBefore, EAllowShrinking::No);
				Out.Events.SetNum(SlotEventsBefore, EAllowShrinking::No);
				if (!Checked.IsValid())
				{
					bChildFailed = true;
					continue;
				}
				const TSharedRef<FSlaterCompiledTemplate> Compiled = MakeShared<FSlaterCompiledTemplate>();
				Compiled->Root = SlotNode.Children[0];
				Compiled->RootNode = SlotResolved;
				Compiled->Path = Path;
				Context.SlotTemplates.Add(Slot->Name, Compiled);
			}
			for (const FSlaterTemplateSlotDesc& Slot : Desc->TemplateSlots)
			{
				if (Slot.bRequired && !Seen.Contains(Slot.Name))
				{
					Error(Node.Pos, FString::Printf(TEXT("<%s> needs a <%s> child holding the template of one item, for example <%s><surface role=\"UI.Surface.Panel\"/></%s>"), *Node.Tag, *Slot.Name.ToString(), *Slot.Name.ToString(), *Slot.Name.ToString()));
					bChildFailed = true;
				}
			}
		}
		else if (Desc->Children == ESlaterChildRule::None && Node.Children.Num() > 0)
		{
			Error(Node.Children[0].Pos, FString::Printf(TEXT("<%s> cannot have child elements"), *Node.Tag));
		}
		else
		{
			for (int32 I = 0; I < Node.Children.Num(); ++I)
			{
				const FSlaterNode& ChildNode = Node.Children[I];
				if (Desc->Children == ESlaterChildRule::One && I > 0)
				{
					Error(ChildNode.Pos, FString::Printf(TEXT("<%s> takes one child; this is number %d"), *Node.Tag, I + 1));
					break;
				}
				FSlaterSlotValues Slot;
				TSharedPtr<FResolvedNode> ChildResolved;
				TSharedPtr<SWidget> ChildWidget = Build(ChildNode, Desc, Slot, &ChildResolved);
				if (ChildWidget.IsValid())
				{
					ChildResolved->Slot = Slot;
					Context.Children.Add({ChildWidget.ToSharedRef(), Slot, &ChildNode});
					if (bInTemplate)
					{
						Resolved->Children.Add(ChildResolved.ToSharedRef());
					}
					if (Desc->bTemplateChildren)
					{
						TemplateChild = ChildResolved;
					}
				}
				else
				{
					bChildFailed = true;
				}
			}
		}

		if (Desc->bTemplateChildren)
		{
			--TemplateDepth;
			// The template was only built to check it; its bindings belong to the instances the element makes later.
			Out.Bindings.SetNum(BindingsBefore, EAllowShrinking::No);
			Out.Events.SetNum(EventsBefore, EAllowShrinking::No);
		}

		if (Out.Errors.Num() != ErrorsBefore || bChildFailed)
		{
			return nullptr;
		}

		for (const FName Name : {AttrWidth, AttrHeight, AttrMinWidth, AttrMinHeight, AttrMaxWidth, AttrMaxHeight, AttrPadding})
		{
			Resolved->bNeedsBox |= Resolved->Values.Contains(Name);
		}
		if (TemplateChild.IsValid())
		{
			const TSharedRef<FSlaterCompiledTemplate> Compiled = MakeShared<FSlaterCompiledTemplate>();
			Compiled->Root = Node.Children[0];
			Compiled->RootNode = TemplateChild;
			Compiled->Path = Path;
			Context.Template = Compiled;
		}
		if (OutResolved)
		{
			*OutResolved = Resolved;
		}
		return Finish(*Resolved, Node, Context);
	}

	TSharedPtr<SWidget> FLayoutBuilder::Instantiate(const FResolvedNode& Resolved, const FSlaterNode& Node)
	{
		FSlaterBuildContext Context(Node, Resolved.Values);
		Context.Style = &Style;
		Context.Registry = &Registry;
		for (int32 I = 0; I < Resolved.Children.Num(); ++I)
		{
			TSharedPtr<SWidget> ChildWidget = Instantiate(*Resolved.Children[I], Node.Children[I]);
			if (!ChildWidget.IsValid())
			{
				return nullptr;
			}
			Context.Children.Add({ChildWidget.ToSharedRef(), Resolved.Children[I]->Slot, &Node.Children[I]});
		}
		return Finish(Resolved, Node, Context);
	}

	TSharedPtr<SWidget> FLayoutBuilder::Finish(const FResolvedNode& Resolved, const FSlaterNode& Node, FSlaterBuildContext& Context)
	{
		const FSlaterElementDesc& Desc = *Resolved.Desc;
		const TSharedPtr<SWidget> Inner = Desc.Builder(Context);
		if (!Inner.IsValid())
		{
			Error(Node.Pos, Context.Failure.IsEmpty() ? FString::Printf(TEXT("<%s> could not be built"), *Node.Tag) : Context.Failure);
			return nullptr;
		}
		if (Desc.bTemplateChildren)
		{
			Out.Repeats.Add(Inner);
		}
		if (Resolved.Events.Num() > 0)
		{
			// The builder made the control's sink; each instance's sink learns what its event attributes say.
			const TSharedPtr<FSlaterEventSink> Sink = Slater::FindEventSink(*Inner);
			if (!Sink.IsValid())
			{
				Error(Node.Pos, FString::Printf(TEXT("<%s> declares events but its builder attached no event sink (Slater::AttachEventSink)"), *Node.Tag));
				return nullptr;
			}
			for (const TSharedRef<const FSlaterEventSpec>& Event : Resolved.Events)
			{
				Sink->AddSpec(Event);
				if (Event->IsWrite())
				{
					Out.Events.Add({Inner, Event});
				}
			}
		}

		TSharedPtr<SWidget> Widget = Inner;
		if (!Desc.bAppliesSizing && Resolved.bNeedsBox)
		{
			TSharedRef<SBox> Box = SNew(SBox)[Widget.ToSharedRef()];
			Slater::ApplySizing(Context, *Box);
			Widget = Box;
		}

		// `width-by` / `height-by` size one box around the element (and its padding box); the view resizes it when the state changes.
		if (Resolved.SizeBy.Num() > 0)
		{
			const TSharedRef<SBox> StateBox = SNew(SBox)[Widget.ToSharedRef()];
			for (const FSlaterStateSize& Size : Resolved.SizeBy)
			{
				FSlaterStateSize& Entry = Out.StateSizes.Add_GetRef(Size);
				Entry.Box = StateBox;
				if (Entry.bAnimate)
				{
					Entry.Anim = MakeShared<FSlaterAnimator>();
				}
			}
			Widget = StateBox;
		}
		if (const FSlaterValue* Clip = Resolved.Values.Find(AttrClip))
		{
			if (Clip->bBool)
			{
				Widget->SetClipping(EWidgetClipping::ClipToBounds);
			}
		}

		// `visible` and `enabled` belong to the outermost widget, so a collapsed element takes its sizing box with it.
		const FSlaterValue* LiteralVisible = Resolved.Values.Find(AttrVisible);
		const FSlaterValue* LiteralEnabled = Resolved.Values.Find(AttrEnabled);
		const bool bVisibleBound = Resolved.Bindings.ContainsByPredicate([](const TSharedRef<const FSlaterBindingSpec>& Spec) { return Spec->Attr == AttrVisible; });
		const FSlaterValue* VisibleWhen = Resolved.Values.Find(AttrVisibleWhen);
		if (LiteralVisible || bVisibleBound || VisibleWhen)
		{
			Widget->AddMetadata(MakeShared<FSlaterShownVisibility>(Widget->GetVisibility()));
		}
		if (LiteralVisible && !LiteralVisible->bBool)
		{
			Widget->SetVisibility(EVisibility::Collapsed);
		}
		if (LiteralEnabled && !LiteralEnabled->bBool)
		{
			Widget->SetEnabled(false);
		}
		FString WhenKey, WhenValue;
		if (VisibleWhen && Slater::ParseStatePair(VisibleWhen->Text, WhenKey, WhenValue))
		{
			Out.StateConditions.Add({Widget, FName(*WhenKey), WhenValue});
		}
		FString OpenKey, OpenValue;
		if (const FSlaterValue* OpenWhen = Resolved.Values.Find(Slater::Attr::OpenWhen))
		{
			if (Slater::ParseStatePair(OpenWhen->Text, OpenKey, OpenValue))
			{
				// The element's own widget (a popup), not its sizing wrapper: the view opens and closes it.
				FSlaterStateCondition& Condition = Out.StateConditions.Add_GetRef({Inner, FName(*OpenKey), OpenValue});
				Condition.bPopup = true;
			}
		}
		for (const TSharedRef<const FSlaterBindingSpec>& Spec : Resolved.Bindings)
		{
			Out.Bindings.Add({Spec->Desc.Bind.bOutermost ? Widget : Inner, Spec});
		}
		if (!Resolved.IdName.IsNone())
		{
			Out.ById.Add(Resolved.IdName, Widget);
		}
		return Widget;
	}

	void CollectOutlets(SWidget& Widget, TMap<FName, TWeakPtr<SSlaterOutlet>>& OutOutlets)
	{
		if (Widget.GetType() == FName(TEXT("SSlaterOutlet")))
		{
			SSlaterOutlet& Outlet = static_cast<SSlaterOutlet&>(Widget);
			OutOutlets.Add(Outlet.GetOutletId(), StaticCastSharedRef<SSlaterOutlet>(Outlet.AsShared()));
		}
		Widget.GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			CollectOutlets(Child, OutOutlets);
		});
	}

	void Measure(SWidget& Widget, int32 Level, int32& Count, int32& Depth)
	{
		++Count;
		Depth = FMath::Max(Depth, Level);
		Widget.GetChildren()->ForEachWidget([&](SWidget& Child)
		{
			if (&Child != &SNullWidget::NullWidget.Get())
			{
				Measure(Child, Level + 1, Count, Depth);
			}
		});
	}
}

namespace Slater
{
	const TArray<FSlaterAttrDesc>& CommonAttributes()
	{
		static const TArray<FSlaterAttrDesc> Attributes = {
			MakeAttr(TEXT("id"), ESlaterAttrType::Text),
			MakeAttr(TEXT("width"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("height"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("min-width"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("min-height"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("max-width"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("max-height"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("padding"), ESlaterAttrType::Metric),
			WithBind(MakeAttr(TEXT("visible"), ESlaterAttrType::Bool), VisibleBind()),
			WithBind(MakeAttr(TEXT("enabled"), ESlaterAttrType::Bool), EnabledBind()),
			MakeVisibleWhenAttr(),
			MakeSizeByAttr(TEXT("width-by")),
			MakeSizeByAttr(TEXT("height-by")),
			MakeTransitionAttr(),
			MakeAttr(TEXT("clip"), ESlaterAttrType::Bool),
		};
		return Attributes;
	}

	const TArray<FSlaterAttrDesc>& SlotAttributes()
	{
		static const TArray<FSlaterAttrDesc> Attributes = {
			MakeAttr(TEXT("fill"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("margin"), ESlaterAttrType::Metric),
			MakeAttr(TEXT("halign"), ESlaterAttrType::Enum, {TEXT("fill"), TEXT("left"), TEXT("center"), TEXT("right")}),
			MakeAttr(TEXT("valign"), ESlaterAttrType::Enum, {TEXT("fill"), TEXT("top"), TEXT("center"), TEXT("bottom")}),
		};
		return Attributes;
	}

	void SetShown(SWidget& Widget, bool bShown)
	{
		Widget.SetVisibility(bShown ? ShownVisibility(Widget) : EVisibility::Collapsed);
	}

	FSlaterElementRegistry& Elements()
	{
		static FSlaterElementRegistry Registry;
		static bool bFilled = false;
		if (!bFilled)
		{
			bFilled = true;
			RegisterLayoutElements(Registry);
			RegisterContentElements(Registry);
			RegisterListElements(Registry);
			RegisterControlElements(Registry);
			RegisterPopupElements(Registry);
			RegisterRichTextElements(Registry);
			RegisterComboElements(Registry);
		}
		return Registry;
	}

	void ApplySizing(const FSlaterBuildContext& Context, SBox& Box)
	{
		if (Context.Has(AttrWidth)) Box.SetWidthOverride(FOptionalSize(Context.GetMetric(AttrWidth)));
		if (Context.Has(AttrHeight)) Box.SetHeightOverride(FOptionalSize(Context.GetMetric(AttrHeight)));
		if (Context.Has(AttrMinWidth)) Box.SetMinDesiredWidth(FOptionalSize(Context.GetMetric(AttrMinWidth)));
		if (Context.Has(AttrMinHeight)) Box.SetMinDesiredHeight(FOptionalSize(Context.GetMetric(AttrMinHeight)));
		if (Context.Has(AttrMaxWidth)) Box.SetMaxDesiredWidth(FOptionalSize(Context.GetMetric(AttrMaxWidth)));
		if (Context.Has(AttrMaxHeight)) Box.SetMaxDesiredHeight(FOptionalSize(Context.GetMetric(AttrMaxHeight)));
		if (Context.Has(AttrPadding)) Box.SetPadding(FMargin(Context.GetMetric(AttrPadding)));
	}

	bool IsBindingExpr(const FString& Text, FString* OutPath)
	{
		int32 Begin = 0;
		int32 End = Text.Len();
		while (Begin < End && FChar::IsWhitespace(Text[Begin])) ++Begin;
		while (End > Begin && FChar::IsWhitespace(Text[End - 1])) --End;
		if (End - Begin < 3 || Text[Begin] != '{' || Text[End - 1] != '}')
		{
			return false;
		}
		for (int32 I = Begin + 1; I < End - 1; ++I)
		{
			const TCHAR C = Text[I];
			if (!(FChar::IsAlnum(C) || C == '_' || C == '.'))
			{
				return false;
			}
		}
		if (OutPath)
		{
			*OutPath = Text.Mid(Begin + 1, End - Begin - 2);
		}
		return true;
	}

	bool ParseBoundValue(const FSlaterAttrDesc& Desc, const FString& Text, const ISlaterStyleSource& Style, FSlaterValue& OutValue)
	{
		OutValue.Type = Desc.Type;
		switch (Desc.Type)
		{
		case ESlaterAttrType::Text:
			OutValue.Text = Text;
			return true;
		case ESlaterAttrType::Bool:
			if (Text == TEXT("true") || Text == TEXT("false"))
			{
				OutValue.bBool = Text == TEXT("true");
				return true;
			}
			return false;
		case ESlaterAttrType::Int:
			return ParseInteger(Text, OutValue.Integer);
		case ESlaterAttrType::Enum:
		{
			const FName Word(*Text, FNAME_Find);
			if (!Word.IsNone() && Desc.EnumValues.Contains(Word))
			{
				OutValue.Word = Word;
				return true;
			}
			return false;
		}
		default:
			break;
		}
		// Metric, color, font and surface take the same path as a layout attribute.
		FSlaterBuildResult Scratch;
		FSlaterElementRegistry Empty;
		FLayoutBuilder Builder(TEXT("bound"), Style, Empty, Scratch);
		FSlaterNode Node;
		Node.Tag = TEXT("bound");
		FSlaterAttribute Attr;
		Attr.Name = Desc.Name.ToString();
		Attr.Value = Text;
		return Builder.ParseBound(Node, Attr, Desc, OutValue) && Scratch.Errors.Num() == 0;
	}

	TSharedPtr<const FSlaterCompiledTemplate> CompileTemplate(const FSlaterNode& Node, const FString& Path, const ISlaterStyleSource& Style, const FSlaterElementRegistry& Registry, TArray<FSlaterError>& OutErrors)
	{
		FSlaterBuildResult Scratch;
		FLayoutBuilder Builder(Path, Style, Registry, Scratch);
		Builder.TemplateDepth = 1;
		FSlaterSlotValues Slot;
		TSharedPtr<FSlaterCompiledTemplate::FNode> Resolved;
		const TSharedPtr<SWidget> Checked = Builder.Build(Node, nullptr, Slot, &Resolved);
		OutErrors = Scratch.Errors;
		if (!Checked.IsValid() || !Resolved.IsValid() || OutErrors.Num() > 0)
		{
			return nullptr;
		}
		const TSharedRef<FSlaterCompiledTemplate> Compiled = MakeShared<FSlaterCompiledTemplate>();
		Compiled->Root = Node;
		Compiled->RootNode = Resolved;
		Compiled->Path = Path;
		return Compiled;
	}

	bool InstantiateTemplate(const FSlaterCompiledTemplate& Template, const ISlaterStyleSource& Style, const FSlaterElementRegistry& Registry, FSlaterBuildResult& Out)
	{
		Out = FSlaterBuildResult();
		FLayoutBuilder Builder(Template.Path, Style, Registry, Out);
		Out.Root = Builder.Instantiate(*Template.RootNode, Template.Root);
		if (Out.Errors.Num() > 0)
		{
			Out.Root.Reset();
			return false;
		}
		return Out.Root.IsValid();
	}

	void GetTemplateBindings(const FSlaterCompiledTemplate& Template, TArray<TSharedRef<const FSlaterBindingSpec>>& OutSpecs)
	{
		TFunction<void(const FSlaterCompiledTemplate::FNode&)> Walk = [&](const FSlaterCompiledTemplate::FNode& Node)
		{
			OutSpecs.Append(Node.Bindings);
			for (const TSharedRef<FSlaterCompiledTemplate::FNode>& Child : Node.Children)
			{
				Walk(*Child);
			}
		};
		if (Template.RootNode.IsValid())
		{
			Walk(*Template.RootNode);
		}
	}

	void GetTemplateEvents(const FSlaterCompiledTemplate& Template, TArray<TSharedRef<const FSlaterEventSpec>>& OutSpecs)
	{
		TFunction<void(const FSlaterCompiledTemplate::FNode&)> Walk = [&](const FSlaterCompiledTemplate::FNode& Node)
		{
			for (const TSharedRef<const FSlaterEventSpec>& Event : Node.Events)
			{
				if (Event->IsWrite())
				{
					OutSpecs.Add(Event);
				}
			}
			for (const TSharedRef<FSlaterCompiledTemplate::FNode>& Child : Node.Children)
			{
				Walk(*Child);
			}
		};
		if (Template.RootNode.IsValid())
		{
			Walk(*Template.RootNode);
		}
	}

	void MeasureWidgetTree(const TSharedRef<SWidget>& Root, int32& OutCount, int32& OutDepth)
	{
		OutCount = 0;
		OutDepth = 0;
		Measure(*Root, 1, OutCount, OutDepth);
	}

	bool BuildLayout(const FSlaterNode& Root, const FString& Path, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget, const FSlaterElementRegistry& Registry)
	{
		Out = FSlaterBuildResult();
		// A layout that declares templates or imports is expanded into plain elements first (on a copy; Root is const).
		FSlaterNode Expanded;
		const FSlaterNode* Source = &Root;
		if (HasTemplateDeclarations(Root))
		{
			Expanded = Root;
			if (!ExpandTemplates(Expanded, Path, Registry, LayoutImportResolver(), Out.Errors))
			{
				return false;
			}
			Source = &Expanded;
		}
		// `<state>` declarations are taken out of the root (they are not widgets), then every state name used is checked.
		if (Source->Children.ContainsByPredicate([](const FSlaterNode& Child) { return Child.Tag.Equals(TEXT("state"), ESearchCase::CaseSensitive); }))
		{
			if (Source != &Expanded)
			{
				Expanded = *Source;
				Source = &Expanded;
			}
			ExtractStates(Expanded, Path, Out.States, Out.Errors);
		}
		CheckStateReferences(*Source, Path, Out.States, Out.Errors);
		if (Out.Errors.Num() > 0)
		{
			Out.States.Reset();
			return false;
		}
		FLayoutBuilder Builder(Path, Style, Registry, Out);
		FSlaterSlotValues RootSlot;
		Out.Root = Builder.Build(*Source, nullptr, RootSlot);
		if (Out.Errors.Num() > 0)
		{
			Out.Root.Reset();
			Out.ById.Reset();
			return false;
		}
		if (!Out.Root.IsValid())
		{
			return false;
		}

		MeasureWidgetTree(Out.Root.ToSharedRef(), Out.WidgetCount, Out.WidgetDepth);
		CollectOutlets(*Out.Root, Out.Outlets);
		// With no view around, a layout shows its states' defaults.
		for (const FSlaterStateCondition& Condition : Out.StateConditions)
		{
			if (const TSharedPtr<SWidget> Target = Condition.Widget.Pin())
			{
				const FSlaterStateDecl* Decl = Out.States.FindByPredicate([&](const FSlaterStateDecl& State) { return State.Name == Condition.Key; });
				SetShown(*Target, Decl && Decl->Default.Equals(Condition.Value, ESearchCase::CaseSensitive));
			}
		}
		for (const FSlaterStateSize& Size : Out.StateSizes)
		{
			const FSlaterStateDecl* Decl = Out.States.FindByPredicate([&](const FSlaterStateDecl& State) { return State.Name == Size.Key; });
			ApplyStateSize(Size, Decl ? &Decl->Default : nullptr);
		}
		auto Warn = [&](const FString& Message)
		{
			FSlaterError& Warning = Out.Warnings.AddDefaulted_GetRef();
			Warning.Path = Path;
			Warning.Line = Root.Pos.Line;
			Warning.Column = Root.Pos.Column;
			Warning.Message = Message;
		};
		if (Out.WidgetCount > Budget.MaxWidgets)
		{
			Warn(FString::Printf(TEXT("layout builds %d widgets, over the budget of %d"), Out.WidgetCount, Budget.MaxWidgets));
		}
		if (Out.WidgetDepth > Budget.MaxDepth)
		{
			Warn(FString::Printf(TEXT("layout is %d widgets deep, over the budget of %d"), Out.WidgetDepth, Budget.MaxDepth));
		}
		return true;
	}

	bool BuildLayoutFromText(const FString& Text, const FString& Path, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget, const FSlaterElementRegistry& Registry)
	{
		FSlaterNode Root;
		FSlaterError ParseError;
		if (!ParseMarkup(Text, Path, Root, ParseError))
		{
			Out = FSlaterBuildResult();
			AppendSyntaxStopsBuildNote(ParseError);
			Out.Errors.Add(ParseError);
			return false;
		}
		return BuildLayout(Root, Path, Style, Out, Budget, Registry);
	}

	void AppendSyntaxStopsBuildNote(FSlaterError& Error)
	{
		Error.Message += TEXT(" (a syntax error stops the build, so more errors may appear once this one is fixed)");
	}
}

bool FSlaterElementRegistry::Register(FSlaterElementDesc Desc, FString* OutError)
{
	auto Fail = [&](const FString& Reason)
	{
		if (OutError)
		{
			*OutError = Reason;
		}
		return false;
	};

	const FString Tag = Desc.Tag.ToString();
	if (!IsMarkupWord(Tag))
	{
		return Fail(FString::Printf(TEXT("'%s' is not a valid tag: lowercase letters, digits and hyphens, starting with a letter"), *Tag));
	}
	if (Elements.Contains(Desc.Tag))
	{
		return Fail(FString::Printf(TEXT("<%s> is already registered"), *Tag));
	}
	if (!Desc.Builder)
	{
		return Fail(FString::Printf(TEXT("<%s> has no builder"), *Tag));
	}

	TSet<FName> Seen;
	for (const FSlaterAttrDesc& Attr : Desc.Attributes)
	{
		const FString Name = Attr.Name.ToString();
		if (!IsMarkupWord(Name))
		{
			return Fail(FString::Printf(TEXT("<%s>: '%s' is not a valid attribute name"), *Tag, *Name));
		}
		if (Seen.Contains(Attr.Name))
		{
			return Fail(FString::Printf(TEXT("<%s>: attribute '%s' is declared twice"), *Tag, *Name));
		}
		Seen.Add(Attr.Name);
		if (Slater::CommonAttributes().ContainsByPredicate([&](const FSlaterAttrDesc& D) { return D.Name == Attr.Name; }) || IsSlotAttr(Attr.Name))
		{
			return Fail(FString::Printf(TEXT("<%s>: '%s' is a common or slot attribute and cannot be redeclared"), *Tag, *Name));
		}
		if (Attr.bEvent && (Attr.Type != ESlaterAttrType::Text || Attr.Bind.IsSet()))
		{
			return Fail(FString::Printf(TEXT("<%s>: event '%s' must be a plain Text attribute (make it with Slater::EventAttribute)"), *Tag, *Name));
		}
		if (Attr.Type == ESlaterAttrType::Enum && Attr.EnumValues.Num() == 0)
		{
			return Fail(FString::Printf(TEXT("<%s>: enum attribute '%s' lists no words"), *Tag, *Name));
		}
	}

	Elements.Add(Desc.Tag, MoveTemp(Desc));
	return true;
}

bool FSlaterElementRegistry::Unregister(FName Tag)
{
	return Elements.Remove(Tag) > 0;
}

const FSlaterElementDesc* FSlaterElementRegistry::Find(FName Tag) const
{
	return Elements.Find(Tag);
}

void FSlaterElementRegistry::GetTags(TArray<FString>& OutTags) const
{
	OutTags.Reset();
	for (const TPair<FName, FSlaterElementDesc>& Pair : Elements)
	{
		OutTags.Add(Pair.Key.ToString());
	}
	OutTags.Sort();
}
