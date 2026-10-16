// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterElements.h"
#include "Types/ISlateMetaData.h"

class SSlaterView;
class SWidget;

/**
 * Element events (T-381): how a control tells the layout that the user changed it. The element's builder makes a sink,
 * wires the Slate widget's own delegate (`OnValueChanged`, `OnCheckStateChanged`, `OnTextCommitted`) to `Fire`, and attaches
 * the sink to the widget with `Slater::AttachEventSink`. The layout build gives the sink what each event attribute says;
 * a command event runs at once (through the nearest `SSlaterView`, like `on-click`), a write-back event (`on-change="{Volume}"`)
 * goes to the writer that `Slater::Bind` installs, which writes the value to the data through its setter.
 *
 * Echo guard: a binding that writes a value into the widget does it inside an `FApplyScope`, so a widget whose setter
 * fires its own change delegate does not write the value straight back; and a fire that arrives while an earlier one of
 * the same sink is still running (a setter that pushes a different value into the control) is dropped.
 */
class SLATERUI_API FSlaterEventSink : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FSlaterEventSink, ISlateMetaData)

	/** Writes the value of a write-back event to the data. Installed per binding set; empty while the layout has no data. */
	using FWriter = TFunction<void(const FSlaterEventSpec& Spec, const FString& Value)>;

	/** The user changed the control: run what the event attribute says, with the value as text. Nothing when the element has no such attribute. */
	void Fire(FName Event, const FString& Value);

	void AddSpec(TSharedRef<const FSlaterEventSpec> Spec) { Specs.Add(MoveTemp(Spec)); }
	const TArray<TSharedRef<const FSlaterEventSpec>>& GetSpecs() const { return Specs; }
	bool HasWriteBack() const;

	void SetWriter(FWriter InWriter) { Writer = MoveTemp(InWriter); }
	/** The control's widget; `Slater::AttachEventSink` sets it. A command event finds its view from here. */
	void SetOwner(const TSharedRef<SWidget>& InOwner) { Owner = InOwner; }
	TWeakPtr<SWidget> GetOwner() const { return Owner; }

	/** Fires that ran something (a write or a command). Echoes and fires during another fire are not counted. For tests. */
	int32 NumHandled() const { return Handled; }
	/** Fires dropped as echoes of a binding's own write, or as fires inside a running fire. For tests. */
	int32 NumDropped() const { return Dropped; }

	/** While one lives, a fire from this sink is the widget echoing a value a binding just wrote, and is dropped. */
	class FApplyScope
	{
	public:
		explicit FApplyScope(FSlaterEventSink* InSink) : Sink(InSink) { if (Sink) { ++Sink->ApplyDepth; } }
		~FApplyScope() { if (Sink) { --Sink->ApplyDepth; } }
		FApplyScope(const FApplyScope&) = delete;
		FApplyScope& operator=(const FApplyScope&) = delete;
	private:
		FSlaterEventSink* Sink;
	};

private:
	TArray<TSharedRef<const FSlaterEventSpec>> Specs;
	FWriter Writer;
	TWeakPtr<SWidget> Owner;
	int32 ApplyDepth = 0;
	bool bFiring = false;
	bool bWarnedNoWriter = false;
	int32 Handled = 0;
	int32 Dropped = 0;
};

namespace Slater
{
	/**
	 * An event attribute for an element descriptor: `on-change="{Path}"` writes the value back, `on-change="Name"` or
	 * `on-change="Name:Word"` runs a command (the value arrives in `FSlaterCommandCall::Value`). Anything else is refused at the attribute.
	 */
	SLATERUI_API FSlaterAttrDesc EventAttribute(const TCHAR* Name);

	/** Attaches the sink to the control's widget (as Slate metadata) and makes the widget its owner. Call from the builder. */
	SLATERUI_API void AttachEventSink(SWidget& Widget, const TSharedRef<FSlaterEventSink>& Sink);

	/** The sink of a widget, or null. */
	SLATERUI_API TSharedPtr<FSlaterEventSink> FindEventSink(const SWidget& Widget);

	/** A number as an event's text, the way `{Path}` prints one: whole numbers without a decimal point (`3`, `0.25`). */
	SLATERUI_API FString NumberText(double Number);

	/** The nearest `SSlaterView` above a widget (its parents, walked at call time), or null. */
	SLATERUI_API TSharedPtr<SSlaterView> FindOwningView(const SWidget& Widget);
}
