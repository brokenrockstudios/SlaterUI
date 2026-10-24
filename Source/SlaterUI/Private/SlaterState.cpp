// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterState.h"

#include "SlaterCommands.h"
#include "Widgets/Layout/SBox.h"

namespace SlaterStatePrivate
{
	const TCHAR* const StateTag = TEXT("state");
	const TCHAR* const SetStateAttr = TEXT("set-state");
	const TCHAR* const HoverSetStateAttr = TEXT("hover-set-state");
	const TCHAR* const LeaveSetStateAttr = TEXT("leave-set-state");
	const TCHAR* const VisibleWhenAttr = TEXT("visible-when");
	const TCHAR* const OpenWhenAttr = TEXT("open-when");
	const TCHAR* const CloseStateAttr = TEXT("close-state");
	const TCHAR* const OnClickAttr = TEXT("on-click");
	const TCHAR* const EventAttrPrefix = TEXT("on-");
	const TCHAR* const WidthByAttr = TEXT("width-by");
	const TCHAR* const HeightByAttr = TEXT("height-by");

	void AddError(TArray<FSlaterError>& Errors, const FString& Path, const FSlaterPos& Pos, const FString& Message)
	{
		FSlaterError& Error = Errors.AddDefaulted_GetRef();
		// An element of an imported template is reported under its own file.
		Error.Path = Pos.File.IsNone() ? Path : Pos.File.ToString();
		Error.Line = Pos.Line;
		Error.Column = Pos.Column;
		Error.Message = Message;
	}

	FString DeclaredNames(const TArray<FSlaterStateDecl>& States)
	{
		if (States.Num() == 0)
		{
			return TEXT("the layout declares none: add <state name=\"tab\" default=\"first\"/> as a child of the root element");
		}
		TArray<FString> Names;
		for (const FSlaterStateDecl& State : States)
		{
			Names.Add(State.Name.ToString());
		}
		return FString::Printf(TEXT("declared: %s"), *FString::Join(Names, TEXT(", ")));
	}

	void CheckKey(const FString& Key, const FSlaterPos& Pos, const FString& Path, const TArray<FSlaterStateDecl>& States, TArray<FSlaterError>& Errors)
	{
		if (!States.ContainsByPredicate([&](const FSlaterStateDecl& State) { return State.Name.ToString().Equals(Key, ESearchCase::CaseSensitive); }))
		{
			AddError(Errors, Path, Pos, FString::Printf(TEXT("'%s' is not a declared state; %s"), *Key, *DeclaredNames(States)));
		}
	}

	void Walk(const FSlaterNode& Node, const FString& Path, const TArray<FSlaterStateDecl>& States, TArray<FSlaterError>& Errors)
	{
		if (Node.Tag.Equals(StateTag, ESearchCase::CaseSensitive))
		{
			AddError(Errors, Path, Node.Pos, TEXT("<state> must be a direct child of the root element of the layout"));
		}
		for (const FSlaterAttribute& Attr : Node.Attributes)
		{
			FString Key, Value;
			if (Attr.Name.Equals(SetStateAttr, ESearchCase::CaseSensitive) || Attr.Name.Equals(VisibleWhenAttr, ESearchCase::CaseSensitive)
				|| Attr.Name.Equals(HoverSetStateAttr, ESearchCase::CaseSensitive) || Attr.Name.Equals(LeaveSetStateAttr, ESearchCase::CaseSensitive)
				|| Attr.Name.Equals(OpenWhenAttr, ESearchCase::CaseSensitive) || Attr.Name.Equals(CloseStateAttr, ESearchCase::CaseSensitive))
			{
				// The shape is the attribute's own check; here only the name matters.
				if (Slater::ParseStatePair(Attr.Value, Key, Value))
				{
					CheckKey(Key, Attr.Pos, Path, States, Errors);
				}
			}
			else if (Attr.Name.Equals(WidthByAttr, ESearchCase::CaseSensitive) || Attr.Name.Equals(HeightByAttr, ESearchCase::CaseSensitive))
			{
				TArray<Slater::FSizeByCase> Cases;
				if (Slater::ParseSizeBy(Attr.Value, *Attr.Name, Key, Cases).IsEmpty())
				{
					CheckKey(Key, Attr.Pos, Path, States, Errors);
				}
			}
			else if (Attr.Name.Equals(OnClickAttr, ESearchCase::CaseSensitive) || Attr.Name.StartsWith(EventAttrPrefix, ESearchCase::CaseSensitive))
			{
				// `on-click`, and an element event (`on-change`, `on-commit`) naming a stock command.
				FString Name, Argument;
				Slater::SplitCommandText(Attr.Value, Name, Argument);
				if (Name.Equals(TEXT("ui.toggle"), ESearchCase::CaseSensitive) && Slater::IsValidStateName(Argument))
				{
					CheckKey(Argument, Attr.Pos, Path, States, Errors);
				}
			}
		}
		for (const FSlaterNode& Child : Node.Children)
		{
			Walk(Child, Path, States, Errors);
		}
	}
}
using namespace SlaterStatePrivate;

bool Slater::IsValidStateName(const FString& Name)
{
	if (Name.IsEmpty() || !((Name[0] < 128 && FChar::IsAlpha(Name[0])) || Name[0] == '_'))
	{
		return false;
	}
	for (const TCHAR C : Name)
	{
		if (!(C < 128 && (FChar::IsAlnum(C) || C == '_')))
		{
			return false;
		}
	}
	return true;
}

bool Slater::IsValidStateValue(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return false;
	}
	for (const TCHAR C : Value)
	{
		if (!(C < 128 && (FChar::IsAlnum(C) || C == '_' || C == '.' || C == '-')))
		{
			return false;
		}
	}
	return true;
}

bool Slater::ParseStatePair(const FString& Text, FString& OutKey, FString& OutValue)
{
	FString Key, Value;
	if (!Text.Split(TEXT("="), &Key, &Value))
	{
		return false;
	}
	if (!IsValidStateName(Key) || !IsValidStateValue(Value))
	{
		return false;
	}
	OutKey = MoveTemp(Key);
	OutValue = MoveTemp(Value);
	return true;
}

FString Slater::CheckStatePair(const FString& Text, const TCHAR* AttributeName)
{
	FString Key, Value;
	if (ParseStatePair(Text, Key, Value))
	{
		return FString();
	}
	return FString::Printf(TEXT("'%s' is not a state test for %s: write one name, '=' and one value, like tab=gear (letters, digits and '_' in the name; letters, digits and '_', '.', '-' in the value)"), *Text, AttributeName);
}

FString Slater::ParseSizeBy(const FString& Text, const TCHAR* AttributeName, FString& OutKey, TArray<FSizeByCase>& OutCases)
{
	const FString Shape = FString::Printf(TEXT("'%s' is not a state-driven size for %s: write the state name, ':', then comma-separated value=size cases, like expanded: true=240, false=UI.Size.Rail"), *Text, AttributeName);
	FString KeyText, CaseText;
	if (!Text.Split(TEXT(":"), &KeyText, &CaseText))
	{
		return Shape;
	}
	KeyText.TrimStartAndEndInline();
	if (!IsValidStateName(KeyText))
	{
		return Shape;
	}
	TArray<FString> Parts;
	CaseText.ParseIntoArray(Parts, TEXT(","), false);
	TArray<FSizeByCase> Cases;
	for (const FString& Part : Parts)
	{
		FSizeByCase Case;
		if (!Part.Split(TEXT("="), &Case.Value, &Case.Metric))
		{
			return Shape;
		}
		Case.Value.TrimStartAndEndInline();
		Case.Metric.TrimStartAndEndInline();
		if (!IsValidStateValue(Case.Value) || Case.Metric.IsEmpty())
		{
			return Shape;
		}
		if (Cases.ContainsByPredicate([&](const FSizeByCase& Other) { return Other.Value.Equals(Case.Value, ESearchCase::CaseSensitive); }))
		{
			return FString::Printf(TEXT("the value '%s' appears twice in %s; each state value gets one size"), *Case.Value, AttributeName);
		}
		Cases.Add(MoveTemp(Case));
	}
	if (Cases.Num() == 0)
	{
		return Shape;
	}
	OutKey = MoveTemp(KeyText);
	OutCases = MoveTemp(Cases);
	return FString();
}

namespace SlaterStatePrivate
{
	void WriteSize(const FSlaterStateSize& Size, const FOptionalSize& Value)
	{
		const TSharedPtr<SBox> Box = Size.Box.Pin();
		if (!Box.IsValid())
		{
			return;
		}
		if (Size.bHeight)
		{
			Box->SetHeightOverride(Value);
		}
		else
		{
			Box->SetWidthOverride(Value);
		}
	}
}
using namespace SlaterStatePrivate;

void Slater::ApplyStateSize(const FSlaterStateSize& Size, const FString* StateValue, bool bAnimate)
{
	if (!Size.Box.IsValid())
	{
		return;
	}
	int32 Index = INDEX_NONE;
	if (StateValue)
	{
		Index = Size.Values.IndexOfByPredicate([&](const FString& Value) { return Value.Equals(*StateValue, ESearchCase::CaseSensitive); });
	}
	if (Index == INDEX_NONE)
	{
		// No case: back to the content size, and nothing to move from next time.
		if (Size.Anim.IsValid())
		{
			Size.Anim->Reset();
		}
		WriteSize(Size, FOptionalSize());
		return;
	}
	const float Target = Size.Sizes[Index];
	if (Size.bAnimate && Size.Anim.IsValid())
	{
		ECurveEaseFunction Ease = ECurveEaseFunction::Linear;
		Slater::EaseFromWord(Size.Motion.Ease, Ease);
		const float Duration = bAnimate ? Size.Motion.Duration * Slater::GetMotionScale() : 0.f;
		if (Size.Anim->HasValue() && Size.Anim->GetTarget() == Target)
		{
			return;
		}
		Size.Anim->Start(Target, Duration, Ease);
		WriteSize(Size, FOptionalSize(Size.Anim->GetValue()));
		return;
	}
	WriteSize(Size, FOptionalSize(Target));
}

bool Slater::AdvanceStateSize(const FSlaterStateSize& Size, float DeltaTime)
{
	if (!Size.Anim.IsValid() || !Size.Anim->IsActive())
	{
		return false;
	}
	const bool bMoving = Size.Anim->Advance(DeltaTime);
	WriteSize(Size, FOptionalSize(Size.Anim->GetValue()));
	return bMoving;
}

void Slater::ExtractStates(FSlaterNode& Root, const FString& Path, TArray<FSlaterStateDecl>& OutStates, TArray<FSlaterError>& OutErrors)
{
	TArray<FSlaterNode> Kept;
	for (FSlaterNode& Child : Root.Children)
	{
		if (!Child.Tag.Equals(StateTag, ESearchCase::CaseSensitive))
		{
			Kept.Add(MoveTemp(Child));
			continue;
		}

		const FSlaterAttribute* NameAttr = Child.FindAttribute(TEXT("name"));
		const FSlaterAttribute* DefaultAttr = Child.FindAttribute(TEXT("default"));
		for (const FSlaterAttribute& Attr : Child.Attributes)
		{
			if (!Attr.Name.Equals(TEXT("name"), ESearchCase::CaseSensitive) && !Attr.Name.Equals(TEXT("default"), ESearchCase::CaseSensitive))
			{
				AddError(OutErrors, Path, Attr.Pos, FString::Printf(TEXT("unknown attribute '%s' on <state>; allowed: name, default"), *Attr.Name));
			}
		}
		if (Child.Children.Num() > 0)
		{
			AddError(OutErrors, Path, Child.Children[0].Pos, TEXT("<state> cannot have child elements"));
		}
		if (!Child.Text.IsEmpty())
		{
			AddError(OutErrors, Path, Child.TextPos, TEXT("<state> cannot contain text"));
		}
		if (!NameAttr)
		{
			AddError(OutErrors, Path, Child.Pos, TEXT("<state> needs the attribute 'name' (text), for example name=\"tab\""));
		}
		else if (!IsValidStateName(NameAttr->Value))
		{
			AddError(OutErrors, Path, NameAttr->Pos, FString::Printf(TEXT("'%s' is not a state name: letters, digits and '_', not starting with a digit"), *NameAttr->Value));
		}
		if (!DefaultAttr)
		{
			AddError(OutErrors, Path, Child.Pos, TEXT("<state> needs the attribute 'default' (text), for example default=\"first\""));
		}
		else if (!IsValidStateValue(DefaultAttr->Value))
		{
			AddError(OutErrors, Path, DefaultAttr->Pos, FString::Printf(TEXT("'%s' is not a state value: letters, digits and '_', '.', '-', at least one"), *DefaultAttr->Value));
		}
		if (NameAttr && DefaultAttr && IsValidStateName(NameAttr->Value) && IsValidStateValue(DefaultAttr->Value))
		{
			const FName Name(*NameAttr->Value);
			if (const FSlaterStateDecl* First = OutStates.FindByPredicate([&](const FSlaterStateDecl& State) { return State.Name == Name; }))
			{
				AddError(OutErrors, Path, NameAttr->Pos, FString::Printf(TEXT("duplicate state '%s' (first declared at %d,%d)"), *NameAttr->Value, First->Pos.Line, First->Pos.Column));
			}
			else
			{
				OutStates.Add({Name, DefaultAttr->Value, NameAttr->Pos});
			}
		}
	}
	Root.Children = MoveTemp(Kept);
}

void Slater::CheckStateReferences(const FSlaterNode& Root, const FString& Path, const TArray<FSlaterStateDecl>& States, TArray<FSlaterError>& OutErrors)
{
	Walk(Root, Path, States, OutErrors);
}
