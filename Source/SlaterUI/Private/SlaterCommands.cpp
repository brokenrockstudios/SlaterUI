// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterCommands.h"

#include "SlaterState.h"
#include "SlaterView.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterCommands, Log, All);

bool Slater::RunCommand(const FSlaterCommandTable& Table, const FSlaterCommandCall& Call)
{
	if (Table.Run(Call))
	{
		return true;
	}
	UE_LOG(LogSlaterCommands, Warning, TEXT("SlaterUI: no handler is registered for the command '%s'"), *Call.Name.ToString());
	return false;
}

namespace SlaterCommandsPrivate
{
	enum class EStockArg : uint8
	{
		None,
		LayoutId,
		StateName,
		StyleName,
		SoundToken,
	};

	struct FStockCommand
	{
		const TCHAR* Name;
		EStockArg Arg;
	};

	const FStockCommand StockCommands[] = {
		{TEXT("ui.close"), EStockArg::None},
		{TEXT("ui.push"), EStockArg::LayoutId},
		{TEXT("ui.pop"), EStockArg::None},
		{TEXT("ui.toggle"), EStockArg::StateName},
		{TEXT("ui.sound"), EStockArg::SoundToken},
		{TEXT("ui.style"), EStockArg::StyleName},
	};

	const FStockCommand* FindStock(const FString& Name)
	{
		for (const FStockCommand& Stock : StockCommands)
		{
			if (Name.Equals(Stock.Name, ESearchCase::CaseSensitive))
			{
				return &Stock;
			}
		}
		return nullptr;
	}

	/** Letters, digits and `_ . / -`: enough for a layout ID, a style name, a state name or a sound token. */
	bool IsArgumentWord(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return false;
		}
		for (const TCHAR C : Text)
		{
			if (!(C < 128 && (FChar::IsAlnum(C) || C == '_' || C == '.' || C == '/' || C == '-')))
			{
				return false;
			}
		}
		return true;
	}

	bool IsReservedName(FName Name)
	{
		return Name.ToString().StartsWith(TEXT("ui."), ESearchCase::CaseSensitive);
	}
}
using namespace SlaterCommandsPrivate;

void Slater::SplitCommandText(const FString& Text, FString& OutName, FString& OutArgument)
{
	if (!Text.Split(TEXT(":"), &OutName, &OutArgument))
	{
		OutName = Text;
		OutArgument.Reset();
	}
}

FString Slater::CheckCommandText(const FString& Text)
{
	FString Name, Argument;
	SplitCommandText(Text, Name, Argument);
	if (!IsValidCommandName(Name))
	{
		return FString::Printf(TEXT("'%s' is not a command name: words of letters, digits and '_' joined by '.', like Inventory.Close (one word of argument may follow a ':', like Inventory.Use:Sword)"), *Text);
	}
	const bool bHasColon = Text.Contains(TEXT(":"));
	const FStockCommand* Stock = FindStock(Name);
	if (IsReservedName(FName(*Name)) && !Stock)
	{
		TArray<FString> Names;
		GetStockCommands(Names);
		return FString::Printf(TEXT("'%s' is not a stock command; names starting with 'ui.' are reserved for these: %s"), *Name, *FString::Join(Names, TEXT(", ")));
	}
	if (bHasColon && !IsArgumentWord(Argument))
	{
		return FString::Printf(TEXT("'%s' needs one word after the ':' (letters, digits and '_', '.', '/', '-')"), *Text);
	}
	if (Stock)
	{
		if (Stock->Arg == EStockArg::None && bHasColon)
		{
			return FString::Printf(TEXT("%s takes no argument; write on-click=\"%s\""), *Name, *Name);
		}
		if (Stock->Arg != EStockArg::None && !bHasColon)
		{
			const TCHAR* Example = Stock->Arg == EStockArg::LayoutId ? TEXT("Inventory") : Stock->Arg == EStockArg::StateName ? TEXT("tab") : Stock->Arg == EStockArg::StyleName ? TEXT("Dusk") : TEXT("UI.Click");
			return FString::Printf(TEXT("%s needs an argument after a ':', for example on-click=\"%s:%s\""), *Name, *Name, Example);
		}
		if (Stock->Arg == EStockArg::LayoutId && !IsValidLayoutId(Argument))
		{
			return FString::Printf(TEXT("'%s' is not a layout ID: letters, digits, '_' and '-', joined by '/'"), *Argument);
		}
		if (Stock->Arg == EStockArg::StateName && !IsValidStateName(Argument))
		{
			return FString::Printf(TEXT("'%s' is not a state name: letters, digits and '_', not starting with a digit"), *Argument);
		}
	}
	return FString();
}

bool Slater::IsStockCommand(FName Name)
{
	return FindStock(Name.ToString()) != nullptr;
}

void Slater::GetStockCommands(TArray<FString>& OutNames)
{
	OutNames.Reset();
	for (const FStockCommand& Stock : StockCommands)
	{
		OutNames.Add(Stock.Name);
	}
}

bool Slater::IsValidCommandName(const FString& Name)
{
	bool bWordStart = true;
	for (const TCHAR C : Name)
	{
		if (C == '.')
		{
			if (bWordStart)
			{
				return false;
			}
			bWordStart = true;
		}
		else if (C < 128 && (FChar::IsAlpha(C) || C == '_'))
		{
			bWordStart = false;
		}
		else if (C < 128 && FChar::IsDigit(C) && !bWordStart)
		{
		}
		else
		{
			return false;
		}
	}
	return !bWordStart;
}

FSlaterCommandTable& Slater::Commands()
{
	static FSlaterCommandTable Table;
	return Table;
}

bool FSlaterCommandTable::Register(FName Name, FSlaterCommandHandler Handler, FString* OutError)
{
	auto Fail = [&](const FString& Reason)
	{
		if (OutError)
		{
			*OutError = Reason;
		}
		return false;
	};
	if (!Slater::IsValidCommandName(Name.ToString()))
	{
		return Fail(FString::Printf(TEXT("'%s' is not a command name: words of letters, digits and '_' joined by '.', like Inventory.Close"), *Name.ToString()));
	}
	if (!Handler)
	{
		return Fail(FString::Printf(TEXT("command '%s' has no handler"), *Name.ToString()));
	}
	if (IsReservedName(Name))
	{
		return Fail(FString::Printf(TEXT("'%s' starts with 'ui.', which is reserved for the stock commands (ui.close, ui.push, ...); pick another name"), *Name.ToString()));
	}
	if (Handlers.Contains(Name))
	{
		return Fail(FString::Printf(TEXT("command '%s' is already registered"), *Name.ToString()));
	}
	Handlers.Add(Name, MoveTemp(Handler));
	return true;
}

bool FSlaterCommandTable::Unregister(FName Name)
{
	return Handlers.Remove(Name) > 0;
}

bool FSlaterCommandTable::Run(const FSlaterCommandCall& Call) const
{
	const FSlaterCommandHandler* Handler = Handlers.Find(Call.Name);
	if (!Handler)
	{
		return false;
	}
	// A copy, so a handler may unregister itself.
	const FSlaterCommandHandler Copy = *Handler;
	Copy(Call);
	return true;
}

void FSlaterCommandTable::GetNames(TArray<FString>& OutNames) const
{
	OutNames.Reset();
	for (const TPair<FName, FSlaterCommandHandler>& Pair : Handlers)
	{
		OutNames.Add(Pair.Key.ToString());
	}
	OutNames.Sort();
}
