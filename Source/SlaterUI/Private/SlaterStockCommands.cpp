// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterCommands.h"

#include "SlaterView.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterStock, Log, All);

namespace
{
	TWeakPtr<ISlaterScreenHost>& ScreenHost()
	{
		static TWeakPtr<ISlaterScreenHost> Host;
		return Host;
	}

	Slater::FSlaterSoundPlayer& SoundPlayer()
	{
		static Slater::FSlaterSoundPlayer Player;
		return Player;
	}

	bool NoHost(const FSlaterCommandCall& Call)
	{
		UE_LOG(LogSlaterStock, Warning, TEXT("SlaterUI: %s needs a screen host and none is set (Slater::SetScreenHost); the game owns the stack of screens"), *Call.Name.ToString());
		return false;
	}
}

void Slater::SetScreenHost(const TSharedPtr<ISlaterScreenHost>& Host)
{
	ScreenHost() = Host;
}

void Slater::SetSoundPlayer(FSlaterSoundPlayer Player)
{
	SoundPlayer() = MoveTemp(Player);
}

bool Slater::RunStockCommand(SSlaterView& View, const FSlaterCommandCall& Call)
{
	const FString Name = Call.Name.ToString();
	if (Name == TEXT("ui.close") || Name == TEXT("ui.pop") || Name == TEXT("ui.push"))
	{
		const TSharedPtr<ISlaterScreenHost> Host = ScreenHost().Pin();
		if (!Host.IsValid())
		{
			return NoHost(Call);
		}
		if (Name == TEXT("ui.close"))
		{
			Host->CloseScreen(View);
		}
		else if (Name == TEXT("ui.pop"))
		{
			Host->PopScreen(View);
		}
		else
		{
			Host->PushScreen(Call.Argument, View);
		}
		return true;
	}
	if (Name == TEXT("ui.toggle"))
	{
		FString Current;
		const FName Key(*Call.Argument);
		if (!View.GetState(Key, Current))
		{
			UE_LOG(LogSlaterStock, Warning, TEXT("SlaterUI: ui.toggle names the state '%s', which the layout '%s' does not declare"), *Call.Argument, *View.GetLayoutId());
			return false;
		}
		return View.SetState(Key, Current.Equals(TEXT("true"), ESearchCase::CaseSensitive) ? TEXT("false") : TEXT("true"));
	}
	if (Name == TEXT("ui.style"))
	{
		FSlaterStyleStack Scratch;
		if (!Slater::StyleLibrary().BuildStack(FName(*Call.Argument), Scratch))
		{
			UE_LOG(LogSlaterStock, Warning, TEXT("SlaterUI: ui.style names the style '%s', which no style file defines"), *Call.Argument);
			return false;
		}
		return View.SetStyle(FName(*Call.Argument));
	}
	if (Name == TEXT("ui.sound"))
	{
		if (!SoundPlayer())
		{
			UE_LOG(LogSlaterStock, Warning, TEXT("SlaterUI: ui.sound:%s needs a sound player and none is set (Slater::SetSoundPlayer)"), *Call.Argument);
			return false;
		}
		SoundPlayer()(FName(*Call.Argument));
		return true;
	}
	UE_LOG(LogSlaterStock, Warning, TEXT("SlaterUI: '%s' is not a stock command"), *Name);
	return false;
}
