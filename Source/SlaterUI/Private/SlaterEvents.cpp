// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterEvents.h"

#include "SlaterCommands.h"
#include "SlaterPopup.h"
#include "SlaterView.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterEvents, Log, All);

void FSlaterEventSink::Fire(FName Event, const FString& Value)
{
	if (ApplyDepth > 0 || bFiring)
	{
		// The widget echoing a value a binding wrote into it, or a setter that pushed a value back while this fire runs.
		++Dropped;
		return;
	}
	TGuardValue<bool> Firing(bFiring, true);
	for (const TSharedRef<const FSlaterEventSpec>& Spec : Specs)
	{
		if (Spec->Event != Event)
		{
			continue;
		}
		++Handled;
		if (Spec->IsWrite())
		{
			if (Writer)
			{
				Writer(*Spec, Value);
			}
			else if (!bWarnedNoWriter)
			{
				bWarnedNoWriter = true;
				UE_LOG(LogSlaterEvents, Warning, TEXT("SlaterUI: %s=\"{%s}\" (line %d) has no data to write to: the layout is not bound to a context"), *Spec->Event.ToString(), *Spec->Path, Spec->Pos.Line);
			}
			continue;
		}
		const TSharedPtr<SWidget> Source = Owner.Pin();
		FSlaterCommandCall Call;
		Call.Name = Spec->Command;
		Call.Source = Source;
		Call.Argument = Spec->Argument;
		Call.Value = Value;
		const TSharedPtr<SSlaterView> View = Source.IsValid() ? Slater::FindOwningView(*Source) : nullptr;
		if (View.IsValid())
		{
			View->RunCommand(Call);
		}
		else
		{
			Slater::RunCommand(Slater::Commands(), Call);
		}
	}
}

bool FSlaterEventSink::HasWriteBack() const
{
	return Specs.ContainsByPredicate([](const TSharedRef<const FSlaterEventSpec>& Spec) { return Spec->IsWrite(); });
}

namespace Slater
{
	FSlaterAttrDesc EventAttribute(const TCHAR* Name)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = ESlaterAttrType::Text;
		Desc.bEvent = true;
		Desc.CheckText = [Name](const FString& Text)
		{
			if (IsBindingExpr(Text))
			{
				return FString();
			}
			const FString Problem = CheckCommandText(Text);
			return Problem.IsEmpty() ? FString() : FString::Printf(TEXT("%s; %s takes {Path} to write the value to the data, or a command name"), *Problem, Name);
		};
		return Desc;
	}

	void AttachEventSink(SWidget& Widget, const TSharedRef<FSlaterEventSink>& Sink)
	{
		Sink->SetOwner(Widget.AsShared());
		Widget.AddMetadata(Sink);
	}

	TSharedPtr<FSlaterEventSink> FindEventSink(const SWidget& Widget)
	{
		return Widget.GetMetaData<FSlaterEventSink>();
	}

	FString NumberText(double Number)
	{
		// The same rule as a view model's float read as text, so a value written back compares equal to what it reads.
		return (Number == FMath::FloorToDouble(Number) && FMath::Abs(Number) < 1e9) ? FString::FromInt(static_cast<int32>(Number)) : FString::SanitizeFloat(Number);
	}

	TSharedPtr<SSlaterView> FindOwningView(const SWidget& Widget)
	{
		static const FName ViewType(TEXT("SSlaterView"));
		static const FName PopupMenuType(TEXT("SSlaterPopupMenu"));
		for (TSharedPtr<SWidget> Parent = Widget.GetParentWidget(); Parent.IsValid(); Parent = Parent->GetParentWidget())
		{
			if (Parent->GetType() == ViewType)
			{
				return StaticCastSharedPtr<SSlaterView>(Parent);
			}
			if (Parent->GetType() == PopupMenuType)
			{
				// The engine's menu stack parents the menu content to its own window; continue from the popup it opened from (T-439).
				Parent = StaticCastSharedPtr<SSlaterPopupMenu>(Parent)->GetPopup();
				if (!Parent.IsValid())
				{
					return nullptr;
				}
			}
		}
		return nullptr;
	}
}
