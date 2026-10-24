// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterStyle.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const TCHAR* LexToString(ESlaterTokenKind Kind)
{
	switch (Kind)
	{
	case ESlaterTokenKind::Color: return TEXT("color");
	case ESlaterTokenKind::Metric: return TEXT("metric");
	case ESlaterTokenKind::Font: return TEXT("font");
	case ESlaterTokenKind::Surface: return TEXT("surface");
	case ESlaterTokenKind::Motion: return TEXT("motion");
	}
	return TEXT("token");
}

namespace Slater
{
	FName ParentToken(FName Token)
	{
		const FString Text = Token.ToString();
		int32 Dot = INDEX_NONE;
		if (Text.FindLastChar('.', Dot) && Dot > 0)
		{
			return FName(*Text.Left(Dot));
		}
		return NAME_None;
	}

	bool IsValidTokenName(const FString& Name)
	{
		if (Name.IsEmpty())
		{
			return false;
		}
		bool bSegmentStart = true;
		for (const TCHAR C : Name)
		{
			const bool bLetter = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
			if (C == '.')
			{
				if (bSegmentStart)
				{
					return false;
				}
				bSegmentStart = true;
			}
			else if (bLetter || (!bSegmentStart && C >= '0' && C <= '9'))
			{
				bSegmentStart = false;
			}
			else
			{
				return false;
			}
		}
		return !bSegmentStart;
	}

	namespace SlaterStylePrivate
	{
		int32 EditDistance(const FString& A, const FString& B)
		{
			TArray<int32> Previous;
			TArray<int32> Current;
			for (int32 J = 0; J <= B.Len(); ++J)
			{
				Previous.Add(J);
			}
			Current.SetNum(B.Len() + 1);
			for (int32 I = 1; I <= A.Len(); ++I)
			{
				Current[0] = I;
				for (int32 J = 1; J <= B.Len(); ++J)
				{
					const int32 Substitute = Previous[J - 1] + (FChar::ToLower(A[I - 1]) == FChar::ToLower(B[J - 1]) ? 0 : 1);
					Current[J] = FMath::Min3(Substitute, Previous[J] + 1, Current[J - 1] + 1);
				}
				Swap(Previous, Current);
			}
			return Previous[B.Len()];
		}
	}
	using namespace SlaterStylePrivate;

	FString SuggestNearest(const FString& Name, const TArray<FString>& Candidates)
	{
		const int32 Allowed = FMath::Max(2, Name.Len() / 4);
		int32 BestDistance = Allowed + 1;
		const FString* Best = nullptr;
		for (const FString& Candidate : Candidates)
		{
			const int32 Distance = EditDistance(Name, Candidate);
			if (Distance < BestDistance && Distance < FMath::Max(Name.Len(), Candidate.Len()))
			{
				BestDistance = Distance;
				Best = &Candidate;
			}
		}
		return Best ? *Best : FString();
	}

	const TArray<FString>& EaseWords()
	{
		static const TArray<FString> Words = {TEXT("Linear"), TEXT("QuadIn"), TEXT("QuadOut"), TEXT("QuadInOut"), TEXT("CubicIn"), TEXT("CubicOut"), TEXT("CubicInOut")};
		return Words;
	}

	FString DidYouMean(const ISlaterStyleSource& Source, ESlaterTokenKind Kind, const FString& Name)
	{
		TArray<FName> Known;
		Source.GetKnownTokens(Kind, Known);
		TArray<FString> Candidates;
		for (const FName Token : Known)
		{
			Candidates.Add(Token.ToString());
		}
		const FString Nearest = SuggestNearest(Name, Candidates);
		return Nearest.IsEmpty() ? FString() : FString::Printf(TEXT("; did you mean '%s'?"), *Nearest);
	}
}

namespace SlaterStylePrivate
{
	/** Name, then each parent, calling Try on each until it succeeds. */
	template <typename TryFunc>
	bool WalkUp(FName Token, TryFunc&& Try)
	{
		while (!Token.IsNone())
		{
			if (Try(Token))
			{
				return true;
			}
			Token = Slater::ParentToken(Token);
		}
		return false;
	}

	/** The definition's colors and metrics looked up in Source, so a stack resolves them through all its layers. */
	bool ResolveSurface(const FSlaterSurfaceDef& Def, const ISlaterStyleSource& Source, FSlaterSurface& Out)
	{
		FSlaterSurface Result;
		if (!Def.Fill.IsNone() && !Source.FindColor(Def.Fill, Result.Fill))
		{
			return false;
		}
		if (!Def.Border.IsNone() && !Source.FindColor(Def.Border, Result.Border))
		{
			return false;
		}
		Result.BorderWidth = Def.BorderWidth;
		if (!Def.BorderWidthToken.IsNone() && !Source.FindMetric(Def.BorderWidthToken, Result.BorderWidth))
		{
			return false;
		}
		Result.CornerRadius = Def.CornerRadius;
		if (!Def.CornerRadiusToken.IsNone() && !Source.FindMetric(Def.CornerRadiusToken, Result.CornerRadius))
		{
			return false;
		}
		Out = Result;
		return true;
	}

	template <typename ValueType>
	void AppendKeys(const TMap<FName, ValueType>& Map, TArray<FName>& Out)
	{
		for (const TPair<FName, ValueType>& Pair : Map)
		{
			Out.AddUnique(Pair.Key);
		}
	}
}
using namespace SlaterStylePrivate;

bool FSlaterStyle::FindColor(FName Token, FLinearColor& OutColor) const
{
	return WalkUp(Token, [&](FName Name)
	{
		const FLinearColor* Found = Colors.Find(Name);
		if (Found) { OutColor = *Found; }
		return Found != nullptr;
	});
}

bool FSlaterStyle::FindMetric(FName Token, float& OutMetric) const
{
	return WalkUp(Token, [&](FName Name)
	{
		const float* Found = Metrics.Find(Name);
		if (Found) { OutMetric = *Found; }
		return Found != nullptr;
	});
}

bool FSlaterStyle::FindFont(FName Token, FSlaterFont& OutFont) const
{
	return WalkUp(Token, [&](FName Name)
	{
		const FSlaterFont* Found = Fonts.Find(Name);
		if (Found) { OutFont = *Found; }
		return Found != nullptr;
	});
}

bool FSlaterStyle::FindSurface(FName Token, FSlaterSurface& OutSurface) const
{
	const FSlaterSurfaceDef* Def = nullptr;
	WalkUp(Token, [&](FName Name)
	{
		Def = Surfaces.Find(Name);
		return Def != nullptr;
	});
	return Def && ResolveSurface(*Def, *this, OutSurface);
}

bool FSlaterStyle::FindMotion(FName Token, FSlaterMotion& OutMotion) const
{
	return WalkUp(Token, [&](FName Name)
	{
		const FSlaterMotion* Found = Motions.Find(Name);
		if (Found) { OutMotion = *Found; }
		return Found != nullptr;
	});
}

void FSlaterStyle::GetKnownTokens(ESlaterTokenKind Kind, TArray<FName>& OutTokens) const
{
	switch (Kind)
	{
	case ESlaterTokenKind::Color: AppendKeys(Colors, OutTokens); break;
	case ESlaterTokenKind::Metric: AppendKeys(Metrics, OutTokens); break;
	case ESlaterTokenKind::Font: AppendKeys(Fonts, OutTokens); break;
	case ESlaterTokenKind::Surface: AppendKeys(Surfaces, OutTokens); break;
	case ESlaterTokenKind::Motion: AppendKeys(Motions, OutTokens); break;
	}
}

bool FSlaterStyleStack::FindColor(FName Token, FLinearColor& OutColor) const
{
	return WalkUp(Token, [&](FName Name)
	{
		for (int32 I = Layers.Num() - 1; I >= 0; --I)
		{
			if (const FLinearColor* Found = Layers[I]->Colors.Find(Name))
			{
				OutColor = *Found;
				return true;
			}
		}
		return false;
	});
}

bool FSlaterStyleStack::FindMetric(FName Token, float& OutMetric) const
{
	return WalkUp(Token, [&](FName Name)
	{
		for (int32 I = Layers.Num() - 1; I >= 0; --I)
		{
			if (const float* Found = Layers[I]->Metrics.Find(Name))
			{
				OutMetric = *Found;
				return true;
			}
		}
		return false;
	});
}

bool FSlaterStyleStack::FindFont(FName Token, FSlaterFont& OutFont) const
{
	return WalkUp(Token, [&](FName Name)
	{
		for (int32 I = Layers.Num() - 1; I >= 0; --I)
		{
			if (const FSlaterFont* Found = Layers[I]->Fonts.Find(Name))
			{
				OutFont = *Found;
				return true;
			}
		}
		return false;
	});
}

bool FSlaterStyleStack::FindSurface(FName Token, FSlaterSurface& OutSurface) const
{
	const FSlaterSurfaceDef* Def = nullptr;
	WalkUp(Token, [&](FName Name)
	{
		for (int32 I = Layers.Num() - 1; I >= 0 && !Def; --I)
		{
			Def = Layers[I]->Surfaces.Find(Name);
		}
		return Def != nullptr;
	});
	return Def && ResolveSurface(*Def, *this, OutSurface);
}

bool FSlaterStyleStack::FindMotion(FName Token, FSlaterMotion& OutMotion) const
{
	return WalkUp(Token, [&](FName Name)
	{
		for (int32 I = Layers.Num() - 1; I >= 0; --I)
		{
			if (const FSlaterMotion* Found = Layers[I]->Motions.Find(Name))
			{
				OutMotion = *Found;
				return true;
			}
		}
		return false;
	});
}

void FSlaterStyleStack::GetKnownTokens(ESlaterTokenKind Kind, TArray<FName>& OutTokens) const
{
	for (const TSharedRef<const FSlaterStyle>& Layer : Layers)
	{
		Layer->GetKnownTokens(Kind, OutTokens);
	}
}

// ---- Style files ----

namespace SlaterStylePrivate
{
	/** `#RRGGBB` or `#RRGGBBAA`, sRGB as designers write it. */
	bool ParseColorLiteral(const FString& Text, FLinearColor& Out)
	{
		if ((Text.Len() != 7 && Text.Len() != 9) || Text[0] != '#')
		{
			return false;
		}
		uint32 Channels[4] = {0, 0, 0, 255};
		for (int32 I = 0; I < (Text.Len() - 1) / 2; ++I)
		{
			const FString Pair = Text.Mid(1 + I * 2, 2);
			if (!FChar::IsHexDigit(Pair[0]) || !FChar::IsHexDigit(Pair[1]))
			{
				return false;
			}
			Channels[I] = FParse::HexNumber(*Pair);
		}
		Out = FLinearColor::FromSRGBColor(FColor(static_cast<uint8>(Channels[0]), static_cast<uint8>(Channels[1]), static_cast<uint8>(Channels[2]), static_cast<uint8>(Channels[3])));
		return true;
	}

	bool ParseNumber(const FString& Text, float& Out)
	{
		if (!FCString::IsNumeric(*Text))
		{
			return false;
		}
		Out = FCString::Atof(*Text);
		return FMath::IsFinite(Out);
	}

	TArray<FString> ToStrings(std::initializer_list<const TCHAR*> List)
	{
		TArray<FString> Result;
		for (const TCHAR* Item : List)
		{
			Result.Add(Item);
		}
		return Result;
	}

	FString Near(const FString& Name, const TArray<FString>& Candidates)
	{
		const FString Nearest = Slater::SuggestNearest(Name, Candidates);
		return Nearest.IsEmpty() ? FString() : FString::Printf(TEXT("; did you mean '%s'?"), *Nearest);
	}

	FString JoinQuoted(const TArray<FString>& Items)
	{
		FString Out;
		for (const FString& Item : Items)
		{
			Out += (Out.IsEmpty() ? TEXT("'") : TEXT(", '")) + Item + TEXT("'");
		}
		return Out;
	}

	/** Reads the children of a `<style>` element into a style, collecting errors. */
	class FStyleReader
	{
	public:
		FStyleReader(const FSlaterNode& InRoot, const FString& InPath, FSlaterStyle& InStyle, TArray<FSlaterError>& InErrors)
			: Root(InRoot), Path(InPath), Style(InStyle), Errors(InErrors)
		{
		}

		void Run()
		{
			if (Root.Tag != TEXT("style"))
			{
				Report(Root.Pos, FString::Printf(TEXT("the root element must be <style>, not <%s>%s"), *Root.Tag, *Near(Root.Tag, ToStrings({TEXT("style")}))));
				return;
			}
			CheckAttributes(Root, {TEXT("name"), TEXT("extends")});
			Style.Path = Path;
			if (const FSlaterAttribute* Name = Root.FindAttribute(TEXT("name")))
			{
				if (Slater::IsValidTokenName(Name->Value) && !Name->Value.Contains(TEXT(".")))
				{
					Style.Name = FName(*Name->Value);
				}
				else
				{
					Report(Name->Pos, FString::Printf(TEXT("style name '%s' must be a single word of letters, digits and underscores, not starting with a digit"), *Name->Value));
				}
			}
			else
			{
				Report(Root.Pos, TEXT("<style> needs a name attribute"));
			}
			if (const FSlaterAttribute* Extends = Root.FindAttribute(TEXT("extends")))
			{
				if (Slater::IsValidTokenName(Extends->Value) && !Extends->Value.Contains(TEXT(".")))
				{
					Style.Extends = FName(*Extends->Value);
					Style.ExtendsPos = Extends->Pos;
				}
				else
				{
					Report(Extends->Pos, FString::Printf(TEXT("extends '%s' is not a style name"), *Extends->Value));
				}
			}
			if (!Root.Text.IsEmpty())
			{
				Report(Root.TextPos, TEXT("<style> holds only <color>, <metric>, <font>, <surface> and <motion> elements, not text"));
			}

			const TArray<FString> Tags = ToStrings({TEXT("color"), TEXT("metric"), TEXT("font"), TEXT("surface"), TEXT("motion")});
			for (const FSlaterNode& Child : Root.Children)
			{
				if (!Child.Children.IsEmpty() || !Child.Text.IsEmpty())
				{
					Report(Child.Children.IsEmpty() ? Child.TextPos : Child.Children[0].Pos, FString::Printf(TEXT("<%s> is empty: it takes attributes only"), *Child.Tag));
				}
				if (Child.Tag == TEXT("color")) { ReadColor(Child); }
				else if (Child.Tag == TEXT("metric")) { ReadMetric(Child); }
				else if (Child.Tag == TEXT("font")) { ReadFont(Child); }
				else if (Child.Tag == TEXT("surface")) { ReadSurface(Child); }
				else if (Child.Tag == TEXT("motion")) { ReadMotion(Child); }
				else
				{
					Report(Child.Pos, FString::Printf(TEXT("unknown element <%s> in a style; expected <color>, <metric>, <font>, <surface> or <motion>%s"), *Child.Tag, *Near(Child.Tag, Tags)));
				}
			}
		}

	private:
		const FSlaterNode& Root;
		const FString& Path;
		FSlaterStyle& Style;
		TArray<FSlaterError>& Errors;
		/** Where each token was first defined, per kind, for the duplicate message. */
		TMap<FName, FSlaterPos> First[5];

		void Report(const FSlaterPos& Pos, const FString& Message)
		{
			FSlaterError& Error = Errors.AddDefaulted_GetRef();
			Error.Path = Path;
			Error.Line = Pos.Line;
			Error.Column = Pos.Column;
			Error.Message = Message;
		}

		void CheckAttributes(const FSlaterNode& Node, const TArray<FString>& Allowed)
		{
			for (const FSlaterAttribute& Attribute : Node.Attributes)
			{
				if (!Allowed.Contains(Attribute.Name))
				{
					Report(Attribute.Pos, FString::Printf(TEXT("<%s> has no attribute '%s'; it takes %s%s"), *Node.Tag, *Attribute.Name, *JoinQuoted(Allowed), *Near(Attribute.Name, Allowed)));
				}
			}
		}

		/** The attribute's value, or null after reporting that it is missing. */
		const FSlaterAttribute* Require(const FSlaterNode& Node, const TCHAR* Name)
		{
			const FSlaterAttribute* Attribute = Node.FindAttribute(Name);
			if (!Attribute)
			{
				Report(Node.Pos, FString::Printf(TEXT("<%s> needs a '%s' attribute"), *Node.Tag, Name));
			}
			return Attribute;
		}

		/** The token name of a definition, after checking its form and that this file has not defined it already. */
		FName ReadTokenName(const FSlaterNode& Node, ESlaterTokenKind Kind)
		{
			const FSlaterAttribute* Name = Require(Node, TEXT("name"));
			if (!Name)
			{
				return NAME_None;
			}
			if (!Slater::IsValidTokenName(Name->Value))
			{
				Report(Name->Pos, FString::Printf(TEXT("'%s' is not a token name; use dotted words such as UI.Color.Accent"), *Name->Value));
				return NAME_None;
			}
			const FName Token(*Name->Value);
			if (const FSlaterPos* Earlier = First[static_cast<int32>(Kind)].Find(Token))
			{
				Report(Name->Pos, FString::Printf(TEXT("%s '%s' is already defined at (%d,%d)"), LexToString(Kind), *Name->Value, Earlier->Line, Earlier->Column));
				return NAME_None;
			}
			First[static_cast<int32>(Kind)].Add(Token, Name->Pos);
			return Token;
		}

		void ReadColor(const FSlaterNode& Node)
		{
			CheckAttributes(Node, {TEXT("name"), TEXT("value")});
			const FName Token = ReadTokenName(Node, ESlaterTokenKind::Color);
			const FSlaterAttribute* Value = Require(Node, TEXT("value"));
			if (Token.IsNone() || !Value)
			{
				return;
			}
			FLinearColor Color;
			if (!ParseColorLiteral(Value->Value, Color))
			{
				Report(Value->Pos, FString::Printf(TEXT("color value '%s' must be #RRGGBB or #RRGGBBAA"), *Value->Value));
				return;
			}
			Style.Colors.Add(Token, Color);
		}

		void ReadMetric(const FSlaterNode& Node)
		{
			CheckAttributes(Node, {TEXT("name"), TEXT("value")});
			const FName Token = ReadTokenName(Node, ESlaterTokenKind::Metric);
			const FSlaterAttribute* Value = Require(Node, TEXT("value"));
			if (Token.IsNone() || !Value)
			{
				return;
			}
			float Number = 0.f;
			if (!ParseNumber(Value->Value, Number))
			{
				Report(Value->Pos, FString::Printf(TEXT("metric value '%s' is not a number"), *Value->Value));
				return;
			}
			Style.Metrics.Add(Token, Number);
		}

		void ReadFont(const FSlaterNode& Node)
		{
			CheckAttributes(Node, {TEXT("name"), TEXT("role"), TEXT("size")});
			const FName Token = ReadTokenName(Node, ESlaterTokenKind::Font);
			const FSlaterAttribute* Role = Require(Node, TEXT("role"));
			const FSlaterAttribute* Size = Require(Node, TEXT("size"));
			if (Token.IsNone() || !Role || !Size)
			{
				return;
			}
			const TArray<FString> Roles = ToStrings({TEXT("Regular"), TEXT("Bold"), TEXT("Italic")});
			bool bOk = true;
			if (!Roles.Contains(Role->Value))
			{
				Report(Role->Pos, FString::Printf(TEXT("font role '%s' is not one of %s%s"), *Role->Value, *JoinQuoted(Roles), *Near(Role->Value, Roles)));
				bOk = false;
			}
			float SizeValue = 0.f;
			if (!ParseNumber(Size->Value, SizeValue) || SizeValue < 1.f || SizeValue > 512.f || SizeValue != FMath::FloorToFloat(SizeValue))
			{
				Report(Size->Pos, FString::Printf(TEXT("font size '%s' must be a whole number from 1 to 512"), *Size->Value));
				bOk = false;
			}
			if (bOk)
			{
				Style.Fonts.Add(Token, FSlaterFont{FName(*Role->Value), static_cast<int32>(SizeValue)});
			}
		}

		void ReadMotion(const FSlaterNode& Node)
		{
			CheckAttributes(Node, {TEXT("name"), TEXT("duration"), TEXT("ease")});
			const FName Token = ReadTokenName(Node, ESlaterTokenKind::Motion);
			const FSlaterAttribute* Duration = Require(Node, TEXT("duration"));
			if (Token.IsNone() || !Duration)
			{
				return;
			}
			bool bOk = true;
			float Seconds = 0.f;
			if (!ParseNumber(Duration->Value, Seconds) || Seconds < 0.f || Seconds > FSlaterMotion::MaxDuration)
			{
				Report(Duration->Pos, FString::Printf(TEXT("motion duration '%s' must be seconds from 0 to %g"), *Duration->Value, FSlaterMotion::MaxDuration));
				bOk = false;
			}
			FName Ease = TEXT("Linear");
			if (const FSlaterAttribute* EaseAttribute = Node.FindAttribute(TEXT("ease")))
			{
				if (!Slater::EaseWords().Contains(EaseAttribute->Value))
				{
					Report(EaseAttribute->Pos, FString::Printf(TEXT("motion ease '%s' is not one of %s%s"), *EaseAttribute->Value, *JoinQuoted(Slater::EaseWords()), *Near(EaseAttribute->Value, Slater::EaseWords())));
					bOk = false;
				}
				else
				{
					Ease = FName(*EaseAttribute->Value);
				}
			}
			if (bOk)
			{
				Style.Motions.Add(Token, FSlaterMotion{Seconds, Ease});
			}
		}

		/** A metric attribute that is a literal number or a metric token. Returns false after reporting. */
		bool ReadNumberOrToken(const FSlaterAttribute* Attribute, float& OutLiteral, FName& OutToken)
		{
			if (!Attribute)
			{
				return true;
			}
			float Number = 0.f;
			if (ParseNumber(Attribute->Value, Number))
			{
				if (Number < 0.f)
				{
					Report(Attribute->Pos, FString::Printf(TEXT("'%s' must not be negative"), *Attribute->Name));
					return false;
				}
				OutLiteral = Number;
				return true;
			}
			if (!Slater::IsValidTokenName(Attribute->Value))
			{
				Report(Attribute->Pos, FString::Printf(TEXT("'%s' must be a number or a metric token, not '%s'"), *Attribute->Name, *Attribute->Value));
				return false;
			}
			OutToken = FName(*Attribute->Value);
			return true;
		}

		void ReadSurface(const FSlaterNode& Node)
		{
			CheckAttributes(Node, {TEXT("name"), TEXT("fill"), TEXT("border"), TEXT("border-width"), TEXT("radius")});
			const FName Token = ReadTokenName(Node, ESlaterTokenKind::Surface);
			if (Token.IsNone())
			{
				return;
			}
			FSlaterSurfaceDef Def;
			Def.Pos = Node.Pos;
			bool bOk = true;
			const auto ReadColorToken = [&](const TCHAR* Name, FName& OutToken, FSlaterPos& OutPos)
			{
				if (const FSlaterAttribute* Attribute = Node.FindAttribute(Name))
				{
					if (Slater::IsValidTokenName(Attribute->Value))
					{
						OutToken = FName(*Attribute->Value);
						OutPos = Attribute->Pos;
					}
					else
					{
						Report(Attribute->Pos, FString::Printf(TEXT("'%s' must name a color token such as UI.Color.Panel, not '%s'"), Name, *Attribute->Value));
						bOk = false;
					}
				}
			};
			ReadColorToken(TEXT("fill"), Def.Fill, Def.FillPos);
			ReadColorToken(TEXT("border"), Def.Border, Def.BorderPos);
			const FSlaterAttribute* Width = Node.FindAttribute(TEXT("border-width"));
			const FSlaterAttribute* Radius = Node.FindAttribute(TEXT("radius"));
			bOk &= ReadNumberOrToken(Width, Def.BorderWidth, Def.BorderWidthToken);
			bOk &= ReadNumberOrToken(Radius, Def.CornerRadius, Def.CornerRadiusToken);
			Def.BorderWidthPos = Width ? Width->Pos : FSlaterPos();
			Def.CornerRadiusPos = Radius ? Radius->Pos : FSlaterPos();
			if (bOk)
			{
				Style.Surfaces.Add(Token, Def);
			}
		}
	};
}
using namespace SlaterStylePrivate;

bool Slater::ParseStyle(const FString& Text, const FString& Path, FSlaterStyle& OutStyle, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits)
{
	OutStyle = FSlaterStyle();
	FSlaterNode Root;
	FSlaterError ParseError;
	if (!ParseMarkup(Text, Path, Root, ParseError, Limits))
	{
		OutErrors.Add(ParseError);
		return false;
	}
	const int32 ErrorsBefore = OutErrors.Num();
	FSlaterStyle Style;
	FStyleReader(Root, Path, Style, OutErrors).Run();
	if (OutErrors.Num() != ErrorsBefore)
	{
		return false;
	}
	OutStyle = MoveTemp(Style);
	return true;
}

// ---- Library ----

bool FSlaterStyleLibrary::AddFromText(const FString& Text, const FString& Path, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits)
{
	TSharedRef<FSlaterStyle> Style = MakeShared<FSlaterStyle>();
	if (!Slater::ParseStyle(Text, Path, *Style, OutErrors, Limits))
	{
		return false;
	}
	if (const TSharedRef<FSlaterStyle>* Existing = Styles.Find(Style->Name))
	{
		FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
		Error.Path = Path;
		Error.Line = 1;
		Error.Column = 1;
		Error.Message = FString::Printf(TEXT("style '%s' is already defined in %s"), *Style->Name.ToString(), *(*Existing)->Path);
		return false;
	}
	Styles.Add(Style->Name, Style);
	return true;
}

bool FSlaterStyleLibrary::LoadDirectory(const FString& Directory, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits)
{
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
		Error.Path = Directory;
		Error.Message = TEXT("the style folder does not exist");
		return false;
	}
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Directory / FString::Printf(TEXT("*.%s"), Slater::StyleFileExtension())), true, false);
	Files.Sort();

	bool bOk = true;
	for (const FString& File : Files)
	{
		const FString FullPath = Directory / File;
		FString Text;
		// The size is checked before the read so that a huge file never gets loaded whole.
		if (IFileManager::Get().FileSize(*FullPath) > Limits.MaxChars * 4)
		{
			FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
			Error.Path = FullPath;
			Error.Message = FString::Printf(TEXT("the file is larger than %d characters"), Limits.MaxChars);
			bOk = false;
		}
		else if (!FFileHelper::LoadFileToString(Text, *FullPath))
		{
			FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
			Error.Path = FullPath;
			Error.Message = TEXT("the file could not be read");
			bOk = false;
		}
		else
		{
			bOk &= AddFromText(Text, FullPath, OutErrors, Limits);
		}
	}
	return Validate(OutErrors) && bOk;
}

bool FSlaterStyleLibrary::LoadPluginStyles(TArray<FSlaterError>& OutErrors)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlaterUI"));
	if (!Plugin.IsValid())
	{
		FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
		Error.Path = TEXT("SlaterUI");
		Error.Message = TEXT("the SlaterUI plugin is not loaded");
		return false;
	}
	return LoadDirectory(Plugin->GetContentDir() / TEXT("UI/Styles"), OutErrors);
}

TSharedPtr<const FSlaterStyle> FSlaterStyleLibrary::Find(FName Name) const
{
	const TSharedRef<FSlaterStyle>* Found = Styles.Find(Name);
	return Found ? TSharedPtr<const FSlaterStyle>(*Found) : nullptr;
}

void FSlaterStyleLibrary::GetStyleNames(TArray<FName>& OutNames) const
{
	Styles.GetKeys(OutNames);
}

bool FSlaterStyleLibrary::BuildStack(FName Name, FSlaterStyleStack& OutStack) const
{
	TArray<TSharedRef<const FSlaterStyle>> Chain;
	TSet<FName> Seen;
	for (FName Current = Name; !Current.IsNone(); )
	{
		const TSharedRef<FSlaterStyle>* Found = Styles.Find(Current);
		if (!Found || Seen.Contains(Current))
		{
			return false;
		}
		Seen.Add(Current);
		Chain.Add(*Found);
		Current = (*Found)->Extends;
	}
	for (int32 I = Chain.Num() - 1; I >= 0; --I)
	{
		OutStack.PushLayer(Chain[I]);
	}
	return true;
}

bool FSlaterStyleLibrary::Validate(TArray<FSlaterError>& OutErrors) const
{
	bool bOk = true;
	const auto Report = [&](const FSlaterStyle& Style, const FSlaterPos& Pos, const FString& Message)
	{
		FSlaterError& Error = OutErrors.AddDefaulted_GetRef();
		Error.Path = Style.Path;
		Error.Line = Pos.Line;
		Error.Column = Pos.Column;
		Error.Message = Message;
		bOk = false;
	};

	TArray<FString> StyleNames;
	for (const TPair<FName, TSharedRef<FSlaterStyle>>& Pair : Styles)
	{
		StyleNames.Add(Pair.Key.ToString());
	}
	StyleNames.Sort();

	for (const FString& NameText : StyleNames)
	{
		const FSlaterStyle& Style = *Styles.FindChecked(FName(*NameText));
		FSlaterStyleStack Stack;
		if (!Style.Extends.IsNone() && !Styles.Contains(Style.Extends))
		{
			Report(Style, Style.ExtendsPos, FString::Printf(TEXT("style '%s' extends '%s', which is not loaded%s"), *NameText, *Style.Extends.ToString(), *Near(Style.Extends.ToString(), StyleNames)));
			continue;
		}
		if (!BuildStack(Style.Name, Stack))
		{
			Report(Style, Style.ExtendsPos, FString::Printf(TEXT("style '%s' extends itself through a cycle"), *NameText));
			continue;
		}

		// Surfaces: every color and metric they name must exist in the stack (with parent fall-back).
		TArray<FName> Surfaces;
		Style.GetKnownTokens(ESlaterTokenKind::Surface, Surfaces);
		Surfaces.Sort(FNameLexicalLess());
		for (const FName SurfaceName : Surfaces)
		{
			const FSlaterSurfaceDef& Def = Style.Surfaces.FindChecked(SurfaceName);
			const auto Check = [&](FName Token, const FSlaterPos& Pos, ESlaterTokenKind Kind)
			{
				if (Token.IsNone())
				{
					return;
				}
				FLinearColor Color;
				float Metric;
				const bool bFound = Kind == ESlaterTokenKind::Color ? Stack.FindColor(Token, Color) : Stack.FindMetric(Token, Metric);
				if (!bFound)
				{
					Report(Style, Pos, FString::Printf(TEXT("surface '%s' uses %s token '%s', which style '%s' does not define%s"), *SurfaceName.ToString(), LexToString(Kind), *Token.ToString(), *NameText, *Slater::DidYouMean(Stack, Kind, Token.ToString())));
				}
			};
			Check(Def.Fill, Def.FillPos, ESlaterTokenKind::Color);
			Check(Def.Border, Def.BorderPos, ESlaterTokenKind::Color);
			Check(Def.BorderWidthToken, Def.BorderWidthPos, ESlaterTokenKind::Metric);
			Check(Def.CornerRadiusToken, Def.CornerRadiusPos, ESlaterTokenKind::Metric);
		}
	}
	return bOk;
}
