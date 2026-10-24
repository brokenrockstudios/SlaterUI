// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterMarkup.h"

const FSlaterAttribute* FSlaterNode::FindAttribute(const FString& Name) const
{
	return Attributes.FindByPredicate([&Name](const FSlaterAttribute& Attribute)
	{
		return Attribute.Name.Equals(Name, ESearchCase::CaseSensitive);
	});
}

FString FSlaterError::ToString() const
{
	return FString::Printf(TEXT("%s(%d,%d): %s"), Path.IsEmpty() ? TEXT("<markup>") : *Path, Line, Column, *Message);
}

namespace SlaterMarkupPrivate
{
	bool IsNameStart(TCHAR C)
	{
		return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
	}

	/** Colon is a name character so that `param:Progress` (T-354) is one attribute name. */
	bool IsNameChar(TCHAR C)
	{
		return IsNameStart(C) || (C >= '0' && C <= '9') || C == '-' || C == '.' || C == ':';
	}

	/** Value of a hex digit; the caller has checked it is one. */
	uint32 DigitValue(TCHAR C)
	{
		return C <= '9' ? static_cast<uint32>(C - '0') : static_cast<uint32>(FChar::ToLower(C) - 'a' + 10);
	}

	/** Carriage returns are folded into line feeds before parsing. */
	bool IsSpace(TCHAR C)
	{
		return C == ' ' || C == '\t' || C == '\n';
	}

	/** Characters allowed to appear as themselves: XML's Char production without the carriage return. */
	bool IsAllowedChar(TCHAR C)
	{
		return C >= 0x20 ? (C != 0xFFFE && C != 0xFFFF) : (C == '\t' || C == '\n');
	}

	/** XML's Char production, for numeric references (which may also name a carriage return). */
	bool IsValidCodePoint(uint32 CodePoint)
	{
		return CodePoint == 0x9 || CodePoint == 0xA || CodePoint == 0xD
			|| (CodePoint >= 0x20 && CodePoint <= 0xD7FF)
			|| (CodePoint >= 0xE000 && CodePoint <= 0xFFFD)
			|| (CodePoint >= 0x10000 && CodePoint <= 0x10FFFF);
	}

	/** One pass over a file. Iterative: nesting depth costs heap, never stack, so a cap is only a policy. */
	class FMarkupParser
	{
	public:
		FMarkupParser(const FString& InText, const FString& InPath, const FSlaterLimits& InLimits, FSlaterNode& InRoot, FSlaterError& InError)
			: Source(InText), Path(InPath), Limits(InLimits), Root(InRoot), Error(InError)
		{
			// One line-break convention, so a line is a run of text ended by '\n'.
			Source.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
			Source.ReplaceInline(TEXT("\r"), TEXT("\n"));
			Data = *Source;
			Len = Source.Len();
		}

		bool Run()
		{
			if (Len > 0 && Data[0] == 0xFEFF)
			{
				Idx = 1;
				LineStart = 1;
			}
			const int32 FirstIdx = Idx;

			while (Idx < Len)
			{
				if (Peek() != '<')
				{
					if (!ReadText())
					{
						return false;
					}
				}
				else if (Looking(TEXT("<!--")))
				{
					if (!SkipComment())
					{
						return false;
					}
				}
				else if (Looking(TEXT("<![CDATA[")))
				{
					return Fail(Here(), TEXT("CDATA sections are not supported; write '<' as &lt; and '&' as &amp;"));
				}
				else if (Looking(TEXT("<!")))
				{
					return Fail(Here(), TEXT("DOCTYPE and other <! declarations are not supported"));
				}
				else if (Looking(TEXT("<?")))
				{
					if (!SkipDeclaration(FirstIdx))
					{
						return false;
					}
				}
				else if (Looking(TEXT("</")))
				{
					if (!ReadClosingTag())
					{
						return false;
					}
				}
				else if (!ReadOpeningTag())
				{
					return false;
				}
			}

			if (!Open.IsEmpty())
			{
				const FSlaterNode& Unclosed = *Open.Last();
				return Fail(Unclosed.Pos, FString::Printf(TEXT("<%s> is never closed: the file ends first"), *Unclosed.Tag));
			}
			if (!bRootSeen)
			{
				return Fail(Here(), TEXT("the file has no root element"));
			}
			return true;
		}

	private:
		FSlaterPos Here() const { return {Line, Idx - LineStart + 1}; }
		bool AtEnd() const { return Idx >= Len; }
		TCHAR Peek(int32 Ahead = 0) const { return Idx + Ahead < Len ? Data[Idx + Ahead] : 0; }
		bool Looking(const TCHAR* Prefix) const { return FCString::Strncmp(Data + Idx, Prefix, FCString::Strlen(Prefix)) == 0; }

		void Advance(int32 Count = 1)
		{
			for (; Count > 0 && Idx < Len; --Count)
			{
				if (Data[Idx] == '\n')
				{
					++Line;
					LineStart = Idx + 1;
				}
				++Idx;
			}
		}

		bool SkipSpaces()
		{
			const int32 Start = Idx;
			while (Idx < Len && IsSpace(Data[Idx]))
			{
				Advance();
			}
			return Idx > Start;
		}

		bool Fail(const FSlaterPos& At, const FString& Message)
		{
			Error.Path = Path;
			Error.Line = At.Line;
			Error.Column = At.Column;
			Error.Message = Message;
			return false;
		}

		/** What is at the cursor, for "found ..." in messages. */
		FString Found() const
		{
			if (AtEnd())
			{
				return TEXT("the end of the file");
			}
			if (Peek() == '\n')
			{
				return TEXT("a line break");
			}
			return FString::Printf(TEXT("'%c'"), Peek());
		}

		bool FailControlChar()
		{
			return Fail(Here(), FString::Printf(TEXT("control character U+%04X is not allowed; remove it"), static_cast<uint32>(Peek())));
		}

		bool ReadName(FString& OutName)
		{
			if (!IsNameStart(Peek()))
			{
				return false;
			}
			const int32 Start = Idx;
			while (IsNameChar(Peek()))
			{
				Advance();
			}
			OutName = FString::ConstructFromPtrSize(Data + Start, Idx - Start);
			return true;
		}

		/** At '&'. Appends the decoded character to Out and moves past the ';'. */
		bool ReadReference(FString& Out)
		{
			const FSlaterPos At = Here();
			int32 End = Idx + 1;
			while (End < Len && End - Idx < 12 && (FChar::IsAlnum(Data[End]) || Data[End] == '#'))
			{
				++End;
			}
			if (End >= Len || Data[End] != ';')
			{
				return Fail(At, TEXT("'&' starts an entity such as &amp;; write &amp; for a literal ampersand"));
			}

			const FString Name = FString::ConstructFromPtrSize(Data + Idx + 1, End - Idx - 1);
			uint32 CodePoint = 0;
			if (Name.Equals(TEXT("amp"), ESearchCase::CaseSensitive))
			{
				CodePoint = '&';
			}
			else if (Name.Equals(TEXT("lt"), ESearchCase::CaseSensitive))
			{
				CodePoint = '<';
			}
			else if (Name.Equals(TEXT("gt"), ESearchCase::CaseSensitive))
			{
				CodePoint = '>';
			}
			else if (Name.Equals(TEXT("quot"), ESearchCase::CaseSensitive))
			{
				CodePoint = '"';
			}
			else if (Name.Equals(TEXT("apos"), ESearchCase::CaseSensitive))
			{
				CodePoint = '\'';
			}
			else if (Name.StartsWith(TEXT("#"), ESearchCase::CaseSensitive))
			{
				const bool bHex = Name.Len() > 1 && Name[1] == 'x';
				const int32 DigitsStart = bHex ? 2 : 1;
				if (DigitsStart >= Name.Len())
				{
					return Fail(At, FString::Printf(TEXT("'&%s;' has no digits; write a number such as &#65; or &#x41;"), *Name));
				}
				for (int32 I = DigitsStart; I < Name.Len(); ++I)
				{
					const TCHAR Digit = Name[I];
					if (!(bHex ? FChar::IsHexDigit(Digit) : FChar::IsDigit(Digit)))
					{
						return Fail(At, FString::Printf(TEXT("'&%s;' is not a number; write &#65; or &#x41;"), *Name));
					}
					CodePoint = CodePoint * (bHex ? 16 : 10) + DigitValue(Digit);
					if (CodePoint > 0x10FFFF)
					{
						break;
					}
				}
				if (!IsValidCodePoint(CodePoint))
				{
					return Fail(At, FString::Printf(TEXT("'&%s;' is not a character XML allows (a control character, a surrogate or beyond U+10FFFF)"), *Name));
				}
			}
			else
			{
				return Fail(At, FString::Printf(TEXT("unknown entity '&%s;'; use &amp; &lt; &gt; &quot; &apos; or a number such as &#65;"), *Name));
			}

			if (CodePoint <= 0xFFFF)
			{
				Out.AppendChar(static_cast<TCHAR>(CodePoint));
			}
			else
			{
				// TCHAR is UTF-16, so a character beyond the basic plane is a surrogate pair.
				CodePoint -= 0x10000;
				Out.AppendChar(static_cast<TCHAR>(0xD800 + (CodePoint >> 10)));
				Out.AppendChar(static_cast<TCHAR>(0xDC00 + (CodePoint & 0x3FF)));
			}
			Advance(End + 1 - Idx);
			return true;
		}

		/** Character data up to the next '<'. Belongs to the open element, or must be blank outside the root. */
		bool ReadText()
		{
			FString Run;
			FSlaterPos First;
			bool bHasContent = false;

			while (Idx < Len && Data[Idx] != '<')
			{
				const TCHAR C = Data[Idx];
				if (!IsAllowedChar(C))
				{
					return FailControlChar();
				}
				if (!bHasContent && !IsSpace(C))
				{
					First = Here();
					bHasContent = true;
				}
				if (C == '&')
				{
					if (!ReadReference(Run))
					{
						return false;
					}
				}
				else
				{
					Run.AppendChar(C);
					Advance();
				}
			}

			if (Open.IsEmpty())
			{
				if (bHasContent)
				{
					return Fail(First, bRootSeen
						? TEXT("text after the root element; a file holds one root element and nothing else but comments")
						: TEXT("text before the root element; a file holds one root element and nothing else but comments"));
				}
				return true;
			}

			Run.TrimStartAndEndInline();
			if (!Run.IsEmpty())
			{
				FSlaterNode& Node = *Open.Last();
				if (Node.Text.IsEmpty())
				{
					Node.TextPos = First;
				}
				else
				{
					Node.Text.AppendChar(' ');
				}
				Node.Text += Run;
			}
			return true;
		}

		bool SkipComment()
		{
			const FSlaterPos At = Here();
			const TCHAR* End = FCString::Strstr(Data + Idx + 4, TEXT("-->"));
			if (!End)
			{
				return Fail(At, TEXT("comment is never closed; end it with -->"));
			}
			Advance(UE_PTRDIFF_TO_INT32(End - (Data + Idx)) + 3);
			return true;
		}

		/** `<?xml ... ?>` is accepted as the first thing in the file; any other `<?` is refused. */
		bool SkipDeclaration(int32 FirstIdx)
		{
			const FSlaterPos At = Here();
			if (Idx != FirstIdx || !Looking(TEXT("<?xml")) || !(IsSpace(Peek(5)) || Peek(5) == '?'))
			{
				return Fail(At, Looking(TEXT("<?xml"))
					? TEXT("an <?xml ... ?> declaration is only allowed at the very start of the file")
					: TEXT("processing instructions are not supported"));
			}
			const TCHAR* End = FCString::Strstr(Data + Idx + 2, TEXT("?>"));
			if (!End)
			{
				return Fail(At, TEXT("the <?xml declaration is never closed; end it with ?>"));
			}
			Advance(UE_PTRDIFF_TO_INT32(End - (Data + Idx)) + 2);
			return true;
		}

		bool ReadOpeningTag()
		{
			const FSlaterPos TagPos = Here();
			Advance();

			FString Tag;
			if (!ReadName(Tag))
			{
				return Fail(Here(), FString::Printf(TEXT("expected a tag name after '<', found %s; write a literal '<' as &lt;"), *Found()));
			}
			if (Open.IsEmpty() && bRootSeen)
			{
				return Fail(TagPos, FString::Printf(TEXT("a file holds one root element; <%s> is a second one (<%s> is already closed)"), *Tag, *Root.Tag));
			}
			if (NodeCount >= Limits.MaxNodes)
			{
				return Fail(TagPos, FString::Printf(TEXT("more than %d elements in one file"), Limits.MaxNodes));
			}
			if (Open.Num() >= Limits.MaxDepth)
			{
				return Fail(TagPos, FString::Printf(TEXT("elements nested deeper than %d levels"), Limits.MaxDepth));
			}

			FSlaterNode* Node = &Root;
			if (Open.IsEmpty())
			{
				bRootSeen = true;
			}
			else
			{
				Node = &Open.Last()->Children.AddDefaulted_GetRef();
			}
			Node->Tag = Tag;
			Node->Pos = TagPos;
			++NodeCount;

			while (true)
			{
				const bool bSpaced = SkipSpaces();
				if (AtEnd())
				{
					return Fail(TagPos, FString::Printf(TEXT("<%s> is never finished: the file ends inside the tag"), *Tag));
				}
				const TCHAR C = Peek();
				if (C == '>')
				{
					Advance();
					Open.Add(Node);
					return true;
				}
				if (C == '/')
				{
					if (Peek(1) != '>')
					{
						return Fail(Here(), TEXT("expected '>' right after '/'"));
					}
					Advance(2);
					return true;
				}
				if (!bSpaced)
				{
					return Fail(Here(), FString::Printf(TEXT("expected a space, '>' or '/>' here, found %s"), *Found()));
				}
				if (!ReadAttribute(*Node))
				{
					return false;
				}
			}
		}

		bool ReadAttribute(FSlaterNode& Node)
		{
			const FSlaterPos NamePos = Here();
			FString Name;
			if (!ReadName(Name))
			{
				return Fail(NamePos, FString::Printf(TEXT("expected an attribute name, '>' or '/>' here, found %s"), *Found()));
			}
			for (const FSlaterAttribute& Existing : Node.Attributes)
			{
				if (Existing.Name.Equals(Name, ESearchCase::CaseSensitive))
				{
					return Fail(NamePos, FString::Printf(TEXT("duplicate attribute '%s' on <%s> (first written at line %d, column %d)"), *Name, *Node.Tag, Existing.Pos.Line, Existing.Pos.Column));
				}
			}

			SkipSpaces();
			if (Peek() != '=')
			{
				return Fail(NamePos, FString::Printf(TEXT("attribute '%s' has no value; write %s=\"...\""), *Name, *Name));
			}
			Advance();
			SkipSpaces();

			const TCHAR Quote = Peek();
			if (Quote != '"' && Quote != '\'')
			{
				return Fail(Here(), FString::Printf(TEXT("the value of '%s' must be in quotes, found %s"), *Name, *Found()));
			}
			const FSlaterPos QuotePos = Here();
			Advance();

			FString Value;
			while (true)
			{
				if (AtEnd())
				{
					return Fail(QuotePos, FString::Printf(TEXT("the value of '%s' is never closed: the file ends before the closing %c"), *Name, Quote));
				}
				const TCHAR C = Peek();
				if (C == Quote)
				{
					Advance();
					break;
				}
				if (C == '<')
				{
					return Fail(Here(), FString::Printf(TEXT("'<' cannot appear in the value of '%s' (starts at line %d, column %d); write &lt;, or a closing %c is missing"), *Name, QuotePos.Line, QuotePos.Column, Quote));
				}
				if (!IsAllowedChar(C))
				{
					return FailControlChar();
				}
				if (C == '&')
				{
					if (!ReadReference(Value))
					{
						return false;
					}
					continue;
				}
				// Like XML, a tab or line break typed into a value is a space; &#10; keeps a real one.
				Value.AppendChar(IsSpace(C) ? TEXT(' ') : C);
				Advance();
			}

			FSlaterAttribute& Added = Node.Attributes.AddDefaulted_GetRef();
			Added.Name = MoveTemp(Name);
			Added.Value = MoveTemp(Value);
			Added.Pos = NamePos;
			return true;
		}

		bool ReadClosingTag()
		{
			const FSlaterPos At = Here();
			Advance(2);

			FString Tag;
			if (!ReadName(Tag))
			{
				return Fail(Here(), FString::Printf(TEXT("expected a tag name after '</', found %s"), *Found()));
			}
			SkipSpaces();
			if (Peek() != '>')
			{
				return Fail(Here(), FString::Printf(TEXT("expected '>' to end </%s>, found %s"), *Tag, *Found()));
			}
			Advance();

			if (Open.IsEmpty())
			{
				return Fail(At, FString::Printf(TEXT("closing tag </%s> has no opening tag"), *Tag));
			}
			const FSlaterNode& Top = *Open.Last();
			if (!Top.Tag.Equals(Tag, ESearchCase::CaseSensitive))
			{
				return Fail(At, FString::Printf(TEXT("closing tag </%s> does not match <%s> opened at line %d, column %d; close <%s> first, or write <%s/> if it has no children"),
					*Tag, *Top.Tag, Top.Pos.Line, Top.Pos.Column, *Top.Tag, *Top.Tag));
			}
			Open.Pop();
			return true;
		}

		FString Source;
		const TCHAR* Data = nullptr;
		int32 Len = 0;
		int32 Idx = 0;
		int32 Line = 1;
		/** Index of the first character of the current line, for the column. */
		int32 LineStart = 0;

		const FString& Path;
		const FSlaterLimits& Limits;
		FSlaterNode& Root;
		FSlaterError& Error;

		/** The elements whose closing tag has not been read. Pointers stay valid: only the top one gets children. */
		TArray<FSlaterNode*> Open;
		int32 NodeCount = 0;
		bool bRootSeen = false;
	};
}
using namespace SlaterMarkupPrivate;

bool Slater::ParseMarkup(const FString& Text, const FString& Path, FSlaterNode& OutRoot, FSlaterError& OutError, const FSlaterLimits& Limits)
{
	OutRoot = FSlaterNode();
	OutError = FSlaterError();

	if (Text.Len() > Limits.MaxChars)
	{
		OutError.Path = Path;
		OutError.Line = 1;
		OutError.Column = 1;
		OutError.Message = FString::Printf(TEXT("the file is %d characters long; the limit is %d"), Text.Len(), Limits.MaxChars);
		return false;
	}

	FMarkupParser Parser(Text, Path, Limits, OutRoot, OutError);
	if (Parser.Run())
	{
		return true;
	}
	OutRoot = FSlaterNode();
	return false;
}
