-- MVLink -- slot mapping, the one place it is written down.
--
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- WoW inventory slot -> ModelViewer CharSlots. Copied from
-- upstream/Source/games/wow/wow_enums.h, where the enum order is NOT the order the
-- equipment list shows -- CS_BOOTS comes third, not seventh. Guessing from the panel
-- would put boots where the belt belongs, so this is transcribed, not derived.
--
-- The receiving side (WowheadDressingRoom / MVLink decoder in ModelViewer) carries the
-- same table with the same comment. If one changes, both change.

local ADDON, MVLink = ...

MVLink.SLOT_MAP = {
  [1]  = 0,   -- INVSLOT_HEAD      -> CS_HEAD
  [3]  = 1,   -- INVSLOT_SHOULDER  -> CS_SHOULDER
  [8]  = 2,   -- INVSLOT_FEET      -> CS_BOOTS
  [6]  = 3,   -- INVSLOT_WAIST     -> CS_BELT
  [4]  = 4,   -- INVSLOT_BODY      -> CS_SHIRT
  [7]  = 5,   -- INVSLOT_LEGS      -> CS_PANTS
  [5]  = 6,   -- INVSLOT_CHEST     -> CS_CHEST
  [9]  = 7,   -- INVSLOT_WRIST     -> CS_BRACERS
  [10] = 8,   -- INVSLOT_HAND      -> CS_GLOVES
  [16] = 9,   -- INVSLOT_MAINHAND  -> CS_HAND_RIGHT
  [17] = 10,  -- INVSLOT_OFFHAND   -> CS_HAND_LEFT
  [15] = 11,  -- INVSLOT_BACK      -> CS_CAPE
  [19] = 12,  -- INVSLOT_TABARD    -> CS_TABARD
}

-- Iterated in this order so a generated code always lists slots the same way. Handy when
-- comparing two codes by eye, and it makes the test against a Wowhead import readable.
MVLink.SLOT_ORDER = { 1, 3, 8, 6, 4, 7, 5, 9, 10, 16, 17, 15, 19 }

-- For the "n of m pieces" line and for naming a slot in an error.
MVLink.SLOT_NAME = {
  [1]  = "Kopf",      [3]  = "Schulter",  [8]  = "Füße",
  [6]  = "Gürtel",    [4]  = "Hemd",      [7]  = "Beine",
  [5]  = "Brust",     [9]  = "Armschienen", [10] = "Hände",
  [16] = "Waffenhand", [17] = "Schildhand", [15] = "Umhang",
  [19] = "Wappenrock",
}
