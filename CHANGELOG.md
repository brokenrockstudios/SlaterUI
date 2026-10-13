# Changelog

Version format `YYMM.DDRR` (year, month, day, revision of that day). Newest first. Keep entries to one line where possible.

## 2610.1211

- Collapsible sidebar v1 sample (T-403): `Content/UI/Layouts/Sidebar.sltr` (template `nav-item`, state `expanded`, hamburger `ui.toggle:expanded`, labels, title, logo and a spacer on `visible-when`, pinned logout), SVG icons in `Content/UI/Icons`, `Slater::MakeFileImageSource` / `MakePluginIconSource` (`SlaterImages.h`, `*.svg` through `FSlateVectorImageBrush`) and `Slater::HasImageSource`; the preview tab installs the plugin icons when the game set no image source. Tests `BRS.SlaterUI.Sidebar.*`, `BRS.SlaterUI.Render.Sidebar` (needs `-WithRHI`).

## 2610.1210

- `hover-set-state` and `leave-set-state` on `surface` (T-414): the pointer entering or leaving sets one view state (`key=value`, checked like `set-state`, instant, works in templates). Tests: `BRS.SlaterUI.State`.

## 2610.1209

- Designer path (T-352). `USlaterLayoutWidget` (UMG palette, "Slater"): a layout ID dropdown, style, and `ViewModelName` to take `{Path}` data from the enclosing widget's `UMVVMView`; `SetViewModel` from code. `USlaterLayoutUserWidget` is the same as a `UUserWidget`. New module `SlaterUIGameFeatures`: game feature action **Add Slater Layout** (layout ID, style, HUD slot tag per entry; hands each to the game's `ISlaterSlotHost` (`Slater::SetSlotHost`) in every game world while the feature is active, and adds the feature plugin's `Content/UI/Layouts` and `Styles` as roots). The UIExtension slot host is the separate plugin SlaterUIExtension; see `Docs/SlaterUI/Extending.md`. Template `Templates/UIPack` (content-only game feature plugin with a sample layout and style). Adds plugin dependencies `GameFeatures`, `ModelViewViewModel`. Tests: `BRS.SlaterUI.LayoutWidget`, `BRS.SlaterUI.GameFeatureAction`.

## 2610.1208

- Stock commands and view state (T-353). `<state name default>` (direct child of the root, `SlaterState.h`), `set-state="k=v"` on `surface`, `visible-when="k=v"` on any element (common attribute; string equality; not with `visible`, not inside a `repeat`), all checked against the declared states at build time (`Slater::ExtractStates`, `CheckStateReferences`); `SSlaterView::GetState` / `SetState` keep the values through reloads, refreshes and restyles. `on-click` takes one word of argument after a colon (`FSlaterCommandCall::Argument`, `Slater::CheckCommandText`); the stock commands `ui.close ui.push ui.pop ui.toggle ui.sound ui.style` run in every view (`Slater::RunStockCommand`), names starting with `ui.` can no longer be registered, `ui.close/push/pop` go to an `ISlaterScreenHost` (`Slater::SetScreenHost`), `ui.sound` to `Slater::SetSoundPlayer`. `SSlaterSurface` gets `SetStateChange` and `HasAction`; `SetCommand` takes the argument. New shipped layout `Tabs`. Tests: `BRS.SlaterUI.State`, `BRS.SlaterUI.StockCommand`.

## 2610.1207

- Templates, imports, template slots and `<umg>` (T-346): `<template name params>` with `$param` (used like an element; arguments, `id` and the slot attributes land on the body root), `<import src>` of a `<templates>` file, expanded before the build by `Slater::ExpandTemplates` (`SlaterTemplates.h`); an error in an imported template is reported under its file (`FSlaterPos::File`). `FSlaterElementDesc::TemplateSlots` lets a host element take named template children (`FSlaterBuildContext::SlotTemplates`). New module `SlaterUIUMG` with `<umg class>` and `USlaterUmgSubsystem` (owns the `UUserWidget`; only in the stock registry, so a mod-layer registry refuses it). Tests: `BRS.SlaterUI.Templates`, `Imports`, `ImportFile`, `TemplateSlots`, `Umg`.

## 2610.1206

- Live mouse states (T-387): `surface` takes `interactive` (Bool); a surface with `on-click` is interactive unless `interactive="false"`, and `state="disabled"` never is. An interactive surface shows `<role>.Hover` under the pointer and `<role>.Pressed` while the left button is down over it, and goes back on leave (`SSlaterSurface::SetInteractive`, `IsInteractive`). Gallery's plain Button is interactive. Tests in `BRS.SlaterUI.Command`.

## 2610.1205

- Preview tab shows a refused style reload (T-388): `Slater::GetStyleReloadErrors()` returns the problems of the last refused `ReloadStyles` (empty after a good one, valid inside `OnContentChanged`), and the tab shows them in a red banner (file, line, column) until the next good reload.

## 2610.1204

- Outlets, commands and view-model bindings (T-345). Data goes through `ISlaterScope` / `ISlaterListSource` (`SlaterScope.h`): `Slater::MakeObjectScope` binds a UObject view model through the engine's FieldNotification descriptor (a change event pushes the value through the attribute's `Apply`; `{A.B}` follows a replaced link; a `TArray` of view models is a list, diffed by key), `Slater::MakeModelScope` wraps the mock JSON model, `FSlaterGetterScope` is plain C++ getters. `FSlaterBindingSet` owns the subscriptions of a view or of one repeated item (dropping it removes every delegate) and feeds the debug command `Slater.StaleBindings` / `Slater::CollectStaleBindings`. `SSlaterView` gets `SetContext` (every path is checked when the layout is built: `Slater::CheckBindings` fails with the binding's line and a "did you mean"), `UseMockContext` (binds `<Layout>.mock.json`, on in the editor preview), `SetOutlet`, `GetCommands`, `RunCommand`. New `<outlet id>` element (`SSlaterOutlet`; content survives reloads), `on-click` on `surface` (`FSlaterCommandTable`, `Slater::Commands()`), common bindable `visible` and `enabled` on every element (`image` loses its own `visible`). `SSlaterRepeat::Attach` takes a scope; `Slater::Bind` and `FSlaterBinder` take a scope or the model; `ESlaterBindMode` and `FSlaterListStats` moved to `SlaterBindings.h`. The view keeps its style stack alive for the tree built from it. `repeat` is `SelfHitTestInvisible` (was `HitTestInvisible`) so a button in an item takes clicks. Adds `FieldNotification` and `InputCore` dependencies. `BRS.SlaterUI.Bind.*`, `Outlet`, `Command`, `VisibleEnabled`, `MockContext` tests.

## 2610.1203

- Live reload and preview (T-344): new editor module `SlaterUIEditor` watches the layout and style folders (`DirectoryWatcher`, 0.25 s quiet time) and calls `Slater::NotifyFilesChanged`, which reloads the style library (`Slater::ReloadStyles`, all or nothing: a broken style file keeps the styles in use) and fires `Slater::OnContentChanged`; `SSlaterView` listens and `Refresh()`es, keeping the last good tree with an error banner over it (`HasReloadErrors`, `GetReloadErrors`, `GetBuildCount`, `SetAutoRefresh`). Preview tab "Slater Layout Preview" (layout and style pickers) and console command `Slater.Layout [Id]`. Also `Slater::AddLayoutRoot` / `AddStyleRoot` (+ Remove), `Slater::GetLayoutIds`; `StyleLibrary()` now reloads in place. `BRS.SlaterUI.Reload.*` tests.

## 2610.1202

- Cheap `repeat` instances (T-370): a `repeat`'s child is checked and resolved once (`Slater::CompileTemplate`, `FSlaterCompiledTemplate`) and each item only runs the builders (`Slater::InstantiateTemplate`, replaces `BuildTemplateInstance`); bindings share one `FSlaterBindingSpec` (`FSlaterBoundAttr` is now `{Widget, Spec}`); `FSlaterBuildContext::Values` is a reference and gains `Template`; new optional `FSlaterElementDesc::ResolveDefaults` (used by `text` and `surface` for style defaults and the literal `state`); `SSlaterSurface::BrushFor` shares one brush per distinct surface and `SetSurface` to an equal surface is a no-op; `FSlaterModel::FindItem` is indexed. Stash instancing 1.9x to about 1.2x of hand-written Slate. `List.Probe` builds are the median of 21 and the burst's JSON items are prepared outside the timer. New tests `List.Model.KeyIndex_*`, `Content.SurfaceBrush.*`.

## 2610.1201

- Error texts that teach (T-373): `fill` says how to fix a bad weight, a missing required attribute gives its type and an example, `unknown element` adds what each known element is for (new `FSlaterElementDesc::Purpose`), and a layout syntax error says more errors may appear after the fix.

## 2610.1106

- Agent fluency probe (T-355): `BRS.SlaterUI.AgentProbe.Sltr` and `.Cpp` tests render agent-written layouts and plain-Slate widgets to `Out.png` with a `Result.txt` and a round history (no-ops without agent files in `Saved/SlaterUI/AgentProbe`); `Tools/Scripts/AgentProbeCpp.ps1` builds an agent's C++ under the build lock. Adds `ImageWrapper` to `SlaterUITests`. Scores, prompts and fixtures are in `Docs/SlaterUI/AgentProbe`; no runtime change.

## 2610.1105

- List probe (T-351): `<repeat source key at columns cell-width cell-height>` instantiates its one child per item of a model array (keyed: a changed field updates only its bindings, an added key builds one instance, a removed key destroys one, the rest keep their widgets); `{Path}` bindings on `text` content, `surface state`, `image src` and `image visible`, pushed (`Slater::Bind`, `FSlaterBinder`) or polled (Slate attributes) for comparison; `<image src size visible>`; `FSlaterModel` (mock data from `.mock.json` through the Json module, change events); `ISlaterImageSource`; shipped `ListProbe` layout and mock file. Element builders can report why they failed (`FSlaterBuildContext::Failure`); `FSlaterBuildResult` gains `Bindings` and `Repeats`; `bTemplateChildren` / `TextBind` on element descriptors; `SSlaterSurface::SetRole`. Adds the `Json` module. `BRS.SlaterUI.List.*` tests; `List.Probe` measures against hand-written Slate (`-WithRHI`).

## 2610.1104

- `surface` (procedural rounded box from a surface role, `state` hover/pressed/disabled with fall-back to the role, `inset`) and `text` elements (T-343); `SSlaterSurface`; `SSlaterView` and `Slater::LoadLayout` / `LoadLayoutFile` / `StyleLibrary` (`SlaterView.h`; layouts from the project's and the plugin's `Content/UI/Layouts`, file size checked before the read); shipped `Gallery` and `Panel` layouts; Base gains `UI.Surface.Background`. Adds `CoreUObject`. `FSlaterBuildContext::Style`. `BRS.SlaterUI.Content.*` and `Render.*` tests (PNG render and cost need `-WithRHI`).

## 2610.1103

- Element registry (T-342): public `Slater::Elements().Register({Tag, typed attributes, child rule, builder})` (`SlaterElements.h`); typed attributes (metric, color, font, surface, enum, bool, int, text; token or literal), common attributes (`id width height min-* max-* padding`) and slot attributes (`fill margin halign valign`), one `SBox` wrapper only when a size or `padding` is set; stock `vbox hbox overlay box scroll uniform-grid wrap spacer`; `Slater::BuildLayout` reports every error as `path(line,col): message` with the nearest valid tag, attribute, enum word or token, and measures widget count and depth with budget warnings (`FSlaterBudget`). Adds `Slate` and `SlateCore`. `BRS.SlaterUI.Elements.*` tests. Fixes the T-359 style tests, which had never compiled or run (float `AreEqual`, 1-based columns).

## 2610.1102

- Style tokens (T-359): `FSlaterStyle` / `FSlaterStyleStack` behind `ISlaterStyleSource` (colors, metrics, fonts, surfaces by dotted `FName`, parent-walk fall-back, never to a child), `FSlaterStyleLibrary` with `extends` and layering, `Slater::ParseStyle` for `.sltrstyle` files (same parser, `path(line,col)` errors, "did you mean" for near-miss tokens), shipped `Base` and `Dusk` in `Content/UI/Styles`; `BRS.SlaterUI.Style.*` tests. Adds the `Projects` module; the plugin now has content.

## 2610.1101

- New plugin (T-341), standalone (engine modules only): `Slater::ParseMarkup` reads a `.sltr` layout file into a `FSlaterNode` tree (tag, attributes, children, text, line and column); strict XML subset with entity decoding, caps on size, depth and element count (`FSlaterLimits`), and errors as `FSlaterError` (`path(line,col): message`); `BRS.SlaterUI.Parse.*` tests.
