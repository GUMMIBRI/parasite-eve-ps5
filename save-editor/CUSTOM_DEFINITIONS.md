# Custom item and effect definitions

The save editor is independent of the mods installed in the game. It never loads
mod code or inspects the game's enabled-mod list. Definitions describe how to
show/create saved data; installing a definition does not implement its gameplay
behavior. The corresponding game mod is still required to use custom content.

Built in: Bargain (17), Scavenger (18), Lucky Find (19), Salvager (20),
Smoke Grenade (0x18), and Panacea (0x19). These are available without the economy
or rebalance mod being installed. PE Enhancements weapon rebalancing uses the
original weapon-effect IDs, so those effects remain in the normal weapon list;
F1 settings and weapon-specific attack bonuses are not stored in an equipment
slot and are not changed by this editor. Salvager's implementation pays +50%
before the cap (the old shared-header +25% comment is stale).

## Independent data-only definition file

Start the editor with:

```text
pe-save-editor.exe --definitions path/to/my-definitions.json
```

Or put `editor-definitions.json` beside the editor executable. An explicit path
takes precedence. One pack loads at startup; restart to change it. Use the same
pack for GUI and CLI operations. `editor-definitions.example.json` is a small
example; it is not installed automatically.

A pack can override existing definitions or supply names/templates for unused
IDs. Custom item choices appear in the bag, Wayne storage and equipment pickers;
custom effects appear in the appropriate armor or weapon selector and listings.
No game source checkout, installed mod, network connection or disc extraction
is needed to read a pack. There is no script/plugin execution.

```json
{
  "format_version": 1,
  "items": [
    {
      "id": 25,
      "name": "Panacea (mod)",
      "description": "Restores 400 HP and cures ailments with PE Enhancements.",
      "category": "Consumable"
    }
  ],
  "effects": [
    {
      "id": 20,
      "name": "Salvager (mod)",
      "description": "Dismantle Junk +50% before the cap.",
      "armor": true,
      "group": 0
    }
  ]
}
```

IDs are decimal JSON numbers: item IDs 1..255, effect IDs 1..31. Armor and weapon
effects have separate namespaces. `group` is the unshifted 0..7 stored group
number; group 0 is unrestricted. The metadata describes the encoding; it does
not change game rules or automatically remove other effects from a record.
Duplicate IDs, unsupported pack versions, oversized files, invalid categories
and invalid equipment templates are rejected before opening a save.

Supported category names are the editor's `ItemCategory` values: `Ammo`,
`Consumable`, `StatUpgrade`, `Tool`, `Crate`, `MeleeWeapon`, `Handgun`, `Shotgun`,
`MachineGun`, `Rifle`, `GrenadeLauncher`, `RocketLauncher`, `Armor`, `KeyItem`,
`TradingCard`, and `ParasiteEnergy`.

### Custom weapons and armor

An equipment definition must include `equipment_template`, an array of exactly
32 byte values matching the native saved equipment record. It supplies the
actual base stats, flags, icon/class, loaded ammo, slot counts and factory effects.
The saved ID at byte 4 must match `id`; kind at byte 6 must match `category`;
slot counts at bytes 1 and 20 must be consistent and at most 10. Weapon icons
must match their class; armor icons must be 17..22.

A template is required so the editor never invents stats or copies an unrelated
retail weapon when creating custom equipment. An explicit template takes priority
over retail/disc templates. It is used only when creating/replacing an item, not
to rewrite every matching weapon already in the save. Other equipment fields
and effects remain the saved instance's values when merely viewing/editing it.

The native save still limits item IDs to a byte and effects to five bits. A mod
that changes those storage rules needs new serializer support, not just a JSON
file. Packs also do not define new mod-record payload editors: those records
remain opaque unless a schema module supports them.

### Unknown content and preservation

Without a definition, an unknown effect displays its ID/raw byte; it is not
converted to Empty. Existing item IDs/equipment records and unknown mod records
remain preserved during ordinary edits. A reused retail item ID can only be
identified accurately with the matching definition pack: the save has no
universal mod-name registry. The editor cannot infer what unknown code does.

Selecting a replacement or explicitly removing an effect changes that slot.
Editing another slot or an equipment stat does not normalize unknown effect
IDs/group bits or hidden tail bytes. Definitions do not unlock effects in the
Mod Catalog or enable/disable any game mod.

## Junk counter

The Mod Save Data page edits the saved economy Junk balance (0..99,999), separate
from the bag's Junk items. It supports fixed `PELT` v1-v4, a legacy version-0 PELT
record inside PEMC, and `pe_economy.loot` schema 1 (36 bytes).

Changing the balance retains difficulty, pity, pending rewards and the other
payload bytes. Existing legacy versions stay at their version; their internal
checksum is updated. PEMC edits update record/container/game checksums and keep
unrelated records. Unsupported or corrupt data is not overwritten.

If no counter exists, changing its value initializes one with Normal difficulty
and empty state. An unknown nonzero legacy range is never treated as free space.
The editor does not consume bag Junk, convert Super/Duper Junk, or change the
saved economy on/off flag. The game performs conversions according to its own
mod settings when the save is played.