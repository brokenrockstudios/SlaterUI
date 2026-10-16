// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "SlaterView.h"

#include "SlaterList.h"
#include "SlaterModel.h"
#include "SlaterOutlet.h"
#include "SlaterPopup.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlaterUI, Log, All);

namespace
{
	void AddError(FSlaterBuildResult& Out, const FString& Path, const FString& Message)
	{
		FSlaterError& Error = Out.Errors.AddDefaulted_GetRef();
		Error.Path = Path;
		Error.Message = Message;
	}

	TArray<FString>& ExtraLayoutRoots()
	{
		static TArray<FString> Roots;
		return Roots;
	}

	TArray<FString>& ExtraStyleRoots()
	{
		static TArray<FString> Roots;
		return Roots;
	}

	/** The one library, replaced in place by a reload so a reference from `StyleLibrary()` stays valid. */
	FSlaterStyleLibrary& MutableLibrary()
	{
		static FSlaterStyleLibrary* Library = new FSlaterStyleLibrary();
		return *Library;
	}

	bool bLibraryLoaded = false;

	/** The problems of the last refused style reload; empty after a reload that worked. */
	TArray<FSlaterError>& LastStyleErrors()
	{
		static TArray<FSlaterError> Errors;
		return Errors;
	}

	/** Plugin styles, the project's `Content/UI/Styles`, then the extra roots, into an empty library. */
	void FillLibrary(FSlaterStyleLibrary& Library, TArray<FSlaterError>& Errors)
	{
		Library.LoadPluginStyles(Errors);
		TArray<FString> Directories;
		Directories.Add(FPaths::ProjectContentDir() / TEXT("UI/Styles"));
		Directories.Append(ExtraStyleRoots());
		for (const FString& Directory : Directories)
		{
			if (IFileManager::Get().DirectoryExists(*Directory))
			{
				Library.LoadDirectory(Directory, Errors);
			}
		}
	}

	Slater::FOnContentChanged& ContentChangedDelegate()
	{
		static Slater::FOnContentChanged Delegate;
		return Delegate;
	}

	bool HasExtension(const FString& File, const TCHAR* Extension)
	{
		return FPaths::GetExtension(File).Equals(Extension, ESearchCase::IgnoreCase);
	}
}

void Slater::AddLayoutRoot(const FString& Directory)
{
	ExtraLayoutRoots().AddUnique(FPaths::ConvertRelativePathToFull(Directory));
}

void Slater::RemoveLayoutRoot(const FString& Directory)
{
	ExtraLayoutRoots().Remove(FPaths::ConvertRelativePathToFull(Directory));
}

void Slater::AddStyleRoot(const FString& Directory)
{
	ExtraStyleRoots().AddUnique(FPaths::ConvertRelativePathToFull(Directory));
}

void Slater::RemoveStyleRoot(const FString& Directory)
{
	ExtraStyleRoots().Remove(FPaths::ConvertRelativePathToFull(Directory));
}

Slater::FOnContentChanged& Slater::OnContentChanged()
{
	return ContentChangedDelegate();
}

bool Slater::ReloadStyles(TArray<FSlaterError>& OutErrors)
{
	StyleLibrary();
	FSlaterStyleLibrary Fresh;
	TArray<FSlaterError> Errors;
	FillLibrary(Fresh, Errors);
	Fresh.Validate(Errors);
	if (Errors.Num() > 0)
	{
		for (const FSlaterError& Error : Errors)
		{
			UE_LOG(LogSlaterUI, Warning, TEXT("Style reload refused, keeping the styles in use: %s"), *Error.ToString());
		}
		LastStyleErrors() = Errors;
		OutErrors.Append(Errors);
		return false;
	}
	LastStyleErrors().Reset();
	MutableLibrary() = MoveTemp(Fresh);
	return true;
}

const TArray<FSlaterError>& Slater::GetStyleReloadErrors()
{
	return LastStyleErrors();
}

bool Slater::NotifyFilesChanged(const TArray<FString>& Files)
{
	bool bLayouts = false;
	bool bStyles = false;
	for (const FString& File : Files)
	{
		bLayouts |= HasExtension(File, LayoutFileExtension());
		bStyles |= HasExtension(File, StyleFileExtension());
	}
	if (!bLayouts && !bStyles)
	{
		return false;
	}
	bool bOk = true;
	if (bStyles)
	{
		TArray<FSlaterError> Errors;
		bOk = ReloadStyles(Errors);
	}
	// Views refresh even when the style reload was refused: a layout may have changed too, and a view that cannot
	// read its style keeps what it shows.
	ContentChangedDelegate().Broadcast();
	return bOk;
}

void Slater::GetLayoutIds(TArray<FString>& OutIds)
{
	OutIds.Reset();
	TArray<FString> Roots;
	GetLayoutRoots(Roots);
	for (const FString& Root : Roots)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(Files, *Root, *FString::Printf(TEXT("*.%s"), LayoutFileExtension()), true, false);
		for (const FString& File : Files)
		{
			FString Relative = File;
			FString NormalRoot = Root;
			FPaths::NormalizeFilename(Relative);
			FPaths::NormalizeFilename(NormalRoot);
			if (Relative.StartsWith(NormalRoot))
			{
				Relative.RightChopInline(NormalRoot.Len());
				Relative.RemoveFromStart(TEXT("/"));
				const FString Id = FPaths::ChangeExtension(Relative, TEXT(""));
				if (IsValidLayoutId(Id))
				{
					OutIds.AddUnique(Id);
				}
			}
		}
	}
	OutIds.Sort();
}

bool Slater::IsValidLayoutId(const FString& Id)
{
	bool bSegmentEmpty = true;
	for (const TCHAR C : Id)
	{
		if (C == '/')
		{
			if (bSegmentEmpty)
			{
				return false;
			}
			bSegmentEmpty = true;
		}
		else if ((C < 128 && FChar::IsAlnum(C)) || C == '_' || C == '-')
		{
			bSegmentEmpty = false;
		}
		else
		{
			return false;
		}
	}
	return !bSegmentEmpty;
}

void Slater::GetLayoutRoots(TArray<FString>& OutRoots)
{
	OutRoots.Reset();
	OutRoots.Append(ExtraLayoutRoots());
	OutRoots.Add(FPaths::ProjectContentDir() / TEXT("UI/Layouts"));
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SlaterUI")))
	{
		OutRoots.Add(Plugin->GetContentDir() / TEXT("UI/Layouts"));
	}
}

FString Slater::FindLayoutFile(const FString& Id)
{
	if (!IsValidLayoutId(Id))
	{
		return FString();
	}
	TArray<FString> Roots;
	GetLayoutRoots(Roots);
	for (const FString& Root : Roots)
	{
		const FString Candidate = Root / FString::Printf(TEXT("%s.%s"), *Id, LayoutFileExtension());
		if (IFileManager::Get().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

bool Slater::LoadLayoutFile(const FString& FilePath, const FString& Label, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget, const FSlaterElementRegistry& Registry, const FSlaterLimits& Limits)
{
	Out = FSlaterBuildResult();
	// A UTF-8 character is at most 4 bytes, so a file over MaxChars * 4 bytes is over the cap. Refused before it is read.
	const int64 Size = IFileManager::Get().FileSize(*FilePath);
	if (Size < 0)
	{
		AddError(Out, Label, TEXT("the file does not exist"));
		return false;
	}
	if (Size > static_cast<int64>(Limits.MaxChars) * 4)
	{
		AddError(Out, Label, FString::Printf(TEXT("the file is larger than %d characters"), Limits.MaxChars));
		return false;
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		AddError(Out, Label, TEXT("the file could not be read"));
		return false;
	}

	FSlaterNode Root;
	FSlaterError ParseError;
	if (!Slater::ParseMarkup(Text, Label, Root, ParseError, Limits))
	{
		Slater::AppendSyntaxStopsBuildNote(ParseError);
		Out.Errors.Add(ParseError);
		return false;
	}
	return Slater::BuildLayout(Root, Label, Style, Out, Budget, Registry);
}

bool Slater::LoadLayout(const FString& Id, const ISlaterStyleSource& Style, FSlaterBuildResult& Out, const FSlaterBudget& Budget, const FSlaterElementRegistry& Registry, const FSlaterLimits& Limits)
{
	const FString Label = FString::Printf(TEXT("UI/Layouts/%s.%s"), *Id, LayoutFileExtension());
	if (!IsValidLayoutId(Id))
	{
		Out = FSlaterBuildResult();
		AddError(Out, Label, FString::Printf(TEXT("'%s' is not a layout ID: letters, digits, '_' and '-', joined by '/'"), *Id));
		return false;
	}
	const FString File = FindLayoutFile(Id);
	if (File.IsEmpty())
	{
		TArray<FString> Roots;
		GetLayoutRoots(Roots);
		Out = FSlaterBuildResult();
		AddError(Out, Label, FString::Printf(TEXT("no such layout; searched %s"), *FString::Join(Roots, TEXT(", "))));
		return false;
	}
	return LoadLayoutFile(File, Label, Style, Out, Budget, Registry, Limits);
}

const FSlaterStyleLibrary& Slater::StyleLibrary()
{
	FSlaterStyleLibrary& Library = MutableLibrary();
	if (!bLibraryLoaded)
	{
		bLibraryLoaded = true;
		TArray<FSlaterError> Errors;
		FillLibrary(Library, Errors);
		for (const FSlaterError& Error : Errors)
		{
			UE_LOG(LogSlaterUI, Warning, TEXT("%s"), *Error.ToString());
		}
	}
	return Library;
}

struct SSlaterView::FGeneration
{
	FSlaterBuildResult Result;
	TSharedPtr<ISlaterScope> Context;
	TSharedPtr<FSlaterModel> Mock;
	TSharedPtr<FSlaterBinder> Binder;
};

SSlaterView::~SSlaterView()
{
	SetAutoRefresh(false);
}

void SSlaterView::Construct(const FArguments& InArgs)
{
	StyleName = InArgs._StyleName;
	LayoutId = InArgs._LayoutId;
	bUseMockContext = InArgs._UseMockContext;
	ExplicitContext = InArgs._Context;
	ChildSlot[SNullWidget::NullWidget];
	SetStyle(StyleName);
	SetAutoRefresh(true);
}

void SSlaterView::SetAutoRefresh(bool bEnabled)
{
	if (bEnabled && !ContentChangedHandle.IsValid())
	{
		ContentChangedHandle = Slater::OnContentChanged().AddSPLambda(this, [this]() { Refresh(); });
	}
	else if (!bEnabled && ContentChangedHandle.IsValid())
	{
		Slater::OnContentChanged().Remove(ContentChangedHandle);
		ContentChangedHandle.Reset();
	}
}

bool SSlaterView::SetLayout(const FString& InLayoutId)
{
	LayoutId = InLayoutId;
	return Reload();
}

bool SSlaterView::SetStyle(FName InStyleName)
{
	const TSharedRef<FSlaterStyleStack> NewStack = MakeShared<FSlaterStyleStack>();
	if (!Slater::StyleLibrary().BuildStack(InStyleName, *NewStack))
	{
		DetachOutlets();
		Binder.Reset();
		ActiveContext.Reset();
		MockModel.Reset();
		Result = FSlaterBuildResult();
		ReloadErrors.Reset();
		AddError(Result, LayoutId, FString::Printf(TEXT("unknown style '%s'"), *InStyleName.ToString()));
		ChildSlot[MakeErrorView(Result.Errors)];
		return false;
	}
	StyleName = InStyleName;
	Stack = NewStack;
	return Reload();
}

bool SSlaterView::TryBuild(const TSharedRef<FSlaterStyleStack>& InStack, FGeneration& OutBuilt, TArray<FSlaterError>& OutErrors) const
{
	const FString Label = FString::Printf(TEXT("UI/Layouts/%s.%s"), *LayoutId, Slater::LayoutFileExtension());
	if (!Slater::LoadLayout(LayoutId, *InStack, OutBuilt.Result))
	{
		OutErrors = OutBuilt.Result.Errors;
		return false;
	}

	OutBuilt.Context = ExplicitContext;
	if (!OutBuilt.Context.IsValid() && bUseMockContext)
	{
		const FString File = Slater::FindLayoutFile(LayoutId);
		if (!File.IsEmpty() && IFileManager::Get().FileExists(*FPaths::ChangeExtension(File, TEXT("mock.json"))))
		{
			OutBuilt.Mock = MakeShared<FSlaterModel>();
			FSlaterError MockError;
			if (!Slater::LoadMockFor(File, *OutBuilt.Mock, MockError))
			{
				OutErrors.Add(MockError);
				return false;
			}
			OutBuilt.Context = Slater::MakeModelScope(OutBuilt.Mock.ToSharedRef());
		}
	}
	if (!OutBuilt.Context.IsValid())
	{
		return true;
	}
	if (!Slater::CheckBindings(OutBuilt.Result, *OutBuilt.Context, Label, OutErrors))
	{
		return false;
	}
	OutBuilt.Binder = MakeShared<FSlaterBinder>(OutBuilt.Context.ToSharedRef(), ESlaterBindMode::Pushed, *InStack);
	OutBuilt.Binder->Attach(OutBuilt.Result, Label);
	return true;
}

void SSlaterView::Install(const TSharedRef<FSlaterStyleStack>& InStack, FGeneration&& Built)
{
	// The old tree's outlets give their content back before the new tree takes it.
	DetachOutlets();
	// Hold the old style until the old tree is off the screen: its repeated items point at it.
	const TSharedRef<FSlaterStyleStack> OldStack = Stack;
	Result = MoveTemp(Built.Result);
	ChildSlot[Result.Root.ToSharedRef()];
	Binder = MoveTemp(Built.Binder);
	ActiveContext = MoveTemp(Built.Context);
	MockModel = MoveTemp(Built.Mock);
	Stack = InStack;
	ReloadErrors.Reset();
	++BuildCount;
	ApplyOutlets();

	// A state the new layout still declares keeps its value; a new one starts at its default.
	TMap<FName, FString> NewState;
	for (const FSlaterStateDecl& Decl : Result.States)
	{
		const FString* Old = State.Find(Decl.Name);
		NewState.Add(Decl.Name, Old ? *Old : Decl.Default);
	}
	State = MoveTemp(NewState);
	// A new tree never animates into its first look: it snaps to the state it opens in.
	ApplyStateConditions(false);
}

void SSlaterView::ApplyStateConditions(bool bAnimate)
{
	for (const FSlaterStateSize& Size : Result.StateSizes)
	{
		Slater::ApplyStateSize(Size, State.Find(Size.Key), bAnimate);
	}
	if (bAnimate)
	{
		EnsureMotionTimer();
	}
	for (const FSlaterStateCondition& Condition : Result.StateConditions)
	{
		if (const TSharedPtr<SWidget> Target = Condition.Widget.Pin())
		{
			const FString* Value = State.Find(Condition.Key);
			const bool bMatch = Value && Value->Equals(Condition.Value, ESearchCase::CaseSensitive);
			if (Condition.bPopup)
			{
				StaticCastSharedPtr<SSlaterPopup>(Target)->SetOpenRequested(bMatch);
			}
			else
			{
				Slater::SetShown(*Target, bMatch);
			}
		}
	}
}

bool SSlaterView::GetState(FName Key, FString& OutValue) const
{
	const FString* Value = State.Find(Key);
	if (!Value)
	{
		return false;
	}
	OutValue = *Value;
	return true;
}

bool SSlaterView::SetState(FName Key, const FString& Value)
{
	FString* Current = State.Find(Key);
	if (!Current)
	{
		UE_LOG(LogSlaterUI, Warning, TEXT("SlaterUI: the layout '%s' does not declare a state '%s'"), *LayoutId, *Key.ToString());
		return false;
	}
	if (!Current->Equals(Value, ESearchCase::CaseSensitive))
	{
		*Current = Value;
		ApplyStateConditions(true);
	}
	return true;
}

bool SSlaterView::IsMoving() const
{
	return Result.StateSizes.ContainsByPredicate([](const FSlaterStateSize& Size) { return Size.Anim.IsValid() && Size.Anim->IsActive(); });
}

bool SSlaterView::AdvanceMotion(float DeltaTime)
{
	bool bMoving = false;
	for (const FSlaterStateSize& Size : Result.StateSizes)
	{
		bMoving |= Slater::AdvanceStateSize(Size, DeltaTime);
	}
	return bMoving;
}

void SSlaterView::EnsureMotionTimer()
{
	if (!IsMoving() || bMotionTimerRunning)
	{
		return;
	}
	bMotionTimerRunning = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSlaterView::TickMotion));
}

EActiveTimerReturnType SSlaterView::TickMotion(double /*CurrentTime*/, float DeltaTime)
{
	if (AdvanceMotion(DeltaTime))
	{
		return EActiveTimerReturnType::Continue;
	}
	bMotionTimerRunning = false;
	return EActiveTimerReturnType::Stop;
}

void SSlaterView::DetachOutlets()
{
	for (const TPair<FName, TWeakPtr<SSlaterOutlet>>& Pair : Result.Outlets)
	{
		if (const TSharedPtr<SSlaterOutlet> Outlet = Pair.Value.Pin())
		{
			Outlet->SetContent(nullptr);
		}
	}
}

void SSlaterView::ApplyOutlets()
{
	for (const TPair<FName, TWeakPtr<SSlaterOutlet>>& Pair : Result.Outlets)
	{
		const TSharedPtr<SWidget>* Content = OutletContent.Find(Pair.Key);
		if (const TSharedPtr<SSlaterOutlet> Outlet = Pair.Value.Pin())
		{
			Outlet->SetContent(Content ? *Content : nullptr);
		}
	}
}

bool SSlaterView::Reload()
{
	ReloadErrors.Reset();
	if (LayoutId.IsEmpty())
	{
		DetachOutlets();
		Binder.Reset();
		ActiveContext.Reset();
		MockModel.Reset();
		Result = FSlaterBuildResult();
		ChildSlot[SNullWidget::NullWidget];
		return true;
	}
	FGeneration Built;
	TArray<FSlaterError> Errors;
	if (TryBuild(Stack, Built, Errors))
	{
		Install(Stack, MoveTemp(Built));
		return true;
	}
	// A failed reload shows its errors in place of the tree.
	DetachOutlets();
	Binder.Reset();
	ActiveContext.Reset();
	MockModel.Reset();
	Result = FSlaterBuildResult();
	Result.Errors = Errors;
	ChildSlot[MakeErrorView(Result.Errors)];
	return false;
}

bool SSlaterView::Refresh()
{
	if (LayoutId.IsEmpty())
	{
		return Reload();
	}

	const TSharedRef<FSlaterStyleStack> NewStack = MakeShared<FSlaterStyleStack>();
	FGeneration Built;
	TArray<FSlaterError> Errors;
	bool bOk = false;
	if (Slater::StyleLibrary().BuildStack(StyleName, *NewStack))
	{
		bOk = TryBuild(NewStack, Built, Errors);
	}
	else
	{
		FSlaterError& Error = Errors.AddDefaulted_GetRef();
		Error.Path = LayoutId;
		Error.Message = FString::Printf(TEXT("unknown style '%s'"), *StyleName.ToString());
	}

	if (bOk)
	{
		Install(NewStack, MoveTemp(Built));
		return true;
	}
	if (HasErrors())
	{
		// Nothing good to keep: show the new errors in place of the old ones.
		Result = FSlaterBuildResult();
		Result.Errors = Errors;
		ReloadErrors.Reset();
		ChildSlot[MakeErrorView(Result.Errors)];
		return false;
	}
	// Keep the tree on screen, error banner on top. The banner does not take clicks.
	ReloadErrors = Errors;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Result.Root.ToSharedRef()
		]
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			MakeErrorView(ReloadErrors)
		]
	];
	return false;
}

bool SSlaterView::SetContext(TSharedPtr<ISlaterScope> InContext)
{
	ExplicitContext = MoveTemp(InContext);
	return Refresh();
}

void SSlaterView::UseMockContext(bool bEnable)
{
	bUseMockContext = bEnable;
	Refresh();
}

bool SSlaterView::SetOutlet(FName Id, TSharedPtr<SWidget> Content)
{
	if (Content.IsValid())
	{
		OutletContent.Add(Id, Content);
	}
	else
	{
		OutletContent.Remove(Id);
	}
	const TWeakPtr<SSlaterOutlet>* Found = Result.Outlets.Find(Id);
	if (const TSharedPtr<SSlaterOutlet> Outlet = Found ? Found->Pin() : nullptr)
	{
		Outlet->SetContent(Content);
		return true;
	}
	return false;
}

TSharedPtr<SWidget> SSlaterView::GetOutletContent(FName Id) const
{
	const TSharedPtr<SWidget>* Content = OutletContent.Find(Id);
	return Content ? *Content : nullptr;
}

bool SSlaterView::RunCommand(const FSlaterCommandCall& Call)
{
	if (Slater::IsStockCommand(Call.Name))
	{
		return Slater::RunStockCommand(*this, Call);
	}
	if (Commands.Run(Call) || Slater::Commands().Run(Call))
	{
		return true;
	}
	if (!WarnedMissingCommands.Contains(Call.Name))
	{
		WarnedMissingCommands.Add(Call.Name);
		UE_LOG(LogSlaterUI, Warning, TEXT("SlaterUI: no handler is registered for the command '%s' (layout '%s'; further clicks stay quiet). A layout under Examples/ gets its handlers from its C++: open it with Slater.Example <Name> (Slater.Example lists them)"), *Call.Name.ToString(), *LayoutId);
	}
	return false;
}

TSharedPtr<SWidget> SSlaterView::FindById(FName Id) const
{
	const TWeakPtr<SWidget>* Found = Result.ById.Find(Id);
	return Found ? Found->Pin() : nullptr;
}

TSharedRef<SWidget> SSlaterView::MakeErrorView(const TArray<FSlaterError>& Errors) const
{
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	for (const FSlaterError& Error : Errors)
	{
		Lines->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(Error.ToString())).ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.35f)).AutoWrapText(true)
		];
	}
	return SNew(SBorder).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.8f)).Padding(8.f)[Lines];
}
