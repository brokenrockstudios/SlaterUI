// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlaterBindings.h"
#include "SlaterCommands.h"
#include "SlaterElements.h"
#include "SlaterScope.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FSlaterBinder;
class FSlaterModel;

namespace Slater
{
	/** File extension of layout files, without the dot. */
	inline const TCHAR* LayoutFileExtension() { return TEXT("sltr"); }

	/** Letters, digits, underscore and hyphen in segments joined by a slash (`Inventory`, `Hud/Health`). No dots, so no extension and no `..`. */
	SLATERUI_API bool IsValidLayoutId(const FString& Id);

	/** Folders layouts are searched in, highest priority first: the project's `Content/UI/Layouts`, then this plugin's. */
	SLATERUI_API void GetLayoutRoots(TArray<FString>& OutRoots);

	/** The file for a layout ID in the first root that has it, or empty (also for an invalid ID). */
	SLATERUI_API FString FindLayoutFile(const FString& Id);

	/**
	 * Reads one layout file and builds it. The file size is checked against the character cap before anything is read,
	 * so an oversized file is refused unread. Every failure (missing, too large, unreadable, syntax, build) lands in
	 * Out.Errors in the usual `Label(Line,Column): message` form. Label is only text for those errors.
	 */
	SLATERUI_API bool LoadLayoutFile(const FString& FilePath, const FString& Label, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget = FSlaterBudget(), const FSlaterElementRegistry& Registry = Elements(), const FSlaterLimits& Limits = FSlaterLimits());

	/** `FindLayoutFile`, then `LoadLayoutFile`. An ID with no file is an error naming the folders searched. */
	SLATERUI_API bool LoadLayout(const FString& Id, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget = FSlaterBudget(), const FSlaterElementRegistry& Registry = Elements(), const FSlaterLimits& Limits = FSlaterLimits());

	/**
	 * The styles every view uses: this plugin's `Base` and `Dusk` plus any `*.sltrstyle` in the project's
	 * `Content/UI/Styles`, loaded on first use. Game thread only. Problems are logged once and the styles that did load stay.
	 */
	SLATERUI_API const FSlaterStyleLibrary& StyleLibrary();

	/**
	 * Extra folders searched before the project's and the plugin's: layout roots for `.sltr` files, style roots for
	 * `.sltrstyle` files (loaded after the project's; style names are unique, so a clash is an error).
	 * For tests and, later, mods (T-347). A style root takes effect at the next `ReloadStyles`. Game thread only.
	 */
	SLATERUI_API void AddLayoutRoot(const FString& Directory);
	SLATERUI_API void RemoveLayoutRoot(const FString& Directory);
	SLATERUI_API void AddStyleRoot(const FString& Directory);
	SLATERUI_API void RemoveStyleRoot(const FString& Directory);

	/**
	 * Reads every style folder again. All or nothing: when any file has a problem the library in use is kept and the
	 * problems are returned (and logged), so a half-saved style never reaches a view. Views that hold a stack built
	 * from the old library keep it until they refresh.
	 */
	SLATERUI_API bool ReloadStyles(TArray<FSlaterError>& OutErrors);

	/**
	 * The problems of the last `ReloadStyles` if it was refused, empty if it worked (or none ran yet). Valid when
	 * `OnContentChanged` fires, so a tool can show why a saved style file did not take effect.
	 */
	SLATERUI_API const TArray<FSlaterError>& GetStyleReloadErrors();

	/** Fires after layouts or styles changed on disk (see `NotifyFilesChanged`). Views listen and refresh. */
	DECLARE_MULTICAST_DELEGATE(FOnContentChanged);
	SLATERUI_API FOnContentChanged& OnContentChanged();

	/**
	 * Tells SlaterUI that these files changed on disk (full paths, as a directory watcher reports them). Files that
	 * are not `.sltr` or `.sltrstyle` are ignored. A changed style file reloads the library first. Then
	 * `OnContentChanged` fires once. Returns false when nothing relevant changed or the style reload was refused.
	 */
	SLATERUI_API bool NotifyFilesChanged(const TArray<FString>& Files);

	/** Every layout ID found in the layout roots, sorted, without duplicates. For pickers. */
	SLATERUI_API void GetLayoutIds(TArray<FString>& OutIds);
}

/**
 * Shows a layout from `Content/UI/Layouts` in a style. A layout that fails to load shows its errors instead, in text,
 * so a broken file is visible where it is used.
 *
 * Three seams connect it to code: a context (`SetContext`, the data `{Path}` reads, checked when the layout is built so
 * an unresolvable path fails with its line), outlets (`SetOutlet` puts a widget of your own into an `<outlet>`, and keeps
 * it there through reloads), and commands (`GetCommands`, what `on-click` names).
 */
class SLATERUI_API SSlaterView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSlaterView)
		: _StyleName(TEXT("Base"))
		, _UseMockContext(false)
	{}
		/** The layout ID (`Panel`, `Hud/Health`). */
		SLATE_ARGUMENT(FString, LayoutId)
		SLATE_ARGUMENT(FName, StyleName)
		/** Bind the layout to `<Layout>.mock.json` next to it (the preview does) unless a context is set. */
		SLATE_ARGUMENT(bool, UseMockContext)
		/** The data `{Path}` reads, in place for the first build (as `SetContext` before it, without the second build). */
		SLATE_ARGUMENT(TSharedPtr<ISlaterScope>, Context)
	SLATE_END_ARGS()

	virtual ~SSlaterView() override;

	void Construct(const FArguments& InArgs);

	/** Builds another layout in the current style. False when it failed (the errors are shown). */
	bool SetLayout(const FString& InLayoutId);
	/** Rebuilds in another style. False when the style is unknown or the layout then failed. */
	bool SetStyle(FName InStyleName);
	/** Reads the file again. A failure replaces the tree with the errors. */
	bool Reload();

	/**
	 * Reads the file and the style again like `Reload`, but a failure keeps the tree that was showing and lays the
	 * errors over it as a banner (`GetReloadErrors`). With no good tree to keep it is the same as `Reload`. This is
	 * what a changed file on disk calls (`Slater::OnContentChanged`).
	 */
	bool Refresh();

	/** True when the tree on screen failed to build (nothing good to show). */
	bool HasErrors() const { return !Result.Succeeded(); }
	/** The last `Refresh` failed and the previous good tree is still showing. */
	bool HasReloadErrors() const { return ReloadErrors.Num() > 0; }
	const TArray<FSlaterError>& GetReloadErrors() const { return ReloadErrors; }
	/** How many times a refresh replaced the tree. For tests and the preview. */
	int32 GetBuildCount() const { return BuildCount; }
	const FSlaterBuildResult& GetResult() const { return Result; }
	const FString& GetLayoutId() const { return LayoutId; }
	FName GetStyleName() const { return StyleName; }
	TSharedPtr<SWidget> FindById(FName Id) const;

	/**
	 * Sets the data `{Path}` bindings read (SlaterObjectScope.h for a view model) and rebuilds. Every path is checked
	 * against it first: an unresolvable one fails the build with its line, like any layout error (the last good tree
	 * stays, with the banner, if there is one). Null removes the context. False when the build failed.
	 */
	bool SetContext(TSharedPtr<ISlaterScope> InContext);
	const TSharedPtr<ISlaterScope>& GetContext() const { return ExplicitContext; }

	/** Binds to the mock file next to the layout when there is no explicit context (a missing file means no data). */
	void UseMockContext(bool bEnable);
	/** The context the tree on screen is bound to: the explicit one, else the mock, else null. */
	TSharedPtr<ISlaterScope> GetActiveContext() const { return ActiveContext; }

	/**
	 * Puts Content into the `<outlet id=Id>` (null brings the outlet's default content back). The view keeps the content
	 * and puts it into the next tree after a reload, a style change or a refresh. False when the layout has no such outlet
	 * now (the content is kept for a layout that has).
	 */
	bool SetOutlet(FName Id, TSharedPtr<SWidget> Content);
	TSharedPtr<SWidget> GetOutletContent(FName Id) const;

	/** What `on-click` names, for this view; a name found here wins over the process-wide `Slater::Commands()`. */
	FSlaterCommandTable& GetCommands() { return Commands; }
	/** Runs a command: this view's table, then the process-wide one. Logs a warning and returns false when neither has it. */
	bool RunCommand(const FSlaterCommandCall& Call);

	/**
	 * The layout's `<state>`s (T-353): text values local to this view, changed by `set-state` on a surface, `ui.toggle`, or
	 * `SetState`, and followed by every `visible-when`. A value survives a reload, a refresh and a restyle when the layout
	 * still declares the name; a new name starts at its default.
	 */
	bool GetState(FName Key, FString& OutValue) const;
	/** False, with a warning, when the layout does not declare Key. Shows or collapses the `visible-when` elements. */
	bool SetState(FName Key, const FString& Value);
	const TMap<FName, FString>& GetStates() const { return State; }

	/**
	 * Moves the `transition`s that are playing on by DeltaTime seconds (T-406). A view with a running transition registers an
	 * active timer that calls this every frame and stops itself when nothing moves, so an idle screen costs nothing; a test
	 * calls it by hand as a manual clock. True while something is still moving.
	 */
	bool AdvanceMotion(float DeltaTime);
	/** A transition is playing. */
	bool IsMoving() const;

	/** The binder of the tree on screen (its `GetBindings()` lists the layout's bindings), or null with no context. */
	const FSlaterBinder* GetBinder() const { return Binder.Get(); }

	/** Stop listening to `Slater::OnContentChanged` (a view that wants to be driven by hand, as tests do). */
	void SetAutoRefresh(bool bEnabled);

private:
	struct FGeneration;

	FString LayoutId;
	FName StyleName;
	/** Shared because repeated items and the binder point at it; a rebuild makes a new one. */
	TSharedRef<FSlaterStyleStack> Stack = MakeShared<FSlaterStyleStack>();
	FSlaterBuildResult Result;
	TArray<FSlaterError> ReloadErrors;
	int32 BuildCount = 0;
	FDelegateHandle ContentChangedHandle;

	TSharedPtr<ISlaterScope> ExplicitContext;
	bool bUseMockContext = false;
	/** What the tree on screen is bound to, and what keeps it alive. */
	TSharedPtr<ISlaterScope> ActiveContext;
	TSharedPtr<FSlaterModel> MockModel;
	TSharedPtr<FSlaterBinder> Binder;
	TMap<FName, TSharedPtr<SWidget>> OutletContent;
	FSlaterCommandTable Commands;
	/** Commands already warned about as having no handler, so a button clicked ten times warns once. */
	TSet<FName> WarnedMissingCommands;
	TMap<FName, FString> State;
	/** The active timer that feeds `AdvanceMotion` is registered; it unregisters itself when nothing moves. */
	bool bMotionTimerRunning = false;

	/** Loads the layout against InStack, binds it to the context, and checks every path. False with the errors in OutErrors. */
	bool TryBuild(const TSharedRef<FSlaterStyleStack>& InStack, FGeneration& OutBuilt, TArray<FSlaterError>& OutErrors) const;
	/** Makes a good build the one on screen. */
	void Install(const TSharedRef<FSlaterStyleStack>& InStack, FGeneration&& Built);
	/** bAnimate false snaps every size (a build, a reload, a restyle); true lets a `transition` play. */
	void ApplyStateConditions(bool bAnimate);
	void EnsureMotionTimer();
	EActiveTimerReturnType TickMotion(double CurrentTime, float DeltaTime);
	void DetachOutlets();
	void ApplyOutlets();

	TSharedRef<SWidget> MakeErrorView(const TArray<FSlaterError>& Errors) const;
};
