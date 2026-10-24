// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterTemplates.h"

#include "SlaterElements.h"
#include "SlaterStyle.h"
#include "SlaterView.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

namespace SlaterTemplatesPrivate
{
	const TCHAR* const TagTemplate = TEXT("template");
	const TCHAR* const TagImport = TEXT("import");
	/** Attributes of a use that are not arguments: they go on the root element of the body. */
	const TCHAR* const PassThrough[] = {TEXT("id"), TEXT("fill"), TEXT("margin"), TEXT("halign"), TEXT("valign")};
	constexpr int32 MaxNesting = 16;
	constexpr int32 MaxImportDepth = 8;

	bool IsIdentStart(TCHAR C)
	{
		return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || C == '_';
	}

	bool IsIdentChar(TCHAR C)
	{
		return IsIdentStart(C) || (C >= '0' && C <= '9');
	}

	bool IsParamName(const FString& Name)
	{
		if (Name.IsEmpty() || !IsIdentStart(Name[0]))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!IsIdentChar(C))
			{
				return false;
			}
		}
		return true;
	}

	/** Lowercase letters, digits and hyphens, starting with a letter: the same words element tags are. */
	bool IsTemplateName(const FString& Name)
	{
		if (Name.IsEmpty() || !(Name[0] >= 'a' && Name[0] <= 'z'))
		{
			return false;
		}
		for (const TCHAR C : Name)
		{
			if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-'))
			{
				return false;
			}
		}
		return true;
	}

	bool IsPassThrough(const FString& Name)
	{
		for (const TCHAR* Word : PassThrough)
		{
			if (Name == Word)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountNodes(const FSlaterNode& Node)
	{
		int32 Count = 1;
		for (const FSlaterNode& Child : Node.Children)
		{
			Count += CountNodes(Child);
		}
		return Count;
	}

	void StampFile(FSlaterNode& Node, FName File)
	{
		Node.Pos.File = File;
		Node.TextPos.File = File;
		for (FSlaterAttribute& Attr : Node.Attributes)
		{
			Attr.Pos.File = File;
		}
		for (FSlaterNode& Child : Node.Children)
		{
			StampFile(Child, File);
		}
	}

	struct FTemplateDef
	{
		FString Name;
		TArray<FString> Params;
		FSlaterNode Body;
		FSlaterPos Pos;
	};

	/**
	 * Substitutes `$name` in In. With Args null it only checks the names. False with Problem set on the first bad `$`.
	 * `$$` is a literal dollar.
	 */
	bool Substitute(const FString& In, const TArray<FString>& Params, const TMap<FString, FString>* Args, FString& Out, FString& Problem)
	{
		Out.Reset();
		for (int32 I = 0; I < In.Len(); ++I)
		{
			const TCHAR C = In[I];
			if (C != '$')
			{
				Out.AppendChar(C);
				continue;
			}
			if (I + 1 < In.Len() && In[I + 1] == '$')
			{
				Out.AppendChar('$');
				++I;
				continue;
			}
			if (I + 1 >= In.Len() || !IsIdentStart(In[I + 1]))
			{
				Problem = TEXT("'$' must start a parameter name such as $label; write $$ for a dollar sign");
				return false;
			}
			int32 End = I + 1;
			while (End < In.Len() && IsIdentChar(In[End]))
			{
				++End;
			}
			const FString Name = In.Mid(I + 1, End - I - 1);
			if (!Params.Contains(Name))
			{
				Problem = Params.Num() == 0
					? FString::Printf(TEXT("unknown parameter '$%s': this template has no parameters (write params=\"...\" on <template>)"), *Name)
					: FString::Printf(TEXT("unknown parameter '$%s'; the parameters are %s"), *Name, *FString::Join(Params, TEXT(", ")));
				return false;
			}
			if (Args)
			{
				Out += Args->FindRef(Name);
			}
			I = End - 1;
		}
		return true;
	}

	class FExpander
	{
	public:
		FExpander(const FString& InPath, const FSlaterElementRegistry& InRegistry, const FSlaterImportResolver& InResolver, TArray<FSlaterError>& InErrors, const FSlaterLimits& InLimits)
			: RootPath(InPath), Registry(InRegistry), Resolver(InResolver), Errors(InErrors), Limits(InLimits)
		{
		}

		bool Run(FSlaterNode& Root)
		{
			const int32 ErrorsBefore = Errors.Num();
			CollectDeclarations(Root, false);
			if (Errors.Num() != ErrorsBefore)
			{
				return false;
			}
			FSlaterNode Holder;
			Holder.Children.Add(MoveTemp(Root));
			TArray<FString> Stack;
			ExpandChildren(Holder, Stack);
			Root = Holder.Children.Num() > 0 ? MoveTemp(Holder.Children[0]) : FSlaterNode();
			return Errors.Num() == ErrorsBefore;
		}

	private:
		const FString& RootPath;
		const FSlaterElementRegistry& Registry;
		const FSlaterImportResolver& Resolver;
		TArray<FSlaterError>& Errors;
		const FSlaterLimits& Limits;
		TMap<FName, TSharedRef<FTemplateDef>> Defs;
		TSet<FString> Imported;
		TArray<FString> ImportStack;
		int32 NodesUsed = 0;
		bool bBudgetReported = false;

		void Error(const FSlaterPos& Pos, const FString& Message)
		{
			FSlaterError& Error = Errors.AddDefaulted_GetRef();
			Error.Path = Pos.File.IsNone() ? RootPath : Pos.File.ToString();
			Error.Line = Pos.Line;
			Error.Column = Pos.Column;
			Error.Message = Message;
		}

		/** Reads the `template` and `import` children of Container. In the root file they are removed from it; an imported file may hold nothing else. */
		void CollectDeclarations(FSlaterNode& Container, bool bImportedFile)
		{
			TArray<FSlaterNode> Kept;
			for (FSlaterNode& Child : Container.Children)
			{
				if (Child.Tag == TagTemplate)
				{
					ReadTemplate(Child);
				}
				else if (Child.Tag == TagImport)
				{
					ReadImport(Child);
				}
				else if (bImportedFile)
				{
					Error(Child.Pos, FString::Printf(TEXT("an imported file may hold only <template> and <import>, not <%s>"), *Child.Tag));
				}
				else
				{
					Kept.Add(MoveTemp(Child));
				}
			}
			if (!bImportedFile)
			{
				Container.Children = MoveTemp(Kept);
			}
		}

		void ReadImport(const FSlaterNode& Node)
		{
			const FSlaterAttribute* Src = nullptr;
			for (const FSlaterAttribute& Attr : Node.Attributes)
			{
				if (Attr.Name == TEXT("src"))
				{
					Src = &Attr;
				}
				else
				{
					Error(Attr.Pos, FString::Printf(TEXT("unknown attribute '%s' on <import>; allowed: src"), *Attr.Name));
				}
			}
			if (Node.Children.Num() > 0 || !Node.Text.IsEmpty())
			{
				Error(Node.Pos, TEXT("<import> has no content: write <import src=\"Common\"/>"));
			}
			if (!Src)
			{
				Error(Node.Pos, TEXT("<import> needs the attribute 'src' (the layout ID of the file to import), for example src=\"Common\""));
				return;
			}
			const FString& Id = Src->Value;
			if (ImportStack.Contains(Id))
			{
				Error(Src->Pos, FString::Printf(TEXT("import cycle: %s -> %s"), *FString::Join(ImportStack, TEXT(" -> ")), *Id));
				return;
			}
			if (Imported.Contains(Id))
			{
				return;
			}
			if (ImportStack.Num() >= MaxImportDepth)
			{
				Error(Src->Pos, FString::Printf(TEXT("imports are nested more than %d deep"), MaxImportDepth));
				return;
			}
			Imported.Add(Id);
			if (!Resolver)
			{
				Error(Src->Pos, TEXT("<import> is not available here: no way to find the file"));
				return;
			}
			FString Text;
			FString Label;
			FString Problem;
			if (!Resolver(Id, Text, Label, Problem))
			{
				Error(Src->Pos, FString::Printf(TEXT("cannot import '%s': %s"), *Id, *Problem));
				return;
			}
			FSlaterNode File;
			FSlaterError ParseError;
			if (!Slater::ParseMarkup(Text, Label, File, ParseError, Limits))
			{
				Slater::AppendSyntaxStopsBuildNote(ParseError);
				Errors.Add(ParseError);
				return;
			}
			StampFile(File, FName(*Label));
			if (File.Tag != TEXT("templates"))
			{
				Error(File.Pos, FString::Printf(TEXT("an imported file must have <templates> as its root element, not <%s>"), *File.Tag));
				return;
			}
			if (File.Attributes.Num() > 0)
			{
				Error(File.Attributes[0].Pos, TEXT("<templates> takes no attributes"));
			}
			ImportStack.Add(Id);
			CollectDeclarations(File, true);
			ImportStack.Pop();
		}

		void ReadTemplate(const FSlaterNode& Node)
		{
			FString Name;
			FString ParamText;
			bool bHasName = false;
			FSlaterPos NamePos = Node.Pos;
			for (const FSlaterAttribute& Attr : Node.Attributes)
			{
				if (Attr.Name == TEXT("name"))
				{
					Name = Attr.Value;
					NamePos = Attr.Pos;
					bHasName = true;
				}
				else if (Attr.Name == TEXT("params"))
				{
					ParamText = Attr.Value;
				}
				else
				{
					Error(Attr.Pos, FString::Printf(TEXT("unknown attribute '%s' on <template>; allowed: name, params"), *Attr.Name));
				}
			}
			if (!bHasName)
			{
				Error(Node.Pos, TEXT("<template> needs the attribute 'name' (the tag it is used by), for example name=\"stat-row\""));
				return;
			}
			if (!IsTemplateName(Name))
			{
				Error(NamePos, FString::Printf(TEXT("'%s' is not a template name: lowercase letters, digits and hyphens, starting with a letter"), *Name));
				return;
			}
			if (Registry.Find(FName(*Name, FNAME_Find)))
			{
				Error(NamePos, FString::Printf(TEXT("'%s' is a built-in element; pick another template name"), *Name));
				return;
			}
			if (const TSharedRef<FTemplateDef>* Existing = Defs.Find(FName(*Name)))
			{
				const FSlaterPos& At = (*Existing)->Pos;
				Error(NamePos, FString::Printf(TEXT("template '%s' is already defined (at %d,%d%s)"), *Name, At.Line, At.Column, At.File.IsNone() ? TEXT("") : *FString::Printf(TEXT(" in %s"), *At.File.ToString())));
				return;
			}

			const TSharedRef<FTemplateDef> Def = MakeShared<FTemplateDef>();
			Def->Name = Name;
			Def->Pos = Node.Pos;
			TArray<FString> Words;
			ParamText.ParseIntoArrayWS(Words);
			for (const FString& Word : Words)
			{
				if (!IsParamName(Word))
				{
					Error(NamePos, FString::Printf(TEXT("'%s' is not a parameter name: letters, digits and '_', starting with a letter"), *Word));
				}
				else if (Def->Params.Contains(Word))
				{
					Error(NamePos, FString::Printf(TEXT("parameter '%s' is listed twice"), *Word));
				}
				else
				{
					Def->Params.Add(Word);
				}
			}
			if (!Node.Text.IsEmpty())
			{
				Error(Node.TextPos, TEXT("<template> holds one element, not text"));
			}
			if (Node.Children.Num() != 1)
			{
				Error(Node.Pos, FString::Printf(TEXT("<template name=\"%s\"> needs exactly one child element (its body); this has %d"), *Name, Node.Children.Num()));
				return;
			}
			Def->Body = Node.Children[0];
			CheckBody(Def->Body, *Def);
			Defs.Add(FName(*Name), Def);
		}

		/** Every `$name` in the body must be a parameter. Reported at the definition, used or not. */
		void CheckBody(const FSlaterNode& Node, const FTemplateDef& Def)
		{
			FString Unused;
			FString Problem;
			for (const FSlaterAttribute& Attr : Node.Attributes)
			{
				if (!Substitute(Attr.Value, Def.Params, nullptr, Unused, Problem))
				{
					Error(Attr.Pos, Problem);
				}
			}
			if (!Substitute(Node.Text, Def.Params, nullptr, Unused, Problem))
			{
				Error(Node.TextPos, Problem);
			}
			for (const FSlaterNode& Child : Node.Children)
			{
				CheckBody(Child, Def);
			}
		}

		/** Replaces the uses among Parent's children (and, inside them, deeper ones). A slot element of a host (`<cell>` in `<inventory-grid>`) is not a use even if a template has that name. */
		void ExpandChildren(FSlaterNode& Parent, TArray<FString>& Stack)
		{
			const FSlaterElementDesc* ParentDesc = Registry.Find(FName(*Parent.Tag, FNAME_Find));
			TArray<FSlaterNode> Result;
			for (FSlaterNode& Child : Parent.Children)
			{
				const bool bSlotElement = ParentDesc && ParentDesc->TemplateSlots.ContainsByPredicate([&](const FSlaterTemplateSlotDesc& Slot) { return Slot.Name.ToString() == Child.Tag; });
				const TSharedRef<FTemplateDef>* Def = bSlotElement ? nullptr : Defs.Find(FName(*Child.Tag, FNAME_Find));
				if (!Def)
				{
					ExpandChildren(Child, Stack);
					Result.Add(MoveTemp(Child));
					continue;
				}
				FSlaterNode Expanded;
				if (ExpandUse(**Def, Child, Stack, Expanded))
				{
					Result.Add(MoveTemp(Expanded));
				}
			}
			Parent.Children = MoveTemp(Result);
		}

		bool ExpandUse(const FTemplateDef& Def, const FSlaterNode& Use, TArray<FString>& Stack, FSlaterNode& Out)
		{
			if (Stack.Contains(Def.Name))
			{
				Error(Use.Pos, FString::Printf(TEXT("template '%s' uses itself: %s -> %s"), *Def.Name, *FString::Join(Stack, TEXT(" -> ")), *Def.Name));
				return false;
			}
			if (Stack.Num() >= MaxNesting)
			{
				Error(Use.Pos, FString::Printf(TEXT("templates are nested more than %d deep"), MaxNesting));
				return false;
			}
			NodesUsed += CountNodes(Def.Body);
			if (NodesUsed > Limits.MaxNodes)
			{
				if (!bBudgetReported)
				{
					bBudgetReported = true;
					Error(Use.Pos, FString::Printf(TEXT("the templates expand to more than %d elements"), Limits.MaxNodes));
				}
				return false;
			}

			bool bOk = true;
			TMap<FString, FString> Args;
			TMap<FString, FSlaterPos> ArgPos;
			TArray<const FSlaterAttribute*> Passed;
			for (const FSlaterAttribute& Attr : Use.Attributes)
			{
				if (Def.Params.Contains(Attr.Name))
				{
					Args.Add(Attr.Name, Attr.Value);
					ArgPos.Add(Attr.Name, Attr.Pos);
				}
				else if (IsPassThrough(Attr.Name))
				{
					Passed.Add(&Attr);
				}
				else
				{
					TArray<FString> Names = Def.Params;
					Names.Append({TEXT("id"), TEXT("fill"), TEXT("margin"), TEXT("halign"), TEXT("valign")});
					const FString Nearest = Slater::SuggestNearest(Attr.Name, Names);
					Error(Attr.Pos, FString::Printf(TEXT("unknown parameter '%s' on <%s>%s; parameters: %s (and id, fill, margin, halign, valign for the slot)"),
						*Attr.Name, *Def.Name, Nearest.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; did you mean '%s'?"), *Nearest), Def.Params.Num() > 0 ? *FString::Join(Def.Params, TEXT(", ")) : TEXT("none")));
					bOk = false;
				}
			}
			for (const FString& Param : Def.Params)
			{
				if (!Args.Contains(Param))
				{
					Error(Use.Pos, FString::Printf(TEXT("<%s> needs the attribute '%s', for example %s=\"...\""), *Def.Name, *Param, *Param));
					bOk = false;
				}
			}
			if (Use.Children.Num() > 0)
			{
				Error(Use.Children[0].Pos, FString::Printf(TEXT("<%s> is a template and takes no child elements"), *Def.Name));
				bOk = false;
			}
			if (!Use.Text.IsEmpty())
			{
				Error(Use.TextPos, FString::Printf(TEXT("<%s> is a template and takes no text"), *Def.Name));
				bOk = false;
			}
			if (!bOk)
			{
				return false;
			}

			FSlaterNode Body = Def.Body;
			Fill(Body, Def, Args, ArgPos);
			FSlaterNode Holder;
			Holder.Children.Add(MoveTemp(Body));
			Stack.Add(Def.Name);
			ExpandChildren(Holder, Stack);
			Stack.Pop();
			if (Holder.Children.Num() != 1)
			{
				return false;
			}
			Out = MoveTemp(Holder.Children[0]);
			for (const FSlaterAttribute* Attr : Passed)
			{
				if (FSlaterAttribute* Existing = Out.Attributes.FindByPredicate([&](const FSlaterAttribute& A) { return A.Name == Attr->Name; }))
				{
					*Existing = *Attr;
				}
				else
				{
					Out.Attributes.Add(*Attr);
				}
			}
			return true;
		}

		/** Substitutes the arguments. A value that is exactly `$name` is reported where the use wrote it; a mixed one at the body. */
		void Fill(FSlaterNode& Node, const FTemplateDef& Def, const TMap<FString, FString>& Args, const TMap<FString, FSlaterPos>& ArgPos)
		{
			FString Problem;
			for (FSlaterAttribute& Attr : Node.Attributes)
			{
				if (Attr.Value.StartsWith(TEXT("$")) && Def.Params.Contains(Attr.Value.Mid(1)))
				{
					Attr.Pos = ArgPos.FindRef(Attr.Value.Mid(1));
				}
				FString Out;
				Substitute(Attr.Value, Def.Params, &Args, Out, Problem);
				Attr.Value = MoveTemp(Out);
			}
			if (!Node.Text.IsEmpty())
			{
				if (Node.Text.StartsWith(TEXT("$")) && Def.Params.Contains(Node.Text.Mid(1)))
				{
					Node.TextPos = ArgPos.FindRef(Node.Text.Mid(1));
				}
				FString Out;
				Substitute(Node.Text, Def.Params, &Args, Out, Problem);
				Node.Text = MoveTemp(Out);
			}
			for (FSlaterNode& Child : Node.Children)
			{
				Fill(Child, Def, Args, ArgPos);
			}
		}
	};
}
using namespace SlaterTemplatesPrivate;

namespace Slater
{
	FSlaterImportResolver LayoutImportResolver()
	{
		return [](const FString& Id, FString& OutText, FString& OutLabel, FString& OutProblem) -> bool
		{
			OutLabel = FString::Printf(TEXT("UI/Layouts/%s.%s"), *Id, LayoutFileExtension());
			if (!IsValidLayoutId(Id))
			{
				OutProblem = TEXT("not a layout ID: letters, digits, '_' and '-', joined by '/'");
				return false;
			}
			const FString File = FindLayoutFile(Id);
			if (File.IsEmpty())
			{
				TArray<FString> Roots;
				GetLayoutRoots(Roots);
				OutProblem = FString::Printf(TEXT("no such file; searched %s"), *FString::Join(Roots, TEXT(", ")));
				return false;
			}
			const FSlaterLimits Limits;
			if (IFileManager::Get().FileSize(*File) > static_cast<int64>(Limits.MaxChars) * 4)
			{
				OutProblem = FString::Printf(TEXT("the file is larger than %d characters"), Limits.MaxChars);
				return false;
			}
			if (!FFileHelper::LoadFileToString(OutText, *File))
			{
				OutProblem = TEXT("the file could not be read");
				return false;
			}
			return true;
		};
	}

	bool HasTemplateDeclarations(const FSlaterNode& Root)
	{
		return Root.Children.ContainsByPredicate([](const FSlaterNode& Child) { return Child.Tag == TagTemplate || Child.Tag == TagImport; });
	}

	bool ExpandTemplates(FSlaterNode& Root, const FString& Path, const FSlaterElementRegistry& Registry, const FSlaterImportResolver& Resolver, TArray<FSlaterError>& OutErrors, const FSlaterLimits& Limits)
	{
		FExpander Expander(Path, Registry, Resolver, OutErrors, Limits);
		return Expander.Run(Root);
	}
}
