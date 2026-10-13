# UI pack template

A game feature plugin that carries SlaterUI layouts and styles and nothing else. Copy this folder to `Plugins/<Where>/<YourPack>/`, rename `UIPack.uplugin.txt` to `<YourPack>.uplugin` (and fix its name inside), rename the `UIPack` folders and the style, then:

1. Write layouts as `Content/UI/Layouts/<YourPack>/<Name>.sltr` and styles as `Content/UI/Styles/<YourPack>.sltrstyle`. Layout IDs and style names are global across every root, so keep a pack's layouts in a folder of its own (`UIPack/Hud`) and give the style a name of its own.
2. In the editor, create the feature's Game Feature Data asset (right-click, Miscellaneous, Data Asset, `GameFeatureData`) named like the plugin and add the action **Add Slater Layout**: one entry per layout with the layout ID (`UIPack/Hud`), the style (`UIPack`) and the HUD slot tag.
3. The game needs a slot host that knows what a HUD slot is. With Lyra's UIExtension it is the **SlaterUIExtension** plugin (enable it): the slot is a **Slater Extension Point** widget (UMG palette) with the same tag, placed in the HUD's widget Blueprint; a plain UI Extension Point shows widget classes only, not layouts. Another HUD system needs its own host, see `Docs/SlaterUI/Extending.md`.

While the feature is active the pack's `Content/UI/Layouts` and `Content/UI/Styles` are searched, before the project's and SlaterUI's own; when it deactivates they are dropped again.

A layout can also go into a widget Blueprint directly, with no action: the **Slater Layout** widget (UMG palette, "Slater") has a layout ID dropdown.
