// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterMarkup.h"
#include "SlaterState.h"
#include "SlaterStyle.h"
#include "Layout/Margin.h"
#include "Types/SlateEnums.h"

class SWidget;
struct FSlaterValue;
class FSlaterCompiledTemplate;
class FSlaterElementRegistry;
class SSlaterOutlet;

/** One thing wrong with the text content of an element, placed inside that text (1-based line and column of the text itself). */
struct FSlaterTextProblem
{
	int32 Line = 1;
	int32 Column = 1;
	FString Message;
};

/** What an attribute holds. Typed by the element, never by a sigil: `padding="UI.Space.M"` and `padding="8"` are both metrics. */
enum class ESlaterAttrType : uint8
{
	/** A number or a metric token. */
	Metric,
	/** `#RRGGBB`, `#RRGGBBAA` or a color token. */
	Color,
	/** A font token. */
	Font,
	/** A surface role token (`UI.Surface.Button`). */
	Surface,
	/** One of a fixed list of words. */
	Enum,
	/** `true` or `false`. */
	Bool,
	Int,
	/** Any text. */
	Text,
};

SLATERUI_API const TCHAR* LexToString(ESlaterAttrType Type);

/** An attribute after it was checked: the value in its own type. Only the member for Type is meaningful. */
struct FSlaterValue
{
	ESlaterAttrType Type = ESlaterAttrType::Text;
	float Number = 0.f;
	int32 Integer = 0;
	bool bBool = false;
	FLinearColor Color = FLinearColor::Transparent;
	FSlaterFont Font;
	FSlaterSurface Surface;
	/** Enum word, or the token name for a Metric/Color/Font/Surface written as a token (None for a literal). */
	FName Word;
	FString Text;
};

/**
 * How a bound attribute reaches its widget. `Apply` sets the value once (pushed binding); `ApplyPolled` hands the widget a
 * getter it evaluates every frame as a Slate attribute (the polled variant, kept only to compare cost). An attribute
 * without `Apply` cannot be written `{Path}`.
 */
struct FSlaterBindTarget
{
	/** True when the value belongs to the outermost widget of the element (its sizing wrapper, if it has one), as `visible` and `enabled` do. */
	bool bOutermost = false;
	TFunction<void(SWidget&, const FSlaterValue&, const ISlaterStyleSource&)> Apply;
	TFunction<void(SWidget&, TFunction<FSlaterValue()>, const ISlaterStyleSource&)> ApplyPolled;

	bool IsSet() const { return Apply && ApplyPolled; }
};

struct FSlaterAttrDesc
{
	FName Name;
	ESlaterAttrType Type = ESlaterAttrType::Text;
	/** For Enum: the allowed words. */
	TArray<FName> EnumValues;
	bool bRequired = false;
	/** Set for an attribute that may be written `{Path}` (a read-only binding). */
	FSlaterBindTarget Bind;
	/** Optional, for a Text attribute: returns what is wrong with a written value (shown at the attribute), or empty when it is fine. */
	TFunction<FString(const FString&)> CheckText;
	/**
	 * An element event (T-381): what happens when the user changes the control, `on-change="{Volume}"` (write the value to
	 * that path of the data) or `on-change="Audio.SetVolume"` (run a command with it). Always Text; make one with
	 * `Slater::EventAttribute` (SlaterEvents.h). The element's builder fires it through its `FSlaterEventSink`.
	 */
	bool bEvent = false;
};

enum class ESlaterChildRule : uint8
{
	/** No child elements. */
	None,
	/** At most one. */
	One,
	Many,
};

/** Attributes a child may carry that its parent's slot uses. */
enum class ESlaterSlotAttrs : uint16
{
	None = 0,
	Fill = 1 << 0,
	Margin = 1 << 1,
	HAlign = 1 << 2,
	VAlign = 1 << 3,
	All = Fill | Margin | HAlign | VAlign,
	/** `canvas` children (T-529). */
	Anchor = 1 << 4,
	Offset = 1 << 5,
	Size = 1 << 6,
	Align = 1 << 7,
	CanvasAll = Anchor | Offset | Size | Align,
	/** `grid` children (T-529). */
	Row = 1 << 8,
	Column = 1 << 9,
	RowSpan = 1 << 10,
	ColumnSpan = 1 << 11,
	GridAll = Row | Column | RowSpan | ColumnSpan,
};
ENUM_CLASS_FLAGS(ESlaterSlotAttrs);

/** The slot attributes of one child, for the parent's builder to apply. Unset means "leave Slate's default". */
struct FSlaterSlotValues
{
	/** Weight along the main axis; unset means auto size. */
	TOptional<float> Fill;
	float Margin = 0.f;
	TOptional<EHorizontalAlignment> HAlign;
	TOptional<EVerticalAlignment> VAlign;
	/** `canvas` slot (T-529): the anchor word (`top-left`, `fill`, ...); unset means top-left. */
	TOptional<FName> Anchor;
	/** `offset="x, y"`: the position from the anchor on a point axis, the inset from both edges on a stretched one. */
	FVector2D Offset = FVector2D::ZeroVector;
	/** `size="w, h"`; unset means the child's own size. */
	TOptional<FVector2D> Size;
	/** `align="x, y"`: which point of the child sits on the anchor, 0 to 1; unset means the anchor's own point. */
	TOptional<FVector2D> Align;
	/** `grid` slot (T-529). */
	int32 Row = 0;
	int32 Column = 0;
	int32 RowSpan = 1;
	int32 ColumnSpan = 1;
};

struct FSlaterBuiltChild
{
	TSharedRef<SWidget> Widget;
	FSlaterSlotValues Slot;
	const FSlaterNode* Node = nullptr;
};

/** What a builder gets: the element's checked attributes and its already built children. */
class SLATERUI_API FSlaterBuildContext
{
public:
	FSlaterBuildContext(const FSlaterNode& InNode, const TMap<FName, FSlaterValue>& InValues) : Node(InNode), Values(InValues) {}

	const FSlaterNode& Node;
	TArray<FSlaterBuiltChild> Children;
	/** The checked attributes. Owned by the caller (a compiled template shares one map between all its instances). */
	const TMap<FName, FSlaterValue>& Values;
	/** The style the layout is built against, for builders that look up more tokens than their attributes name (a surface state variant, the default text color). Set by `BuildLayout`. */
	const ISlaterStyleSource* Style = nullptr;
	/** The registry the layout is built against; elements that build their children later (`repeat`) keep it. Set by `BuildLayout`. */
	const FSlaterElementRegistry* Registry = nullptr;
	/** For an element with `bTemplateChildren`: its child, checked and resolved once, ready to instantiate per item. Null if the element has no valid single child. */
	TSharedPtr<const FSlaterCompiledTemplate> Template;
	/** For an element with `TemplateSlots`: the template of each slot that was written, by slot name. Instantiate with `Slater::InstantiateTemplate`. */
	TMap<FName, TSharedPtr<const FSlaterCompiledTemplate>> SlotTemplates;
	/** A builder that returns null can say why here; it becomes the error text. */
	FString Failure;

	bool Has(FName Name) const { return Values.Contains(Name); }
	float GetMetric(FName Name, float Default = 0.f) const;
	FLinearColor GetColor(FName Name, const FLinearColor& Default = FLinearColor::White) const;
	FSlaterFont GetFont(FName Name) const;
	FSlaterSurface GetSurface(FName Name) const;
	bool GetBool(FName Name, bool Default = false) const;
	int32 GetInt(FName Name, int32 Default = 0) const;
	FName GetEnum(FName Name, FName Default = NAME_None) const;
	FString GetText(FName Name) const;
};

/**
 * A named template an element takes as a child element: `<inventory-grid><cell>...</cell><item>...</item></inventory-grid>`
 * has the slots `cell` and `item`. The slot element holds exactly one child, the template.
 */
struct FSlaterTemplateSlotDesc
{
	FName Name;
	bool bRequired = false;
};

/** Returns the element's own widget (without the common wrapper), or null to report that it could not be built. */
using FSlaterBuilder = TFunction<TSharedPtr<SWidget>(FSlaterBuildContext&)>;

struct FSlaterElementDesc
{
	/** Lowercase words with hyphens, as written in markup. */
	FName Tag;
	/** A few plain words on what the element is for; the unknown-element error lists them so a wrong guess (`label`) points at the right tag (`text`). */
	const TCHAR* Purpose = TEXT("");
	/** The element's own attributes. `id`, the size attributes and `padding` are common and not listed here. */
	TArray<FSlaterAttrDesc> Attributes;
	ESlaterChildRule Children = ESlaterChildRule::None;
	/** Which slot attributes the element's children may use. */
	ESlaterSlotAttrs SlotAttrs = ESlaterSlotAttrs::None;
	/** True when the element accepts character data (`FSlaterNode::Text`). */
	bool bAllowsText = false;
	/**
	 * True when the builder applied `width height min-* max-* padding` itself (an `SBox` element does), so the common
	 * wrapper is not added around it. Use `ApplySizing`.
	 */
	bool bAppliesSizing = false;
	/**
	 * True when the child is a template the element instantiates itself (`repeat`): it is checked once at build time, not
	 * added to the tree, and its bindings do not belong to the layout. Inside a template: no `id`, no further template.
	 */
	bool bTemplateChildren = false;
	/**
	 * Template slots (T-346), for a host that draws items itself, such as `<inventory-grid>`: its children are exactly
	 * these elements, each at most once and holding one child element. Each is checked and resolved once like a `repeat`
	 * child (no `id`, no `repeat` or slot element inside) and handed to the builder in `SlotTemplates`; none of them is
	 * added to the tree. The element must have `Children` set to `None`. The host connects the data itself: it
	 * instantiates a template per item and binds the instance with `Slater::Bind`.
	 */
	TArray<FSlaterTemplateSlotDesc> TemplateSlots;
	/**
	 * Optional. Called once per element after its attributes were checked and before the builder, with the typed values
	 * the builder will read, to add or change values that depend on the style (the default font, the state variant of a
	 * surface). For a template this runs at compile time, so an instance does no token lookups.
	 */
	TFunction<void(const ISlaterStyleSource&, TMap<FName, FSlaterValue>&)> ResolveDefaults;
	/** Optional, for an element with `bAllowsText`: checks literal text content (not a `{Path}`) and adds a problem for each fault, each reported at its own line and column in the file. */
	TFunction<void(const FString&, const ISlaterStyleSource&, TArray<FSlaterTextProblem>&)> CheckContent;
	/** For an element that takes bound text content (`<text>{Count}</text>`). */
	FSlaterBindTarget TextBind;
	FSlaterBuilder Builder;
};

class SBox;

namespace Slater
{
	/** Sets the common size attributes and `padding` of the context on a box. For elements with `bAppliesSizing`. */
	SLATERUI_API void ApplySizing(const FSlaterBuildContext& Context, SBox& Box);
}

/**
 * Elements by tag. The game and other plugins register their own the same way as the stock ones. Descriptors drive both
 * validation and building, so an element cannot be built from an attribute it did not declare.
 */
class SLATERUI_API FSlaterElementRegistry
{
public:
	/** False, with a reason in OutError, for a bad tag, a duplicate tag, no builder, or an attribute that clashes with a common one. */
	bool Register(FSlaterElementDesc Desc, FString* OutError = nullptr);
	bool Unregister(FName Tag);
	const FSlaterElementDesc* Find(FName Tag) const;
	void GetTags(TArray<FString>& OutTags) const;

private:
	TMap<FName, FSlaterElementDesc> Elements;
};

namespace Slater
{
	/** Adds `vbox hbox overlay box scroll uniform-grid wrap spacer` to a registry. */
	SLATERUI_API void RegisterLayoutElements(FSlaterElementRegistry& Registry);

	/** Adds `surface` and `text` to a registry. */
	SLATERUI_API void RegisterContentElements(FSlaterElementRegistry& Registry);
	/** `canvas`, `grid` and `safe-zone` (T-529). */
	SLATERUI_API void RegisterPanelElements(FSlaterElementRegistry& Registry);

	/** Adds `repeat` and `image` (SlaterList.h). */
	SLATERUI_API void RegisterListElements(FSlaterElementRegistry& Registry);

	/** Adds the controls `slider`, `checkbox` and `textbox`, whose events write back (T-381, SlaterEvents.h). */
	SLATERUI_API void RegisterControlElements(FSlaterElementRegistry& Registry);

	/** Adds `popup` (SlaterPopup.h, T-439). */
	SLATERUI_API void RegisterPopupElements(FSlaterElementRegistry& Registry);

	/** Adds `rich-text` (SlaterRichText.h, T-527). */
	SLATERUI_API void RegisterRichTextElements(FSlaterElementRegistry& Registry);

	/** The process-wide registry, with the stock elements already in it. Game thread only. */
	SLATERUI_API FSlaterElementRegistry& Elements();

	/** Shows the widget (as it was built) or collapses it, the way `visible` and `visible-when` do. */
	SLATERUI_API void SetShown(SWidget& Widget, bool bShown);

	/** Attributes every element takes: `id width height min-width min-height max-width max-height padding visible enabled visible-when width-by height-by clip`. `visible` and `enabled` may be bound; `visible-when="key=value"` follows a view's `<state>` (T-353); `width-by` / `height-by` size the element by a state (T-405); `clip="true"` cuts what overflows it. */
	SLATERUI_API const TArray<FSlaterAttrDesc>& CommonAttributes();

	/** Attributes a child can carry for its parent's slot: `fill margin halign valign`. */
	SLATERUI_API const TArray<FSlaterAttrDesc>& SlotAttributes();

	/**
	 * The text of a layout literal (T-535). With an empty `Loc` it is `FText::FromString(Literal)`. `Loc` is the element's `loc` attribute,
	 * `Key` or `Namespace/Key` (namespace `Slater` when there is none); the text then carries that namespace and key (`Key` plus `Suffix`,
	 * for example `.hint`) with the literal as its source string, so the engine's localization lookup can replace it for the active culture.
	 * A translation whose source string differs from the literal is ignored, as for any `FText`.
	 */
	SLATERUI_API FText MakeLiteralText(const FString& Literal, const FString& Loc, const TCHAR* Suffix = TEXT(""));
}

/** Above this a layout gets a warning, not an error. Guesses until T-349 and T-351 show real sizes. */
struct FSlaterBudget
{
	/** Slate widgets in the built tree, wrappers included. */
	int32 MaxWidgets = 400;
	/** Levels of widgets, wrappers included. */
	int32 MaxDepth = 40;
};

/** What a `{Path}` binding says, apart from the widget it landed on. Shared by every instance of a template. */
struct FSlaterBindingSpec
{
	/** The attribute name, or `text` for bound text content. */
	FName Attr;
	/** The path inside the braces (`Count`, `Player.Name`). */
	FString Path;
	FSlaterAttrDesc Desc;
	FSlaterPos Pos;
};

/** One `{Path}` binding found while building: which widget, and what the binding says. */
struct FSlaterBoundAttr
{
	TWeakPtr<SWidget> Widget;
	TSharedPtr<const FSlaterBindingSpec> Spec;
};

/** What an element event says (T-381): write the control's value to a path, or run a command with it. Shared by every instance of a template. */
struct FSlaterEventSpec
{
	/** The attribute (`on-change`, `on-commit`). */
	FName Event;
	/** Set for a write-back: the path inside the braces (`Volume`, `Player.Name`). */
	FString Path;
	/** Set for a command: its name and the one word after a colon. */
	FName Command;
	FString Argument;
	FSlaterPos Pos;

	bool IsWrite() const { return !Path.IsEmpty(); }
};

/** One write-back event found while building: the control's widget (it carries the `FSlaterEventSink`) and what it writes. */
struct FSlaterBoundEvent
{
	TWeakPtr<SWidget> Widget;
	TSharedPtr<const FSlaterEventSpec> Spec;
};

/** A command an `on-click` or element event names (T-566), for a view to check against its tables. Stock commands are included. */
struct FSlaterCommandRef
{
	FName Name;
	FSlaterPos Pos;
};

struct SLATERUI_API FSlaterBuildResult
{
	/** Null when there are errors. */
	TSharedPtr<SWidget> Root;
	TArray<FSlaterError> Errors;
	/** Budget overruns. The layout still builds. */
	TArray<FSlaterError> Warnings;
	/** Measured on the built tree (zero after an error), wrappers included. */
	int32 WidgetCount = 0;
	int32 WidgetDepth = 0;
	/** Elements that carried an `id`. */
	TMap<FName, TWeakPtr<SWidget>> ById;
	/** `{Path}` bindings of the layout itself (not of the templates it repeats). Connect them with `Slater::Bind` (SlaterList.h). */
	TArray<FSlaterBoundAttr> Bindings;
	/** Events of the layout itself that write to a `{Path}` (T-381); `Slater::Bind` gives each control its writer. Command events need no binding and are not listed. */
	TArray<FSlaterBoundEvent> Events;
	/** Elements with `bTemplateChildren` that were built, for `Slater::Bind` to connect. */
	TArray<TWeakPtr<SWidget>> Repeats;
	/** The `<outlet>`s of the layout by their id (a view fills them with `SetOutlet`). */
	TMap<FName, TWeakPtr<SSlaterOutlet>> Outlets;
	/** The `<state>`s the layout declares (T-353), in file order. A view keeps their values. */
	TArray<FSlaterStateDecl> States;
	/** Elements with `visible-when`, for the view to show or collapse when a state changes. */
	TArray<FSlaterStateCondition> StateConditions;
	/** Elements with `width-by` / `height-by` (T-405), for the view to resize when a state changes. */
	TArray<FSlaterStateSize> StateSizes;
	/** Every command the layout names (`on-click`, `on-change`, ...), templates and repeats included, in file order (T-566). */
	TArray<FSlaterCommandRef> Commands;

	bool Succeeded() const { return Errors.Num() == 0 && Root.IsValid(); }
};

/**
 * A template checked once: element descriptors, typed attribute values, slot values, binding specs and wrapper needs
 * are resolved, so an instance only runs the builders. It reflects the style and registry at compile time; compile
 * again after either changes. Owns a copy of the node it was made from. Made by `Slater::CompileTemplate`; the members
 * are internal.
 */
class SLATERUI_API FSlaterCompiledTemplate
{
public:
	struct FNode;
	FSlaterCompiledTemplate();
	~FSlaterCompiledTemplate();

	FSlaterNode Root;
	TSharedPtr<FNode> RootNode;
	FString Path;
};

namespace Slater
{
	/**
	 * Checks a parsed layout against the registry and the style, and builds it. Every problem is reported, not only the
	 * first, in `Path(Line,Column): message` form, with the nearest valid name suggested for a mistyped tag, attribute,
	 * enum word or token. Nothing is returned built unless there are no errors. The subtree of an unknown tag is not checked.
	 */
	SLATERUI_API bool BuildLayout(const FSlaterNode& Root, const FString& Path, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget = FSlaterBudget(), const FSlaterElementRegistry& Registry = Elements());

	/** Adds the note that a syntax error stops the build, so errors in the rest of the file only show once it is fixed. */
	SLATERUI_API void AppendSyntaxStopsBuildNote(FSlaterError& Error);

	/** `ParseMarkup`, then `BuildLayout`. A syntax error lands in Errors. */
	SLATERUI_API bool BuildLayoutFromText(const FString& Text, const FString& Path, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget = FSlaterBudget(), const FSlaterElementRegistry& Registry = Elements());

	/**
	 * Checks a node as a template and resolves it. False with the errors in OutErrors when it does not fit the registry
	 * and style (a layout build reports the same errors, so for a `repeat` this only fails if either changed).
	 */
	SLATERUI_API TSharedPtr<const FSlaterCompiledTemplate> CompileTemplate(const FSlaterNode& Node, const FString& Path, const ISlaterStyleSource& Style, const FSlaterElementRegistry& Registry, TArray<FSlaterError>& OutErrors);

	/** Builds one instance of a compiled template into Out (Root, Bindings; no budget, no measuring). False if a builder fails. Style and Registry are the ones it was compiled against. */
	SLATERUI_API bool InstantiateTemplate(const FSlaterCompiledTemplate& Template, const ISlaterStyleSource& Style, const FSlaterElementRegistry& Registry, FSlaterBuildResult& Out);

	/** The binding specs of every element of a compiled template (an item's bindings), for checking them against data before an item exists. */
	SLATERUI_API void GetTemplateBindings(const FSlaterCompiledTemplate& Template, TArray<TSharedRef<const FSlaterBindingSpec>>& OutSpecs);

	/** The write-back events of every element of a compiled template, for checking their paths against an item. */
	SLATERUI_API void GetTemplateEvents(const FSlaterCompiledTemplate& Template, TArray<TSharedRef<const FSlaterEventSpec>>& OutSpecs);

	/** True for `{Path}` (letters, digits, underscore and dots between the braces, nothing else); OutPath gets the path. */
	SLATERUI_API bool IsBindingExpr(const FString& Text, FString* OutPath = nullptr);

	/** Reads text into a value of the attribute's type, the way a layout attribute is read; false when it does not fit. Used for bound values at run time. */
	SLATERUI_API bool ParseBoundValue(const FSlaterAttrDesc& Desc, const FString& Text, const ISlaterStyleSource& Style, FSlaterValue& OutValue);

	/** Widgets in the tree under Root, itself included, and the number of levels. */
	SLATERUI_API void MeasureWidgetTree(const TSharedRef<SWidget>& Root, int32& OutCount, int32& OutDepth);
}
