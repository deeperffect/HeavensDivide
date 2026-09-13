# Keybindings

Open **Settings → Keybinds** in the main menu. Select a binding, then press a keyboard key or mouse button. Escape cancels selection. Changes save automatically and remain in effect on subsequent launches. Assigning an occupied key exchanges the two actions' bindings. **Restore Defaults** resets the keyboard/mouse bindings; Back returns to Settings.

| Action | Default |
| --- | --- |
| Move forward / backward / left / right | W / S / A / D |
| Swap character | Right mouse button |
| Dash | Space |
| Interact | E |

Controller mappings remain unchanged. Escape and the console key are reserved. Automatic attacks do not require an attack key.

The subpage shares the existing settings frame, background texture, heading/body fonts, colors and ink-button artwork. Its content scrolls when necessary.

Bindings are stored in `UHeavensDivideGameUserSettings`. Each player gets a runtime copy of the Enhanced Input context, retaining movement modifiers and controller mappings. Changing settings rebuilds that copy without editing the input asset. The saved `IMC_Player` asset also uses right mouse button for swap; `Tools/configure_default_swap_input.py` authors and verifies that default.

`HeavensDivide.Settings.Keybinds` tests persistence using isolated settings files, conflict exchange, reset, runtime mapping and menu navigation.
