// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterExamples.h"
#include "SlaterCommands.h"
#include "SlaterScope.h"
#include "SlaterView.h"

/**
 * Hello, the smallest binding there is. The layout (`Examples/Hello.sltr`) reads `{Count}` and `{Greeting}` and names one
 * command, `Hello.Add`. Everything C++ does is here:
 *
 *  1. Keep the data somewhere that outlives the view (a shared int here; your own object in a real screen).
 *  2. Give the layout a scope: `FSlaterGetterScope` maps a path to a function returning text. A getter may compute
 *     anything, so `Greeting` is derived from the count and has no storage of its own.
 *  3. Register the commands the layout names on the view (`GetCommands`), so they exist only while this view does.
 *  4. When the data changes, `Notify(Path)` for each path that reads it. The bound widgets update; nothing else does.
 *  5. `SetContext` checks every `{Path}` in the layout against the scope and fails the build, with the line, on a typo.
 */
TSharedRef<SSlaterView> SlaterExamples::MakeHello(FName StyleName)
{
	const TSharedRef<int32> Count = MakeShared<int32>(0);

	const TSharedRef<FSlaterGetterScope> Scope = MakeShared<FSlaterGetterScope>();
	Scope->AddValue("Count", [Count]() { return FString::FromInt(*Count); });
	Scope->AddValue("Greeting", [Count]()
	{
		if (*Count == 0)
		{
			return FString(TEXT("Press the button."));
		}
		return *Count < 10 ? FString(TEXT("Keep going.")) : FString(TEXT("That is plenty."));
	});

	const TSharedRef<SSlaterView> View = SNew(SSlaterView).LayoutId(TEXT("Examples/Hello")).StyleName(StyleName);

	// The handlers hold the scope and the count, not the view, so the view and its handlers can go together.
	View->GetCommands().Register(TEXT("Hello.Add"), [Count, Scope](const FSlaterCommandCall&)
	{
		++*Count;
		Scope->Notify("Count");
		Scope->Notify("Greeting");
	});
	View->GetCommands().Register(TEXT("Hello.Reset"), [Count, Scope](const FSlaterCommandCall&)
	{
		*Count = 0;
		Scope->Notify("Count");
		Scope->Notify("Greeting");
	});

	View->SetContext(Scope);
	return View;
}
