// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterList.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SUniformGridPanel.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterList, Log, All);

namespace
{
	FSlaterListStats GStats;
	TSharedPtr<const ISlaterImageSource> GImageSource;

	TSharedPtr<SWidget> BuildRepeat(FSlaterBuildContext& Context)
	{
		FSlaterRepeatConfig Config;
		if (!Slater::IsBindingExpr(Context.GetText(TEXT("source")), &Config.SourcePath))
		{
			Context.Failure = TEXT("<repeat> 'source' must name the list in braces, like source=\"{Items}\"");
			return nullptr;
		}
		if (const FSlaterAttribute* SourceAttr = Context.Node.FindAttribute(TEXT("source")))
		{
			Config.SourcePos = SourceAttr->Pos;
		}
		Config.KeyField = Context.GetText(TEXT("key"));
		Config.AtField = Context.GetText(TEXT("at"));
		Config.Columns = Context.GetInt(TEXT("columns"), 1);
		if (Config.KeyField.IsEmpty() || Config.Columns < 1)
		{
			Context.Failure = TEXT("<repeat> needs a 'key' field name and 'columns' of at least 1");
			return nullptr;
		}
		if (Context.Node.Children.Num() != 1)
		{
			Context.Failure = TEXT("<repeat> needs one child element: the template of one item");
			return nullptr;
		}
		Config.Template = Context.Template;
		if (!Config.Template.IsValid())
		{
			Context.Failure = TEXT("<repeat> could not compile its template");
			return nullptr;
		}
		Config.CellWidth = Context.GetMetric(TEXT("cell-width"));
		Config.CellHeight = Context.GetMetric(TEXT("cell-height"));
		Config.Style = Context.Style;
		Config.Registry = Context.Registry;
		TSharedRef<SSlaterRepeat> Widget = SNew(SSlaterRepeat, MoveTemp(Config));
		// The grid itself takes no clicks (the host hit-tests cells and items, see the proposal's cost section); a
		// `surface` with `on-click` inside an item still does.
		Widget->SetVisibility(EVisibility::SelfHitTestInvisible);
		return Widget;
	}

	TSharedPtr<SWidget> BuildImage(FSlaterBuildContext& Context)
	{
		static const FName SrcName(TEXT("src"));
		static const FName SizeName(TEXT("size"));
		// Arguments, not setters: a setter on a fresh widget only invalidates it.
		const FSlaterValue* Src = Context.Values.Find(SrcName);
		// No `src` (it is bound) keeps SImage's default brush until the binding writes one, as before.
		const FSlateBrush* Brush = Src ? Slater::FindImage(FName(*Src->Text, FNAME_Find)) : FCoreStyle::Get().GetDefaultBrush();
		// `visible` is a common attribute: the layout builder applies it; this is the visibility the image has when shown.
		const EVisibility Visibility = EVisibility::HitTestInvisible;
		// `tint` multiplies the brush color (the stock icons are white); without it the image keeps its own colors.
		const FSlateColor Tint = FSlateColor(Context.GetColor(TEXT("tint")));
		TSharedPtr<SImage> Image;
		if (Context.Has(SizeName))
		{
			const float Size = Context.GetMetric(SizeName);
			Image = SNew(SImage).Image(Brush).ColorAndOpacity(Tint).DesiredSizeOverride(FVector2D(Size, Size)).Visibility(Visibility);
		}
		else
		{
			Image = SNew(SImage).Image(Brush).ColorAndOpacity(Tint).Visibility(Visibility);
		}
		return Image.ToSharedRef();
	}

	FSlaterAttrDesc ListAttr(const TCHAR* Name, ESlaterAttrType Type, bool bRequired = false)
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		Desc.bRequired = bRequired;
		return Desc;
	}
}

namespace Slater
{
	FSlaterListStats& ListStats()
	{
		return GStats;
	}

	void SetImageSource(TSharedPtr<const ISlaterImageSource> Source)
	{
		GImageSource = MoveTemp(Source);
	}

	bool HasImageSource()
	{
		return GImageSource.IsValid();
	}

	const FSlateBrush* FindImage(FName Name)
	{
		return (GImageSource.IsValid() && !Name.IsNone()) ? GImageSource->FindImage(Name) : nullptr;
	}

	void RegisterListElements(FSlaterElementRegistry& Registry)
	{
		FString Error;

		FSlaterElementDesc RepeatDesc;
		RepeatDesc.Tag = TEXT("repeat");
		RepeatDesc.Purpose = TEXT("one copy of its child per item of a list");
		RepeatDesc.Attributes = {
			ListAttr(TEXT("source"), ESlaterAttrType::Text, true),
			ListAttr(TEXT("key"), ESlaterAttrType::Text, true),
			ListAttr(TEXT("at"), ESlaterAttrType::Text),
			ListAttr(TEXT("columns"), ESlaterAttrType::Int),
			ListAttr(TEXT("cell-width"), ESlaterAttrType::Metric),
			ListAttr(TEXT("cell-height"), ESlaterAttrType::Metric),
		};
		RepeatDesc.Children = ESlaterChildRule::One;
		RepeatDesc.bTemplateChildren = true;
		RepeatDesc.Builder = &BuildRepeat;
		const bool bRepeat = Registry.Register(MoveTemp(RepeatDesc), &Error);
		ensureMsgf(bRepeat, TEXT("SlaterUI stock element <repeat>: %s"), *Error);

		FSlaterElementDesc ImageDesc;
		ImageDesc.Tag = TEXT("image");
		ImageDesc.Purpose = TEXT("a picture");
		FSlaterAttrDesc Src = ListAttr(TEXT("src"), ESlaterAttrType::Text);
		Src.Bind.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource&)
		{
			static_cast<SImage&>(Widget).SetImage(FindImage(FName(*Value.Text, FNAME_Find)));
		};
		Src.Bind.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource&)
		{
			static_cast<SImage&>(Widget).SetImage(TAttribute<const FSlateBrush*>::CreateLambda([Get = MoveTemp(Get)]() { return FindImage(FName(*Get().Text, FNAME_Find)); }));
		};
		ImageDesc.Attributes = {Src, ListAttr(TEXT("size"), ESlaterAttrType::Metric), ListAttr(TEXT("tint"), ESlaterAttrType::Color)};
		ImageDesc.Children = ESlaterChildRule::None;
		ImageDesc.Builder = &BuildImage;
		const bool bImage = Registry.Register(MoveTemp(ImageDesc), &Error);
		ensureMsgf(bImage, TEXT("SlaterUI stock element <image>: %s"), *Error);
	}

	TSharedRef<FSlaterBinder> Bind(const FSlaterBuildResult& Result, TSharedRef<FSlaterModel> Model, ESlaterBindMode Mode, const ISlaterStyleSource& Style)
	{
		return Bind(Result, MakeModelScope(Model), Mode, Style);
	}

	TSharedRef<FSlaterBinder> Bind(const FSlaterBuildResult& Result, TSharedRef<ISlaterScope> Scope, ESlaterBindMode Mode, const ISlaterStyleSource& Style)
	{
		TSharedRef<FSlaterBinder> Binder = MakeShared<FSlaterBinder>(Scope, Mode, Style);
		Binder->Attach(Result);
		return Binder;
	}

	bool CheckBindings(const FSlaterBuildResult& Result, ISlaterScope& Scope, const FString& Label, TArray<FSlaterError>& OutErrors)
	{
		const int32 ErrorsBefore = OutErrors.Num();
		auto Report = [&](const FSlaterPos& Pos, const FString& Message)
		{
			FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
			Error.Path = Pos.File.IsNone() ? Label : Pos.File.ToString();
			Error.Line = Pos.Line;
			Error.Column = Pos.Column;
			Error.Message = Message;
		};
		for (const FSlaterBoundAttr& Bound : Result.Bindings)
		{
			FString Why;
			if (!Scope.Check(Bound.Spec->Path, false, Why))
			{
				Report(Bound.Spec->Pos, FString::Printf(TEXT("{%s} cannot be bound: %s"), *Bound.Spec->Path, *Why));
			}
		}
		for (const FSlaterBoundEvent& Event : Result.Events)
		{
			FString Why;
			if (!Scope.CheckWrite(Event.Spec->Path, Why))
			{
				Report(Event.Spec->Pos, FString::Printf(TEXT("%s=\"{%s}\" cannot write to the data: %s"), *Event.Spec->Event.ToString(), *Event.Spec->Path, *Why));
			}
		}
		for (const TWeakPtr<SWidget>& Weak : Result.Repeats)
		{
			const TSharedPtr<SWidget> Widget = Weak.Pin();
			if (!Widget.IsValid())
			{
				continue;
			}
			const FSlaterRepeatConfig& Config = StaticCastSharedRef<SSlaterRepeat>(Widget.ToSharedRef())->GetConfig();
			FString Why;
			if (!Scope.Check(Config.SourcePath, true, Why))
			{
				Report(Config.SourcePos, FString::Printf(TEXT("<repeat> cannot use {%s}: %s"), *Config.SourcePath, *Why));
				continue;
			}
			// The item template's paths are checked against the first item, when the list has one.
			const TSharedPtr<ISlaterListSource> List = Scope.FindList(Config.SourcePath, Config.KeyField);
			const TSharedPtr<ISlaterScope> First = (List.IsValid() && List->Num() > 0) ? List->ItemAt(0) : nullptr;
			if (!First.IsValid() || !Config.Template.IsValid())
			{
				continue;
			}
			TArray<TSharedRef<const FSlaterBindingSpec>> Specs;
			GetTemplateBindings(*Config.Template, Specs);
			for (const TSharedRef<const FSlaterBindingSpec>& Spec : Specs)
			{
				if (!First->Check(Spec->Path, false, Why))
				{
					Report(Spec->Pos, FString::Printf(TEXT("{%s} cannot be bound in the <repeat> of {%s}: %s"), *Spec->Path, *Config.SourcePath, *Why));
				}
			}
			TArray<TSharedRef<const FSlaterEventSpec>> Events;
			GetTemplateEvents(*Config.Template, Events);
			for (const TSharedRef<const FSlaterEventSpec>& Event : Events)
			{
				if (!First->CheckWrite(Event->Path, Why))
				{
					Report(Event->Pos, FString::Printf(TEXT("%s=\"{%s}\" cannot write to an item of {%s}: %s"), *Event->Event.ToString(), *Event->Path, *Config.SourcePath, *Why));
				}
			}
		}
		return OutErrors.Num() == ErrorsBefore;
	}

	bool LoadMockFor(const FString& LayoutFilePath, FSlaterModel& Model, FSlaterError& OutError)
	{
		return Model.LoadMockFile(FPaths::ChangeExtension(LayoutFilePath, TEXT("mock.json")), OutError);
	}
}

// ---- SSlaterRepeat ----

void SSlaterRepeat::Construct(const FArguments&, FSlaterRepeatConfig InConfig)
{
	Config = MoveTemp(InConfig);
	ChildSlot
	[
		SAssignNew(Grid, SUniformGridPanel)
		.MinDesiredSlotWidth(Config.CellWidth)
		.MinDesiredSlotHeight(Config.CellHeight)
	];
}

SSlaterRepeat::~SSlaterRepeat()
{
	if (Source.IsValid())
	{
		Source->OnChanged.RemoveAll(this);
	}
}

void SSlaterRepeat::Attach(TSharedRef<FSlaterModel> InModel, ESlaterBindMode InMode)
{
	Attach(Slater::MakeModelScope(InModel), InMode);
}

void SSlaterRepeat::Attach(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode)
{
	if (Source.IsValid())
	{
		Source->OnChanged.RemoveAll(this);
	}
	Grid->ClearChildren();
	Items.Reset();
	Order.Reset();
	Source.Reset();

	Mode = InMode;
	TemplateLabel = FName(*Config.Template->Path);
	Source = InScope->FindList(Config.SourcePath, Config.KeyField);
	if (!Source.IsValid())
	{
		UE_LOG(LogSlaterList, Warning, TEXT("SlaterUI: <repeat source=\"{%s}\"> found no list in the data"), *Config.SourcePath);
		return;
	}
	const int32 Count = Source->Num();
	Items.Reserve(Count);
	for (int32 I = 0; I < Count; ++I)
	{
		const FString Key = Source->KeyAt(I);
		if (Key.IsEmpty())
		{
			continue;
		}
		if (Items.Contains(Key))
		{
			UE_LOG(LogSlaterList, Warning, TEXT("SlaterUI: <repeat source=\"{%s}\"> has two items with the key '%s'; the second is ignored"), *Config.SourcePath, *Key);
			continue;
		}
		AddItem(Source->ItemAt(I), Key);
	}
	Source->OnChanged.AddSP(StaticCastSharedRef<SSlaterRepeat>(AsShared()), &SSlaterRepeat::HandleListChanged);
}

TSharedPtr<SWidget> SSlaterRepeat::FindItemWidget(const FString& Key) const
{
	const FItem* Item = Items.Find(Key);
	return Item ? Item->Built.Root : nullptr;
}

void SSlaterRepeat::Place(const FString& Key, int32 Ordinal)
{
	FItem* Item = Items.Find(Key);
	if (!Item || !Item->Built.Root.IsValid())
	{
		return;
	}
	int32 Cell = Ordinal;
	FString Text;
	if (!Config.AtField.IsEmpty() && Item->Scope->Read(Config.AtField, Text))
	{
		Cell = FCString::Atoi(*Text);
	}
	Grid->AddSlot(Cell % Config.Columns, Cell / Config.Columns)
	[
		Item->Built.Root.ToSharedRef()
	];
}

void SSlaterRepeat::AddItem(const TSharedPtr<ISlaterScope>& ItemScope, const FString& Key)
{
	if (!ItemScope.IsValid())
	{
		return;
	}
	if (Items.Contains(Key))
	{
		UE_LOG(LogSlaterList, Warning, TEXT("SlaterUI: <repeat source=\"{%s}\"> has two items with the key '%s'; the second is ignored"), *Config.SourcePath, *Key);
		return;
	}
	FItem Item;
	Item.Scope = ItemScope;
	if (!Slater::InstantiateTemplate(*Config.Template, *Config.Style, *Config.Registry, Item.Built))
	{
		UE_LOG(LogSlaterList, Warning, TEXT("SlaterUI: <repeat> could not build the item '%s'"), *Key);
		return;
	}
	++GStats.ItemsBuilt;
	FItem& Stored = Items.Add(Key, MoveTemp(Item));
	Connect(Key, Stored);
	if (Config.AtField.IsEmpty())
	{
		Order.Add(Key);
	}
	// With a position field the ordinal is only the cell of an item whose field is missing.
	Place(Key, Config.AtField.IsEmpty() ? Order.Num() - 1 : Items.Num() - 1);
}

void SSlaterRepeat::RemoveItemByKey(const FString& Key)
{
	FItem* Item = Items.Find(Key);
	if (!Item)
	{
		return;
	}
	if (Item->Built.Root.IsValid())
	{
		Grid->RemoveSlot(Item->Built.Root.ToSharedRef());
	}
	Items.Remove(Key);
	++GStats.ItemsDestroyed;

	// With a position field nothing else moves and `Order` is not kept.
	if (!Config.AtField.IsEmpty())
	{
		return;
	}
	const int32 Index = Order.IndexOfByKey(Key);
	if (Index == INDEX_NONE)
	{
		return;
	}
	Order.RemoveAt(Index);
	// Without a position field the array order is the layout, so the items after the gap move up (the widgets are kept).
	for (int32 I = Index; I < Order.Num(); ++I)
	{
		if (const FItem* Moved = Items.Find(Order[I]))
		{
			Grid->RemoveSlot(Moved->Built.Root.ToSharedRef());
			Place(Order[I], I);
		}
	}
}

void SSlaterRepeat::Connect(const FString& Key, FItem& Item)
{
	Item.Bindings = MakeUnique<FSlaterBindingSet>(Item.Scope.ToSharedRef(), Mode, *Config.Style, Item.Built.Bindings.Num());
	Item.Bindings->SetLabel(TemplateLabel);
	for (const FSlaterBoundAttr& Bound : Item.Built.Bindings)
	{
		Item.Bindings->Add(Bound);
	}
	for (const FSlaterBoundEvent& Event : Item.Built.Events)
	{
		Item.Bindings->AddEvent(Event);
	}
	if (!Config.AtField.IsEmpty() && Mode == ESlaterBindMode::Pushed)
	{
		// The item moves to the cell its `at` field names, and keeps its widget.
		Item.AtSubscription = Item.Scope->Subscribe(Config.AtField, [this, Key]()
		{
			FItem* Moving = Items.Find(Key);
			if (Moving && Moving->Built.Root.IsValid())
			{
				Grid->RemoveSlot(Moving->Built.Root.ToSharedRef());
				Place(Key, Items.Num() - 1);
			}
		});
	}
}

void SSlaterRepeat::HandleListChanged(const FString& Key, bool bAdded)
{
	if (!bAdded)
	{
		RemoveItemByKey(Key);
		return;
	}
	if (Source.IsValid())
	{
		AddItem(Source->FindItem(Key), Key);
	}
}

// ---- FSlaterBinder ----

FSlaterBinder::FSlaterBinder(TSharedRef<ISlaterScope> InScope, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle)
	: Scope(MoveTemp(InScope))
	, Mode(InMode)
	, Style(&InStyle)
{
}

FSlaterBinder::FSlaterBinder(TSharedRef<FSlaterModel> InModel, ESlaterBindMode InMode, const ISlaterStyleSource& InStyle)
	: FSlaterBinder(Slater::MakeModelScope(InModel), InMode, InStyle)
{
}

FSlaterBinder::~FSlaterBinder() = default;

void FSlaterBinder::Attach(const FSlaterBuildResult& Result, const FString& Label)
{
	Bindings = MakeUnique<FSlaterBindingSet>(Scope, Mode, *Style, Result.Bindings.Num());
	Bindings->SetLabel(FName(*Label));
	for (const FSlaterBoundAttr& Bound : Result.Bindings)
	{
		Bindings->Add(Bound);
	}
	for (const FSlaterBoundEvent& Event : Result.Events)
	{
		Bindings->AddEvent(Event);
	}
	for (const TWeakPtr<SWidget>& Weak : Result.Repeats)
	{
		if (const TSharedPtr<SWidget> Widget = Weak.Pin())
		{
			StaticCastSharedRef<SSlaterRepeat>(Widget.ToSharedRef())->Attach(Scope, Mode);
		}
	}
}
