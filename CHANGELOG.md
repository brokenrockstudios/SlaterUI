# Changelog

Version format `YYMM.DDRR` (year, month, day, revision of that day). Newest first. Keep entries to one line where possible.

## 2610.1703

- `segmented` `icons="A, B"` (one per option; a Slater image or an engine style brush like `Icons.cylinder`); the WidgetGallery SegmentedControl row shows the Starship icons. Tests `ASegmentedControl_Icons*`.

## 2610.1702

- Icon-only `combo` (T-797): `icon-only`, `icon-size` on `combo` and `icon` on `option`; the WidgetGallery SimpleComboBox row shows it (8 TODO lines left). Tests `AnIconOnlyCombo_*` in `Combo`.

## 2610.1701

- `tooltip` attribute (T-423): any element, literal, `{Path}` or `$param`; the engine `SToolTip` with the `UI.Surface.Menu` surface, `UI.Font.Body` and `UI.Color.MenuText`; empty shows none. `Sidebar.sltr` nav items show their label. Tests `BRS.SlaterUI.Tooltip`.
- `loc` attribute (T-535): `loc="Key"` or `loc="Namespace/Key"` gives `text`, `textbox` `text` and `hint`, and `tooltip` literals an `FText` namespace and key (`Key`, `Key.hint`, `Key.tooltip`) so a loaded culture can translate them; `Slater::MakeLiteralText`. Tests `BRS.SlaterUI.Loc`.

## 2610.1701

- `radio` element (T-797): a group of round boxes over `SCheckBox` with `options`, `selected` (bindable), `direction`, `on-change`, surfaces `UI.Surface.Radio*`; the WidgetGallery Radio Button row shows it (9 TODO lines left). Tests `ARadio_*` in `Controls.Input`.

## 2610.1701

- `surface` gets `shadow` (offset in slate units, a metric) and `shadow-color` (default black at 50% alpha) (T-823): a fake drop shadow of the surface's own shape (rounded or chamfered), painted under its fill; paint only, no layout or hit-test change. `SSlaterSurface::SetShadow`; tests `BRS.SlaterUI.Shadow` and `BRS.SlaterUI.Render.Shadow`.

## 2610.1611

- `HealthHud` example (T-554): `USlaterHealthHudViewModel` (`SlaterHealthHud.h`, a `UMVVMViewModelBase` with the minimal field-notify pattern), `Examples/HealthHud.sltr` with mock, `SlaterExamples::MakeHealthHud` and `Slater.Example HealthHud` (timer drains and heals the view model; `HealthHud.Hurt:10` / `HealthHud.Heal:10`). `SlaterUIExamples` now depends on `ModelViewViewModel`. Tests `MvvmViewModelBase_PushesChangesThroughMakeObjectScope` and `HealthHud_*`.

## 2610.1610

- Sample layout `WidgetGallery` (`Content/UI/Layouts/WidgetGallery.sltr`): the stock controls in two columns, laid out like the engine's Starship Gallery widgets tab (buttons in three looks, toggles, segmented, checkboxes, combo, slider, spin box, text boxes, meter, toolbars); controls SlaterUI lacks show as a red `TODO:` line (T-797). Tests `BRS.SlaterUI.WidgetGallery.*`.

## 2610.1609

- A `USlaterLayoutUserWidget` subclass fails its build, with the attribute's line, for an `on-click` or element event naming a command that is not stock, not registered on the view and not process-wide (T-566). `SSlaterView` gets `CheckCommands` and `RegisterCommands` arguments (off by default, so plain views, the preview and `USlaterLayoutWidget` stay lenient); `FSlaterBuildResult::Commands` lists the named commands. Commands are now registered before the first build. Tests in `BRS.SlaterUI.GameFeatureAction`.

## 2610.1608

- README: "Animated transitions" section with `Docs/Images/Sidebar_Dusk.gif` (the sidebar sample closing and opening in Dusk). New test `BRS.SlaterUI.Render.SidebarGif` (`-WithRHI`) writes the frames and an ffconcat list to `Saved/SlaterUI/Render/SidebarGif/`; ffmpeg command in DevNotes.

## 2610.1607

- `USlaterLayoutUserWidget` gets protected `Notify(Path)`, `Notify({Paths})` and `NotifyAll()` (T-565), no-ops before the build and after `SetViewModel`; `FSlaterGetterScope::NotifyAll` added. `USlaterHelloWidget` uses them and keeps `Count` private (`GetCount()`). Tests `Notify_WithSeveralPaths_UpdatesEachBoundText`, `NotifyAll_MakesEveryBindingReadAgain`, `Notify_BeforeTheBuild_DoesNothing`, `Notify_AfterSetViewModel_IsSafe_AndTheViewModelStaysTheSource`.

## 2610.1606

- `FSlaterGetterScope` takes paths as plain literals (T-562): `ANSICHAR*` overloads of `AddValue`, `AddList`, `AddSetter` and `Notify` forward to the `FString` ones; `Hello` example uses them. Test `APathAsAPlainLiteral_RegistersReadsAndNotifiesLikeTEXT`.

## 2610.1605

- A command with no handler warns once per view and command (T-561), naming the layout and pointing to `Slater.Example <Name>`, instead of on every click; opening `Examples/*` without its C++ stays quiet until a button is clicked. Test `ACommandNobodyRegistered_WarnsOncePerView`.

## 2610.1604

- `HelloWidget` example (T-560): `USlaterHelloWidget` (`SlaterHelloWidget.h`, module `SlaterUIExamples`) is Hello as a `USlaterLayoutUserWidget` subclass tied to `Examples/Hello.sltr`, made with `CreateWidget` and `AddToViewport`; `Slater.Example HelloWidget` adds it to the play world (PIE). `FSlaterExample` gets an optional `Show(UWorld&)` for widget examples and `Create` may be null. Tests `BRS.SlaterUI.Examples.*HelloWidget*`.

## 2610.1603

- `USlaterLayoutUserWidget` can be subclassed as a code-owned layout (T-559): `LayoutId` and `StyleName` are protected values a subclass sets in its constructor, and the virtual hooks `BuildScope(FSlaterGetterScope&)` and `RegisterCommands(FSlaterCommandTable&)` run on each build, so the class owns its layout, data and commands. `SSlaterView` takes a `Context` argument so the first build already has its data. `Configure` stays for the generic widget the extension point makes.

## 2610.1602

- New module `SlaterUIExamples` (`DeveloperTool`: editor and development builds, not shipping) with worked examples of C++ bound to layouts (T-553). `FSlaterExampleRegistry` lists them, the console command `Slater.Example [Name] [Style]` opens one in a window (no name lists them), and the first, `Hello` (`Content/UI/Layouts/Examples/Hello.sltr` plus a `.mock.json` for the preview tab), is a counter over `FSlaterGetterScope` and two commands. Tests `BRS.SlaterUI.Examples.*` build every registered example in `Base` and `Dusk`, from its mock file, and drive Hello's commands.

## 2610.1601

- `canvas` (over `SConstraintCanvas`: `anchor`, `offset`, `size`, `align` on a child), `grid` (over `SGridPanel`: `row`, `column`, `row-span`, `column-span` on a child, `gap`, `column-fill`, `row-fill`) and `safe-zone` (over `SSafeZone`: `title-safe`, `pad-*`) elements (T-529). The new slot attributes are accepted on the right parent only; an element's own attribute of the same name wins. Tests `BRS.SlaterUI.Panels.*`. Registered by `Slater::RegisterPanelElements`.

## 2610.1511

- `rich-text` `<img>` placement (T-544): by default an inline image is as tall as the font's line and centered on it (it used to sit on the baseline at the brush's size, riding high); `size` (height, width keeps the aspect) and `baseline` (how far the bottom sits below the text baseline) override it, checked at build. `tint` (a color token, as on `image`) multiplies the picture, so white icons show on a light style; without it the image keeps its colors. `FSlaterRichRun` reports `ImageSize`, `ImageBaseline` and the tint as `Color`. `Layouts/RichText.sltr` has a placement line, and the sample test counts its four rich texts.
- Samples: the white line icons of `Account.sltr` (card icons `UI.Color.Text`, External arrow `UI.Color.Accent`) and the `User` and `ChevronUp` icons of `ContextMenu.sltr` and `ContextMenuPopup.sltr` take a `tint`, so they show in `Base` (T-544). Test `Account.TheWhiteLineIcons_TakeTheStyleColors_AndTheAvatarKeepsItsOwn`.
- `SlaterContentElements.cpp`: the file-local `Attr` helper is now `ContentAttr`, so it no longer meets the `Slater::Attr` namespace in a unity build (T-544).

## 2610.1510

- `scale` element (T-528) over `SScaleBox` (`stretch` none/fit/fill/stretch/user, `scale`), `throbber` over `SCircularThrobber` (`color`, `size`), and `tint` on `image`. Tests `BRS.SlaterUI.ImageSizing.*`. Shared attribute names moved to `Private/SlaterAttrNames.h` (`Slater::Attr::`) after file-scope name clashes in the unity build; RichText's `IsNameChar` renamed.

## 2610.1509

- `rich-text` element (T-527) over `SRichTextBlock`: run tags `<b>`, `<i>`, `<font value="token">`, `<color value="token">` (closed with `</>`) and `<img src="Name"/>` (the installed image source); written with entities in `.sltr` (`&lt;b&gt;`), real tags in bound `{Path}` text. Bad markup is refused at its line and column (new `FSlaterElementDesc::CheckContent`, `FSlaterTextProblem`). New `SSlaterRichText` (`SetRichText`, `GetRuns`), `Slater::CheckRichText`, `Slater::RegisterRichTextElements`. Sample `Layouts/RichText.sltr` with `RichText.mock.json`. Tests `BRS.SlaterUI.RichText.*`.

## 2610.1508

- `spinbox` and `segmented` elements (T-526): `spinbox` over `SSpinBox<float>` (`value` may be `{Path}`, `min` 0, `max` 100, `step` 1, `on-change` while dragging or typing, `on-commit` when set; the engine clamps to the range) and `segmented` over `SSegmentedControl<FString>` (`options="A, B, C"`, `selected` may be `{Path}`, `on-change` gets the option; no usable option is refused). Tests `BRS.SlaterUI.Controls.Input.*`.

## 2610.1507

- `combo` and `option` elements over the engine's `SComboBox` (T-525): `<option value="..">Label</option>` children or a `options="a|Label, b"` list (also `{Path}`), `selected` (may be `{Path}`), `hint`, `font`, `on-change` (the picked value; moving the data fires nothing); look from the `TextBox` and `Menu` surface tokens, menu flips at the window edge (`FitInWindow`, T-439); a literal `selected` no option has, a duplicate value, a non-`option` child or both sources is refused at build with its line. Sample `Layouts/Combo.sltr`. Tests `BRS.SlaterUI.Combo`.

## 2610.1506

- `popup` element over `SMenuAnchor` (T-439): first child the anchor, second the menu; `placement=above|below|right`, `open-when="key=value"` (opens while the view state matches) and `close-state` (written when the engine closes it; defaults to `key=false` for `open-when="key=true"`), or a plain click on the anchor with the `SComboButton` click rule; `FitInWindow(true)`, no Slate focus on rows, a submenu is a nested popup that closes with its parent. New `SSlaterPopup`, `SSlaterPopupMenu`, `Slater::RegisterPopupElements`, `FSlaterStateCondition::bPopup`; `Slater::FindOwningView` (and the surface's `hover-set-state`) now reaches the view from inside a popup's menu. Sample `Layouts/ContextMenuPopup.sltr` (same rows as `ContextMenu.sltr`). Tests `BRS.SlaterUI.Popup.*`.

## 2610.1505

- Stock controls take their look from the style (T-514): `slider`, `checkbox` and `textbox` fill the engine's `FSliderStyle`, `FCheckBoxStyle` and `FEditableTextBoxStyle` with rounded-box brushes made from surface tokens (`UI.Surface.Slider.Bar` / `.Thumb` / `.Thumb.Hover`, `UI.Surface.CheckBox` / `.Hover` / `.Checked`, `UI.Surface.TextBox` / `.Hover` / `.Focused`) and `UI.Color.Text`; `Base` and `Dusk` gain `UI.Color.Control` / `ControlLine` and `UI.Radius.Round`. `slider` `color` and `track` now replace the fills only when written. New `FSlaterControlLook` / `Slater::FindControlLook` (public `SlaterControls.h`). Tests `BRS.SlaterUI.ControlStyle.*`.

## 2610.1504

- `button` element over `SButton` (T-524): keyboard and gamepad route for a clickable face (focus, navigation, Enter / Space / confirm key), `on-click` and `set-state` like `surface`, looks from `<role>.Hover` / `.Pressed` / `.Focused` / `.Disabled`; new `SSlaterButton`, `ESlaterButtonLook`, `Slater::ActivateAction` (shared with `SSlaterSurface::Activate`), `Base` gains `UI.Surface.Button.Focused`. Tests `BRS.SlaterUI.Button.*`.

## 2610.1503

- Two-way bindings and element events (T-381): stock controls `slider`, `checkbox` and `textbox` over the engine's `SSlider`, `SCheckBox`, `SEditableTextBox`; event attributes `on-change` and `on-commit` write the value to `{Path}` (view-model setter `SetX`, a native `Setter`, or a BlueprintReadWrite property; mock JSON; `FSlaterGetterScope::AddSetter`) or run a command with it (`FSlaterCommandCall::Value`). A path nothing can write fails at load with its line; an echo guard (`FSlaterEventSink`) keeps a write from bouncing back. New `ISlaterScope::CheckWrite` / `Write`, `FSlaterBuildResult::Events`, `Slater::EventAttribute`, `Slater::RegisterControlElements`; sample `Layouts/Settings.sltr`; tests `BRS.SlaterUI.TwoWay.*`, `Render.Settings`.

## 2610.1502

- Test fix (T-492): `SizeBy.ClipCutsTheElementToItsBox_AndIsOffByDefault` asserts "not `ClipToBounds`" for an element without `clip` and with `clip="false"` (it expected `Inherit`, but `STextBlock` defaults to `OnDemand` on 5.8). No runtime change.

## 2610.1501

- `transition="UI.Motion.Panel"` (T-406) on an element with `width-by` / `height-by`: the size moves to the new state's size over the motion token's duration and ease, continues from where it is on a mid-flight toggle, and snaps on first apply, reload and restyle. New `FSlaterAnimator` (pure, manual clock), `Slater::EaseFromWord`, console variable `Slater.MotionScale` (0 = instant), `SSlaterView::AdvanceMotion` / `IsMoving` with an active timer that runs only while something moves. `Sidebar.sltr` now animates. Tests `BRS.SlaterUI.Motion.*`.

## 2610.1401

- `width-by` / `height-by` (state-driven size, instant) and `clip` on any element (T-405): `Slater::ParseSizeBy`, `Slater::ApplyStateSize`, `FSlaterStateSize`, `FSlaterBuildResult::StateSizes`; state names checked like `visible-when`. `Sidebar.sltr` moves from the spacer trick to `width-by`; Base gains `UI.Size.SidebarOpen` and `UI.Size.SidebarRail`. Tests `BRS.SlaterUI.SizeBy.*`.

## 2610.1304

- `shape="chamfer"` on `surface` (T-413) with `cut` and `cut-tl`/`cut-tr`/`cut-br`/`cut-bl`: fill polygon and anti-aliased outline painted in `SSlaterSurface::OnPaint`, no material; `FSlaterChamfer`, `Slater::MakeChamferPath`, `SetChamfer`. Gallery gains a "Shapes" row. Tests `BRS.SlaterUI.Chamfer.*`, `Render.Chamfer`, `Render.ChamferGallery` (need `-WithRHI`).

## 2610.1303

- `motion` style token (T-404): `<motion name duration ease/>` (duration 0 to 2 s, ease one of `Linear`, `Quad*`, `Cubic*` In/Out/InOut, default `Linear`), `FSlaterMotion`, `ISlaterStyleSource::FindMotion` with the parent walk, `ESlaterTokenKind::Motion`, `Slater::EaseWords()`; Base defines `UI.Motion.Panel` (0.1 s, `CubicOut`). Tests in `BRS.SlaterUI.Style.*`.

## 2610.1302

- `<meter>` (`value` bindable 0..1 and clamped, `color`, `track`, height by the common `height`) and `<rule>` (`color`, `thickness`) stock elements, widget `SSlaterMeter` (T-409). Rounded-box brush with a texture rounds the image (checked by `Render.Meter`).

## 2610.1301

- Account dashboard v1 sample (T-408): `Content/UI/Layouts/Account.sltr` (templates `stat` and `link-card`, credit rows by `repeat`, wide cards, a fixed-width progress fill until `<meter>`), `Account.mock.json`, style `Skyline` (navy and teal, extends Base); Base gains `UI.Surface.Card` (+`.Hover`) `Inset` `Track` `Fill` with their colors and `UI.Radius.Card`, Dusk their dark colors. Tests `BRS.SlaterUI.Account.*`, `BRS.SlaterUI.Render.Account` (needs `-WithRHI`).

## 2610.1213

- Popup spike harness (T-416): console command `Slater.PopupSpike` in SlaterUIUMG shows an `SMenuAnchor` menu and `MenuRight` submenu in the PIE viewport and logs focus, keys and open/close. No behavior change otherwise.

## 2610.1212

- Popup menu v1 sample (T-415): `Content/UI/Layouts/ContextMenu.sltr` (footer chevron `ui.toggle:menu`, transparent scrim, popup frame, templates `menu-header` `menu-sep` `menu-item` `menu-plain` `menu-parent` `menu-link`, a Learn more submenu opened by `hover-set-state` that stays open until another row is hovered or the menu closes (the chevron resets it)), Base tokens `UI.Surface.Menu`, `MenuItem` (+`.Hover`), `Divider`, `Scrim` and colors `UI.Color.Menu` `Menu.Hover` `MenuLine` `MenuText` `MenuMuted` `Clear`, eight more SVG icons. Tests `BRS.SlaterUI.ContextMenu.*`, `BRS.SlaterUI.Render.ContextMenu` (needs `-WithRHI`).

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
