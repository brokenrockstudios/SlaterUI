// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SSlaterView;
class SWidget;

/**
 * What a command handler is told: which command, the widget that fired it, and the one word written after a colon
 * (`on-click="Inventory.Use:Sword"` gives Name `Inventory.Use` and Argument `Sword`; empty when there is none).
 * A command named by an element event (`on-change="Audio.SetVolume"`, T-381) also gets the control's new value.
 */
struct FSlaterCommandCall
{
	FName Name;
	TSharedPtr<SWidget> Source;
	FString Argument;
	/** The value an element event carries, as text (`0.25`, `true`, the committed text); empty for a click. */
	FString Value;
};

using FSlaterCommandHandler = TFunction<void(const FSlaterCommandCall&)>;

/**
 * Commands by name, the seam between markup and code: `on-click="Inventory.Close"` names an entry here and nothing
 * else (no expressions; at most one word of argument after a colon). A view has its own table that wins over the
 * process-wide one. Names starting with `ui.` are the stock commands (below) and cannot be registered. Game thread only.
 */
class SLATERUI_API FSlaterCommandTable
{
public:
	/** False, with a reason in OutError, for a bad name, a stock (`ui.`) name, or one that is already registered. */
	bool Register(FName Name, FSlaterCommandHandler Handler, FString* OutError = nullptr);
	bool Unregister(FName Name);
	bool Contains(FName Name) const { return Handlers.Contains(Name); }
	/** Runs the handler; false when the name is not registered. */
	bool Run(const FSlaterCommandCall& Call) const;
	void GetNames(TArray<FString>& OutNames) const;

private:
	TMap<FName, FSlaterCommandHandler> Handlers;
};

/**
 * What the stock commands `ui.push`, `ui.pop` and `ui.close` ask of the game: it owns the stack of screens, SlaterUI only
 * shows one layout in a view. Set one with `Slater::SetScreenHost`. Game thread only.
 */
class ISlaterScreenHost
{
public:
	virtual ~ISlaterScreenHost() = default;
	/** Show the layout on top of the current screen (`ui.push:Inventory`). From is the view whose button asked. */
	virtual void PushScreen(const FString& LayoutId, SSlaterView& From) = 0;
	/** Remove the top screen (`ui.pop`). */
	virtual void PopScreen(SSlaterView& From) = 0;
	/** Remove the screen this view is (`ui.close`). */
	virtual void CloseScreen(SSlaterView& View) = 0;
};

namespace Slater
{
	/** The process-wide command table. */
	SLATERUI_API FSlaterCommandTable& Commands();

	/** Runs the call in Table and logs a warning when no handler is registered. False in that case. */
	SLATERUI_API bool RunCommand(const FSlaterCommandTable& Table, const FSlaterCommandCall& Call);

	/** Words of letters, digits and `_` (not starting with a digit) joined by dots: `Close`, `Inventory.Close`. */
	SLATERUI_API bool IsValidCommandName(const FString& Name);

	/** Splits `Name:Argument` at the first colon; no colon gives the whole text as the name and an empty argument. */
	SLATERUI_API void SplitCommandText(const FString& Text, FString& OutName, FString& OutArgument);

	/**
	 * What is wrong with the text of an `on-click` (`Name` or `Name:Argument`), or empty when it is fine. A stock command is
	 * checked for its argument too: `ui.close` and `ui.pop` take none; `ui.push` takes a layout ID, `ui.toggle` a state name,
	 * `ui.style` a style name, `ui.sound` a sound token.
	 */
	SLATERUI_API FString CheckCommandText(const FString& Text);

	/** The stock commands, run by every view without registration: `ui.close ui.push ui.pop ui.toggle ui.sound ui.style`. */
	SLATERUI_API bool IsStockCommand(FName Name);
	SLATERUI_API void GetStockCommands(TArray<FString>& OutNames);

	/**
	 * Runs a stock command for a view: `ui.close`/`ui.pop`/`ui.push:Id` through the screen host, `ui.toggle:key` flips a state
	 * between `true` and `false`, `ui.style:Name` restyles this view, `ui.sound:Token` goes to the sound player. False, with a
	 * warning, when it could not run (no host, an unknown style or state, no sound player).
	 */
	SLATERUI_API bool RunStockCommand(SSlaterView& View, const FSlaterCommandCall& Call);

	/** The game's screen stack for `ui.push`, `ui.pop` and `ui.close`; held weakly. Null clears it. */
	SLATERUI_API void SetScreenHost(const TSharedPtr<ISlaterScreenHost>& Host);

	/** What plays a sound for `ui.sound:Token`; an empty function clears it. SlaterUI knows nothing about audio. */
	using FSlaterSoundPlayer = TFunction<void(FName Token)>;
	SLATERUI_API void SetSoundPlayer(FSlaterSoundPlayer Player);
}
