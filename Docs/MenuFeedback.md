# Menu interaction feedback

The menus now use a shared `UMenuFeedbackWidget` base for UMG buttons and native Slate buttons. Main menu, collection, settings/keybindings, pause, trial choices, game over, victory, upgrade choices, skill-tree actions and tester settings inherit it. Existing ink reveals, colors, click handlers and navigation remain intact.

## Research and chosen treatment

- [Epic's Fortnite button documentation](https://dev.epicgames.com/documentation/fortnite/custom-buttons-in-fortnite) describes shared selection states, animated backgrounds, button sounds and hover/unhover animation bindings.
- [Epic's CommonUI button style](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonUI/UCommonButtonStyle) separates hover and press sounds.
- [Xbox UI focus guidance](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/113) supports clear, consistent focus handling for navigation.

These are established patterns, not evidence that a particular animation increases commercial success. Our chosen tuning is 1.05x on hover/focus, 1.08x on press, approximately 120 ms easing and a 90 ms minimum press pop for UMG buttons. Render scaling preserves layout dimensions and sibling placement. A menu that closes immediately still closes immediately; feedback never delays an action. Hidden or disabled buttons reset. Mouse hover and controller focus share the same visual treatment, with no repeated sound while remaining focused.

## Tuning

On the menu widget's Class Defaults, use **Menu > Feedback**:

- `Hover Scale` (1.05), `Press Scale` (1.08), `Response Seconds` (0.12).
- `Hover Sound` and `Press Sound`; clear them to silence feedback. Set both scales to 1 to disable enlargement.

Audio assets are in `/Game/HeavensDivide/Audio/Menu`: `MS_MenuHover` and `MS_MenuPress`. Each has a generated WAV, independent two-voice concurrency, and graph inputs `PitchMin`/`PitchMax` (±0.35 semitones). Adjust the MetaSound's Volume to tune loudness. Separate concurrency prevents fast hovering from suppressing a press. Playback uses Slate UI audio, works while paused, and follows the existing master volume.

The original procedural WAVs are in `SourceAudio/Menu`. `Tools/create_menu_audio.py` regenerates source WAVs and creates any missing assets when run through Unreal Python. Existing MetaSounds are preserved.

Future button-based menus should inherit `UMenuFeedbackWidget` (or `UMenuPromptWidget`). Standard UMG/SButton controls are discovered automatically, including dynamically rebuilt choices. Custom-painted controls need their own feedback integration. The skill-map constellation keeps its existing node highlights; its Learn/Back/Refund/Reset View buttons use the new feedback.

## Validation

`HeavensDivide.UI.MenuFeedback` checks discovery, dynamic buttons, controller focus, unchanged layout dimensions, press/release while paused, disabled states, inactive switcher pages and teardown. `HeavensDivide.Audio.CombatPalette` also renders both new UI MetaSounds twice, checking audible finite output, no clipping, completion and random pitch variation. Existing UI and settings automation checks cover integration.

Validated September 28, 2026: editor build passed; all nine selected UI/settings/meta/audio checks passed across the initial suite and focused final rerun. Reports: `Saved/MenuAudio/Tests` and `Saved/MenuAudio/FinalTests`. The first engine launch crashed in DevHttp before tests started; validation used the command-line-only `-DDC=InstalledNoZenLocalFallback` cache override. The isolated pause test was corrected to pause through its controller, and both feedback and pause tests then passed.
