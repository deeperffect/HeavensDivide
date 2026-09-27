# Card descriptions

`Tools/card_descriptions.json` contains compact copy for all 57 saved upgrade cards. Each entry contains the base description and, when applicable, its rolled rarity format. Existing rarity overrides are resolved from the compact format so they cannot reintroduce longer text.

Run `Tools/configure_card_descriptions.py` with Unreal's Python commandlet after any older upgrade-authoring scripts. It changes only description fields, backs up changed assets under `Saved/Backups/CardDescriptions`, and checks that gameplay tuning is preserved. Copy is limited to 125 characters; this is an editorial budget, not a pixel-perfect layout test.

Run the same commandlet in a fresh process with `-ValidateCardDescriptions` to verify all saved descriptions without modifying assets. Reports are written to `Saved/CardDescriptionsChanges.json` and `Saved/CardDescriptionsValidation.json`.

The September 2026 pass rewrote 44 cards and retained 13 already compact cards. Long examples and implementation details were removed while retaining the main effects and saved numerical values. Preview the longest stance and synergy descriptions in the level-up menu after changing card fonts or layout. Reload externally changed assets or restart the editor before previewing, and repackage for packaged builds.
