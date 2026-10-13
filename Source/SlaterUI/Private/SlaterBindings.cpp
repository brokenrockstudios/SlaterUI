// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterBindings.h"

#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterBindings, Log, All);

namespace
{
	/** Head of the list of sets that exist now, for the stale-binding report. Game thread only. */
	FSlaterBindingSet* GLiveHead = nullptr;

	FAutoConsoleCommand StaleBindingsCommand(
		TEXT("Slater.StaleBindings"),
		TEXT("Lists the SlaterUI bindings that show another value than their data holds (a view model that changed a field without broadcasting)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			TArray<FString> Lines;
			Slater::CollectStaleBindings(Lines);
			if (Lines.Num() == 0)
			{
				UE_LOG(LogSlaterBindings, Display, TEXT("Slater.StaleBindings: every pushed binding shows its data."));
			}
			for (const FString& Line : Lines)
			{
				UE_LOG(LogSlaterBindings, Warning, TEXT("Slater.StaleBindings: %s"), *Line);
			}
		}));
}

namespace Slater
{
	void CollectStaleBindings(TArray<FString>& OutLines)
	{
		FSlaterBindingSet::CollectAllStale(OutLines);
	}
}

FSlaterBindingSet::FSlaterBindingSet(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle, int32 ExpectedBindings)
	: Scope(MoveTemp(InScope))
	, Mode(InMode)
	, Style(&InStyle)
{
	Entries.Reserve(ExpectedBindings);
	NextLive = GLiveHead;
	if (GLiveHead)
	{
		GLiveHead->PrevLive = this;
	}
	GLiveHead = this;
}

void FSlaterBindingSet::CollectAllStale(TArray<FString>& OutLines)
{
	for (const FSlaterBindingSet* Set = GLiveHead; Set; Set = Set->NextLive)
	{
		Set->CollectStale(OutLines);
	}
}

FSlaterBindingSet::~FSlaterBindingSet()
{
	if (PrevLive)
	{
		PrevLive->NextLive = NextLive;
	}
	else
	{
		GLiveHead = NextLive;
	}
	if (NextLive)
	{
		NextLive->PrevLive = PrevLive;
	}
}

void FSlaterBindingSet::Add(const FSlaterBoundAttr& Bound)
{
	const int32 Index = Entries.Num();
	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Bound = Bound;
	const FString& Path = Bound.Spec->Path;

	if (Mode == ESlaterBindMode::Pushed)
	{
		FString Text;
		if (Scope->Read(Path, Text))
		{
			Write(Entry, Text);
		}
		Entry.Subscription = Scope->Subscribe(Path, [this, Index]() { Refresh(Index); });
		return;
	}

	// Polled: the widget evaluates the getter itself, every frame.
	const TSharedPtr<SWidget> Widget = Bound.Widget.Pin();
	if (!Widget.IsValid())
	{
		return;
	}
	const TSharedRef<const FSlaterBindingSpec> Spec = Bound.Spec.ToSharedRef();
	const ISlaterStyleSource* StylePtr = Style;
	TFunction<FSlaterValue()> Getter = [Spec, StylePtr, Source = Scope]()
	{
		++Slater::ListStats().PolledReads;
		FSlaterValue Value;
		FString Text;
		if (Source->Read(Spec->Path, Text))
		{
			Slater::ParseBoundValue(Spec->Desc, Text, *StylePtr, Value);
		}
		return Value;
	};
	Spec->Desc.Bind.ApplyPolled(*Widget, MoveTemp(Getter), *Style);
}

void FSlaterBindingSet::Refresh(int32 Index)
{
	FEntry& Entry = Entries[Index];
	FString Text;
	if (Scope->Read(Entry.Bound.Spec->Path, Text))
	{
		Write(Entry, Text);
	}
}

void FSlaterBindingSet::Write(FEntry& Entry, const FString& Text)
{
	Entry.Applied = Text;
	Entry.bApplied = true;
	const TSharedPtr<SWidget> Widget = Entry.Bound.Widget.Pin();
	FSlaterValue Value;
	// A value from data that does not fit the attribute leaves the widget as it was.
	if (Widget.IsValid() && Slater::ParseBoundValue(Entry.Bound.Spec->Desc, Text, *Style, Value))
	{
		Entry.Bound.Spec->Desc.Bind.Apply(*Widget, Value, *Style);
		++Slater::ListStats().Applies;
	}
}

void FSlaterBindingSet::CollectStale(TArray<FString>& OutLines) const
{
	if (Mode != ESlaterBindMode::Pushed)
	{
		return;
	}
	for (const FEntry& Entry : Entries)
	{
		FString Now;
		if (!Entry.Bound.Widget.IsValid() || !Scope->Read(Entry.Bound.Spec->Path, Now))
		{
			continue;
		}
		if (Entry.bApplied && Entry.Applied == Now)
		{
			continue;
		}
		const FSlaterBindingSpec& Spec = *Entry.Bound.Spec;
		OutLines.Add(FString::Printf(TEXT("%s(%d,%d): {%s} on '%s' %s, the data says '%s'"), *Label.ToString(), Spec.Pos.Line, Spec.Pos.Column, *Spec.Path, *Spec.Attr.ToString(),
			Entry.bApplied ? *FString::Printf(TEXT("shows '%s'"), *Entry.Applied) : TEXT("was never set"), *Now));
	}
}
