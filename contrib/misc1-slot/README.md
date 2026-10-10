# Misc 1 accessory slot: build tools

These build the `Misc1_Accessory_Slot` mod: a new accessory slot, **Misc 1**, with its own
body mesh slot (so its item layers with hats, glasses and socks) and its own tab on the
wardrobe's Accessories page. No game data is kept here; the tools read it from the game.

## What the mod changes

| Asset (bundle) | Change |
|---|---|
| `gameplay/characters/data/recipe/skaterslots` (`cas_main_sharedbundle`) | a `cust_misc1` customization slot, copied from `cust_hats` |
| `items/cust_misc1/cust_misc1` (`cas_main_sharedbundle`) | new item type, display name "Misc 1" |
| `items/itemmastercollection` (`cas_main_sharedbundle`) | registers the test item |
| `items/cust_misc1/own_misc1_test_beaniewatch_00009` (`cas_main_sharedbundle`) | test item: the beanie mesh with the free beanie 00001's preset |
| `gameplay/characters/data/recipetemplate/casrecipetemplate` (`bam_levelroot`, `dingolevel_splash`) | a `Misc1` port on the new slot |
| `gameplay/characters/data/recipe/computedrecipes/computedrecipe_casrecipetemplate*` (40, `delmargameconfiguration` in `globals.toc`) | an empty Misc 1 equip at the end of each (`misc1_recipes`): they list one equip per template port, in the template's order, and with the port added to the template alone the skater is not drawn |
| `characters/maincharacters/generic/cas/common/slots/cas_misc1_slot` (`bam_levelroot`) | new body mesh slot `CAS_Misc1_Slot` |
| `characters/maincharacters/generic/cas/common/loaders/cas_rsp_lualoader_prefab` (`bam_levelroot`) | `Misc1` -> `CAS_Misc1_Slot` in the loader's slot list |
| `characters/lua/skaterlualoaderasset` (`bam_levelroot`, resource) | `SkaterLoader.lua` gains `misc1_loader.lua`, called from `loader.loadAsset` after headgear |
| `ui/features/mystuff/flow/ms_skater_slots_contentresources` (`bam_coregameassets`) | `MS_Skater_Accessories_Misc1Slot/_Stub/_Tab`, copied from Headwear |
| `ui/features/mystuff/ms_presentationtemplate_contentresources` (`bam_coregameassets`) | `MS_PresentationTemplate_Accessories_Misc1`, copied from Headwear's and naming the Misc 1 Slot (the wardrobe makes one tab per template, and the template decides which items the grid shows) |
| `ui/features/mystuff/ms_tabbedcontentslotpages_contentresources` (`bam_coregameassets`) | the Misc 1 tab on the Accessories page |

ReSkate's mod merge combines the additions to `SkaterSlots`, the template and the item
master collection with other mods (they are root lists). The loader's slot list, the Lua
script and the wardrobe page are taken whole from the highest-priority mod that changes them.

## For item creators

- Item type: `items/cust_misc1/cust_misc1`, file `bf42bf95-3868-458c-8be5-7b0c97465eb3`,
  instance `be51f6f7-3ec1-4ca1-bb10-e5d13d409243` (stable: the tools derive every new
  identity from a name).
- The item's mesh names its material `misc1_mat`.
- Register the item in `items/itemmastercollection`, like any new cosmetic.
- Item packs depend on the Misc 1 mod.
- An item must not use an appearance preset (its look) that only a store item uses: ReSkate
  refuses such a mod whole ("merge error 0x5343", `Engine/Vfs/mod_store_copies.h`). Meshes are
  shared and fine; give the item a preset of its own, or a free item's.

## Building

1. Decode the base assets listed at the top of `misc1_data.cpp` and the two wardrobe assets
   from the game (ReSkate's `Engine/Resource` readers; the game's archives use Oodle).
2. `misc1_data <base assets> <out>` and `misc1_wardrobe <slots asset> <pages asset> <out> <presentation templates asset>`.
3. Rebuild the script resource: the game's `SkaterLoader.lua` with `misc1_loader.lua` inserted
   before the handwear loader, the guarded call (at the end of that file) after headgear, and the header's source length and
   pointer-table offset (also the resource meta) updated.
4. `pack <spec> <mod dir>`, a spec naming each base TOC, bundle, base manifest and payload.
5. `verify <mod toc> <base toc> <mod cas_01.cas> <bundle>=<base manifest>...` lists what the
   mod changes and adds, as ReSkate's merge will see it.

Add `layout.toc`, `initfs_win32` and `.reskate-studio-patch` from any ReSkate Studio patch for
the same game build, and a `manifest.json`.

Each tool builds from the repository root, e.g.
`g++ -std=c++23 -I. contrib/misc1-slot/pack.cpp Engine/Resource/toc.cpp Engine/Resource/binary_bundle.cpp Engine/Resource/binary_io.cpp`.
