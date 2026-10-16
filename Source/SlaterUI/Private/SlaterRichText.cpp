// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterRichText.h"

#include "SlaterElements.h"
#include "SlaterList.h"

#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateImageRun.h"
#include "Framework/Text/SlateTextRun.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Text/SRichTextBlock.h"

/** What the decorators need from the style, copied when the text is set so they never reach back into a style that moved on. */
struct FSlaterRichSnapshot
{
	FTextBlockStyle Base;
	int32 Size = 12;
	TMap<FString, FLinearColor> Colors;
	TMap<FString, FSlaterFont> Fonts;
};

namespace
{
	/** One run of rich text as written. Offsets are into the text. */
	struct FRichTag
	{
		FString Name;
		TMap<FString, FString> Attributes;
		int32 Begin = 0;
		bool bSelfClosing = false;
	};

	bool IsWordChar(TCHAR C)
	{
		return FChar::IsAlnum(C) || C == TEXT('_');
	}

	bool IsNameChar(TCHAR C)
	{
		return IsWordChar(C) || C == TEXT('.') || C == TEXT('-');
	}

	void LineAndColumn(const FString& Text, int32 Index, int32& OutLine, int32& OutColumn)
	{
		OutLine = 1;
		OutColumn = 1;
		for (int32 I = 0; I < Index; ++I)
		{
			if (Text[I] == TEXT('\n'))
			{
				++OutLine;
				OutColumn = 1;
			}
			else
			{
				++OutColumn;
			}
		}
	}

	/**
	 * Reads the tags the way the engine's parser does (`<name a="b">content</>` or `<name a="b"/>`, the content on one
	 * line), and reports what the engine would silently show as plain text: a tag that never closes, a stray `</>`.
	 */
	void ScanTags(const FString& Text, TArray<FRichTag>& OutTags, TArray<FSlaterTextProblem>& OutProblems)
	{
		const int32 Num = Text.Len();
		auto Problem = [&](int32 Index, const FString& Message)
		{
			FSlaterTextProblem& Found = OutProblems.AddDefaulted_GetRef();
			LineAndColumn(Text, Index, Found.Line, Found.Column);
			Found.Message = Message;
		};

		int32 I = 0;
		while (I < Num)
		{
			if (Text[I] != TEXT('<'))
			{
				++I;
				continue;
			}
			const int32 Begin = I;
			if (I + 1 < Num && Text[I + 1] == TEXT('/'))
			{
				Problem(Begin, Text.Mid(I, 3) == TEXT("</>") ? TEXT("'</>' closes nothing: there is no open tag before it on this line")
					: TEXT("a run is closed with '</>', not with the tag name"));
				I += 2;
				continue;
			}
			if (I + 1 >= Num || !IsNameChar(Text[I + 1]))
			{
				++I; // a '<' that is only a less-than sign
				continue;
			}

			FRichTag Tag;
			Tag.Begin = Begin;
			int32 J = I + 1;
			while (J < Num && IsNameChar(Text[J]))
			{
				Tag.Name.AppendChar(Text[J++]);
			}
			bool bBad = false;
			while (J < Num && Text[J] == TEXT(' '))
			{
				++J;
				FString Key, Value;
				while (J < Num && IsNameChar(Text[J]))
				{
					Key.AppendChar(Text[J++]);
				}
				if (Key.IsEmpty() || J + 1 >= Num || Text[J] != TEXT('=') || Text[J + 1] != TEXT('"'))
				{
					bBad = true;
					break;
				}
				J += 2;
				while (J < Num && Text[J] != TEXT('"') && Text[J] != TEXT('\n'))
				{
					Value.AppendChar(Text[J++]);
				}
				if (J >= Num || Text[J] != TEXT('"'))
				{
					bBad = true;
					break;
				}
				++J;
				Tag.Attributes.Add(Key, Value);
			}
			if (!bBad && Text.Mid(J, 2) == TEXT("/>"))
			{
				Tag.bSelfClosing = true;
				J += 2;
			}
			else if (!bBad && J < Num && Text[J] == TEXT('>'))
			{
				++J;
				int32 Close = INDEX_NONE;
				for (int32 K = J; K + 2 < Num && Text[K] != TEXT('\n'); ++K)
				{
					if (Text[K] == TEXT('<') && Text[K + 1] == TEXT('/') && Text[K + 2] == TEXT('>'))
					{
						Close = K;
						break;
					}
				}
				if (Close == INDEX_NONE)
				{
					Problem(Begin, FString::Printf(TEXT("<%s> is never closed: end the run with '</>' on the same line"), *Tag.Name));
					I = J;
					continue;
				}
				for (int32 K = J; K < Close; ++K)
				{
					if (Text[K] == TEXT('<') && K + 1 < Close && IsNameChar(Text[K + 1]))
					{
						Problem(K, TEXT("rich-text tags do not nest: close the first run before opening another"));
						break;
					}
				}
				J = Close + 3;
			}
			else
			{
				bBad = true;
			}
			if (bBad)
			{
				Problem(Begin, TEXT("this '<' starts a tag that is not valid: a tag is <name key=\"value\">text</> or <name key=\"value\"/>; to show a '<' write &amp;lt; in the layout"));
				++I;
				continue;
			}
			OutTags.Add(MoveTemp(Tag));
			I = J;
		}
	}

	void CheckTag(const FString& Text, const FRichTag& Tag, const ISlaterStyleSource& Style, TArray<FSlaterTextProblem>& OutProblems)
	{
		auto Problem = [&](const FString& Message)
		{
			FSlaterTextProblem& Found = OutProblems.AddDefaulted_GetRef();
			LineAndColumn(Text, Tag.Begin, Found.Line, Found.Column);
			Found.Message = Message;
		};
		auto Only = [&](std::initializer_list<const TCHAR*> Allowed)
		{
			for (const TPair<FString, FString>& Pair : Tag.Attributes)
			{
				bool bFound = false;
				for (const TCHAR* Name : Allowed)
				{
					bFound |= Pair.Key == Name;
				}
				if (!bFound)
				{
					Problem(FString::Printf(TEXT("<%s> has no attribute '%s'"), *Tag.Name, *Pair.Key));
				}
			}
		};

		if (Tag.Name == TEXT("b") || Tag.Name == TEXT("i"))
		{
			Only({});
			if (Tag.bSelfClosing)
			{
				Problem(FString::Printf(TEXT("<%s/> holds nothing: write <%s>words</>"), *Tag.Name, *Tag.Name));
			}
		}
		else if (Tag.Name == TEXT("font") || Tag.Name == TEXT("color"))
		{
			Only({TEXT("value")});
			const bool bFont = Tag.Name == TEXT("font");
			const FString* Value = Tag.Attributes.Find(TEXT("value"));
			if (Tag.bSelfClosing)
			{
				Problem(FString::Printf(TEXT("<%s/> holds nothing: write <%s value=\"...\">words</>"), *Tag.Name, *Tag.Name));
			}
			if (!Value)
			{
				Problem(FString::Printf(TEXT("<%s> needs value=\"%s\""), *Tag.Name, bFont ? TEXT("UI.Font.Heading") : TEXT("UI.Color.Accent")));
			}
			else
			{
				FSlaterFont Font;
				FLinearColor Color;
				const bool bKnown = bFont ? Style.FindFont(FName(**Value), Font) : Style.FindColor(FName(**Value), Color);
				if (!bKnown)
				{
					Problem(FString::Printf(TEXT("unknown %s token '%s'%s"), bFont ? TEXT("font") : TEXT("color"), **Value,
						*Slater::DidYouMean(Style, bFont ? ESlaterTokenKind::Font : ESlaterTokenKind::Color, *Value)));
				}
			}
		}
		else if (Tag.Name == TEXT("img"))
		{
			Only({TEXT("src")});
			if (!Tag.bSelfClosing)
			{
				Problem(TEXT("<img> holds nothing: write <img src=\"Name\"/>"));
			}
			if (!Tag.Attributes.Contains(TEXT("src")))
			{
				Problem(TEXT("<img> needs src=\"Name\", the name of an image"));
			}
		}
		else
		{
			Problem(FString::Printf(TEXT("unknown rich-text tag <%s>; the tags are b, i, font, color and img"), *Tag.Name));
		}
	}

	/** The decorator for all five tags. Reads only the snapshot, which the widget refreshes before each text. */
	class FSlaterRichDecorator : public ITextDecorator
	{
	public:
		explicit FSlaterRichDecorator(TSharedRef<FSlaterRichSnapshot> InSnapshot) : Snapshot(MoveTemp(InSnapshot)) {}

		virtual bool Supports(const FTextRunParseResults& RunInfo, const FString& Text) const override
		{
			return RunInfo.Name == TEXT("b") || RunInfo.Name == TEXT("i") || RunInfo.Name == TEXT("font") || RunInfo.Name == TEXT("color") || RunInfo.Name == TEXT("img");
		}

		virtual TSharedRef<ISlateRun> Create(const TSharedRef<FTextLayout>& TextLayout, const FTextRunParseResults& RunInfo, const FString& OriginalText,
			const TSharedRef<FString>& InOutModelText, const ISlateStyle* Style) override
		{
			FRunInfo Info(RunInfo.Name);
			TMap<FString, FString> Attributes;
			for (const TPair<FString, FTextRange>& Pair : RunInfo.MetaData)
			{
				const FString Value = OriginalText.Mid(Pair.Value.BeginIndex, Pair.Value.EndIndex - Pair.Value.BeginIndex);
				Info.MetaData.Add(Pair.Key, Value);
				Attributes.Add(Pair.Key, Value);
			}

			FTextRange ModelRange;
			ModelRange.BeginIndex = InOutModelText->Len();
			if (RunInfo.Name == TEXT("img"))
			{
				*InOutModelText += TEXT('\x200B'); // the zero-width space the engine's image decorator uses
				ModelRange.EndIndex = InOutModelText->Len();
				const FString* Src = Attributes.Find(TEXT("src"));
				const FSlateBrush* Brush = Src ? Slater::FindImage(FName(**Src, FNAME_Find)) : nullptr;
				return FSlateImageRun::Create(Info, InOutModelText, Brush ? Brush : FStyleDefaults::GetNoBrush(), 0, ModelRange);
			}

			*InOutModelText += OriginalText.Mid(RunInfo.ContentRange.BeginIndex, RunInfo.ContentRange.EndIndex - RunInfo.ContentRange.BeginIndex);
			ModelRange.EndIndex = InOutModelText->Len();

			FTextBlockStyle RunStyle = Snapshot->Base;
			if (RunInfo.Name == TEXT("b"))
			{
				RunStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Snapshot->Size));
			}
			else if (RunInfo.Name == TEXT("i"))
			{
				RunStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), Snapshot->Size));
			}
			else if (const FString* Value = Attributes.Find(TEXT("value")))
			{
				if (RunInfo.Name == TEXT("font"))
				{
					if (const FSlaterFont* Font = Snapshot->Fonts.Find(*Value))
					{
						RunStyle.SetFont(FCoreStyle::GetDefaultFontStyle(Font->Role, Font->Size));
					}
				}
				else if (const FLinearColor* Color = Snapshot->Colors.Find(*Value))
				{
					RunStyle.SetColorAndOpacity(FSlateColor(*Color));
				}
			}
			return FSlateTextRun::Create(Info, InOutModelText, RunStyle, ModelRange);
		}

	private:
		TSharedRef<FSlaterRichSnapshot> Snapshot;
	};

	FTextBlockStyle MakeBaseStyle(const FSlaterFont& Font, const FLinearColor& Color)
	{
		return FTextBlockStyle()
			.SetFont(FCoreStyle::GetDefaultFontStyle(Font.Role, Font.Size))
			.SetColorAndOpacity(FSlateColor(Color));
	}

	TSharedPtr<SWidget> BuildRichText(FSlaterBuildContext& Context)
	{
		static const FName Left(TEXT("left"));
		static const FName Center(TEXT("center"));
		const FName Justify = Context.GetEnum(TEXT("justify"), Left);
		const bool bBound = Slater::IsBindingExpr(Context.Node.Text);
		TSharedRef<SSlaterRichText> Widget = SNew(SSlaterRichText)
			.Font(Context.GetFont(TEXT("font")))
			.Color(Context.GetColor(TEXT("color")))
			.Wrap(Context.GetBool(TEXT("wrap"), false))
			.Justify(Justify == Center ? ETextJustify::Center : Justify == TEXT("right") ? ETextJustify::Right : ETextJustify::Left);
		if (Context.Style)
		{
			Widget->SetRichText(bBound ? FString() : Context.Node.Text, *Context.Style);
		}
		return Widget;
	}

	void ResolveRichDefaults(const ISlaterStyleSource& Style, TMap<FName, FSlaterValue>& Values)
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

	FSlaterAttrDesc RichAttr(const TCHAR* Name, ESlaterAttrType Type, std::initializer_list<const TCHAR*> Words = {})
	{
		FSlaterAttrDesc Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		for (const TCHAR* Word : Words)
		{
			Desc.EnumValues.Add(Word);
		}
		return Desc;
	}
}

namespace Slater
{
	void CheckRichText(const FString& Text, const ISlaterStyleSource& Style, TArray<FSlaterTextProblem>& OutProblems)
	{
		TArray<FRichTag> Tags;
		ScanTags(Text, Tags, OutProblems);
		for (const FRichTag& Tag : Tags)
		{
			CheckTag(Text, Tag, Style, OutProblems);
		}
		OutProblems.StableSort([](const FSlaterTextProblem& A, const FSlaterTextProblem& B)
		{
			return A.Line != B.Line ? A.Line < B.Line : A.Column < B.Column;
		});
	}

	void RegisterRichTextElements(FSlaterElementRegistry& Registry)
	{
		FSlaterElementDesc Desc;
		Desc.Tag = TEXT("rich-text");
		Desc.Purpose = TEXT("text with bold, italic, colored and sized words and inline images, for descriptions and tooltips");
		Desc.Attributes = {
			RichAttr(TEXT("font"), ESlaterAttrType::Font),
			RichAttr(TEXT("color"), ESlaterAttrType::Color),
			RichAttr(TEXT("wrap"), ESlaterAttrType::Bool),
			RichAttr(TEXT("justify"), ESlaterAttrType::Enum, {TEXT("left"), TEXT("center"), TEXT("right")}),
		};
		Desc.Children = ESlaterChildRule::None;
		Desc.SlotAttrs = ESlaterSlotAttrs::None;
		Desc.bAllowsText = true;
		Desc.TextBind.Apply = [](SWidget& Widget, const FSlaterValue& Value, const ISlaterStyleSource& Style)
		{
			static_cast<SSlaterRichText&>(Widget).SetRichText(Value.Text, Style);
		};
		Desc.TextBind.ApplyPolled = [](SWidget& Widget, TFunction<FSlaterValue()> Get, const ISlaterStyleSource& Style)
		{
			// The decorators read a snapshot, so the polled variant writes once like the meter's.
			static_cast<SSlaterRichText&>(Widget).SetRichText(Get().Text, Style);
		};
		Desc.CheckContent = [](const FString& Text, const ISlaterStyleSource& Style, TArray<FSlaterTextProblem>& OutProblems)
		{
			CheckRichText(Text, Style, OutProblems);
		};
		Desc.ResolveDefaults = &ResolveRichDefaults;
		Desc.Builder = &BuildRichText;
		FString Error;
		const bool bOk = Registry.Register(MoveTemp(Desc), &Error);
		ensureMsgf(bOk, TEXT("SlaterUI stock element <rich-text>: %s"), *Error);
	}
}

SSlaterRichText::SSlaterRichText() = default;
SSlaterRichText::~SSlaterRichText() = default;

void SSlaterRichText::Construct(const FArguments& InArgs)
{
	Snapshot = MakeShared<FSlaterRichSnapshot>();
	Snapshot->Size = InArgs._Font.Size;
	Snapshot->Base = MakeBaseStyle(InArgs._Font, InArgs._Color);
	const TSharedRef<FSlaterRichDecorator> Decorator = MakeShared<FSlaterRichDecorator>(Snapshot.ToSharedRef());
	ChildSlot
	[
		SAssignNew(Block, SRichTextBlock)
		.TextStyle(&Snapshot->Base)
		.DecoratorStyleSet(&FCoreStyle::Get())
		.AutoWrapText(InArgs._Wrap)
		.Justification(InArgs._Justify)
		+ SRichTextBlock::Decorator(Decorator)
	];
}

void SSlaterRichText::SetRichText(const FString& Text, const ISlaterStyleSource& Style)
{
	Source = Text;
	// Resolve the tokens this text names, so the decorators never touch the style.
	Snapshot->Colors.Reset();
	Snapshot->Fonts.Reset();
	TArray<FRichTag> Tags;
	TArray<FSlaterTextProblem> Ignored;
	ScanTags(Text, Tags, Ignored);
	for (const FRichTag& Run : Tags)
	{
		const FString* Value = Run.Attributes.Find(TEXT("value"));
		if (!Value)
		{
			continue;
		}
		if (Run.Name == TEXT("font"))
		{
			FSlaterFont Font;
			if (Style.FindFont(FName(**Value), Font))
			{
				Snapshot->Fonts.Add(*Value, Font);
			}
		}
		else if (Run.Name == TEXT("color"))
		{
			FLinearColor Color;
			if (Style.FindColor(FName(**Value), Color))
			{
				Snapshot->Colors.Add(*Value, Color);
			}
		}
	}
	Block->SetText(FText::FromString(Text));
}
