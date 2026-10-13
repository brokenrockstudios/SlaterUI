// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterState.h"

#include "SlaterCommands.h"

namespace
{
	const TCHAR* const StateTag = TEXT("state");
	const TCHAR* const SetStateAttr = TEXT("set-state");
	const TCHAR* const VisibleWhenAttr = TEXT("visible-when");
	const TCHAR* const OnClickAttr = TEXT("on-click");

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
			if (Attr.Name.Equals(SetStateAttr, ESearchCase::CaseSensitive) || Attr.Name.Equals(VisibleWhenAttr, ESearchCase::CaseSensitive))
			{
				// The shape is the attribute's own check; here only the name matters.
				if (Slater::ParseStatePair(Attr.Value, Key, Value))
				{
					CheckKey(Key, Attr.Pos, Path, States, Errors);
				}
			}
			else if (Attr.Name.Equals(OnClickAttr, ESearchCase::CaseSensitive))
			{
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
