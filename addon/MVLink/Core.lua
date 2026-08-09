-- MVLink -- reading the look and turning it into a code.
--
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- An addon cannot write files (beyond its own SavedVariables) and cannot reach the
-- network, so there is no way to hand ModelViewer anything directly. Two routes exist and
-- both are built here: a short code to copy, and the same code parked in SavedVariables
-- for ModelViewer to read. WoW only flushes SavedVariables on /reload or logout, which is
-- why copying is the primary route and the file is the convenience one.

local ADDON, MVLink = ...

MVLink.FORMAT = "MVM1"

-- --------------------------------------------------------------------------------------
-- Reading

-- The appearance actually shown in a slot, as itemID + appearance modifier.
--
-- GetSourceInfo's itemModID is 0=Normal, 1=Heroic, 3=Mythic/Elite, 4=Raid Finder -- the
-- same numbers ModelViewer already resolves out of Wowhead's bonus ids
-- (ItemAppearanceModifierID). That is why the colour variant needs no translation at all.
--
-- Returns nil when the slot is empty, hidden, or holds something with no appearance.
-- "No transmog" is Constants.Transmog.NoTransmogID where that table exists, and the
-- literal 0 or -1 where it does not. Reaching straight into Constants.Transmog would
-- raise on any client that spells it differently, and an error here kills the whole
-- read -- which looks exactly like "the addon does nothing".
local NO_TRANSMOG = (Constants and Constants.Transmog and Constants.Transmog.NoTransmogID)
                    or -1

local function pieceFromSource(sourceID)
  if not sourceID or sourceID == 0 or sourceID == NO_TRANSMOG then
    return nil
  end
  if not C_TransmogCollection or not C_TransmogCollection.GetSourceInfo then
    return nil
  end
  local info = C_TransmogCollection.GetSourceInfo(sourceID)
  if not info or not info.itemID or info.itemID == 0 then
    return nil
  end
  -- isHideVisual marks the "hidden helm" style entries: a real source, but its whole
  -- purpose is to show nothing. Passing it on would equip an invisible item.
  if info.isHideVisual then
    return nil
  end
  return { itemID = info.itemID, modID = info.itemModID or 0 }
end

-- One slot, from whichever source this client actually offers.
--
-- The first attempt reads the TRANSMOG appearance, which is what should travel. On this
-- machine that came back empty for every slot -- the encoded look was "MVM1:R=9:S=0" with
-- no pieces at all -- so the worn item is used as a fallback rather than producing an
-- empty code. A look without its transmog is still far better than nothing, and
-- /mvlink debug names which source each slot came from.
function MVLink:ReadSlot(invSlot)
  -- 1) the applied transmog appearance
  if TransmogUtil and TransmogUtil.GetInfoForEquippedSlot then
    local ok, applied = pcall(TransmogUtil.GetInfoForEquippedSlot, invSlot)
    if ok and applied then
      local p = pieceFromSource(applied)
      if p then p.src = "transmog"; return p end
    end
  end

  -- 2) the transmog info attached to the equipped item
  if C_Transmog and C_Transmog.GetSlotVisualInfo then
    local ok, appliedID = pcall(C_Transmog.GetSlotVisualInfo, invSlot, 0)
    if ok and appliedID then
      local p = pieceFromSource(appliedID)
      if p then p.src = "slotvisual"; return p end
    end
  end

  -- 3) whatever is actually equipped. No appearance modifier is available this way, so
  --    the colour variant defaults to 0 -- the base look of that item.
  local itemID = GetInventoryItemID("player", invSlot)
  if itemID then
    return { itemID = itemID, modID = 0, src = "equipped" }
  end
  return nil
end

-- What the character is wearing right now, including unsaved changes in the transmog
-- window -- which is the whole point, and the one thing the armory route cannot do.
function MVLink:ReadWornLook()
  local look = {
    name = "Aktuell getragen",
    raceID = select(3, UnitRace("player")) or 0,
    sex = (UnitSex("player") or 2) - 2,      -- UnitSex: 2 male, 3 female -> 0/1
    pieces = {},
    missing = 0,
  }

  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local piece = self:ReadSlot(invSlot)
    if piece then
      look.pieces[self.SLOT_MAP[invSlot]] = piece
    elseif GetInventoryItemID("player", invSlot) then
      -- Something IS worn here but produced nothing usable. Counted rather than silently
      -- dropped, so the window can say the look is incomplete.
      look.missing = look.missing + 1
    end
  end
  return look
end

-- A saved wardrobe outfit. GetOutfitItemTransmogInfoList returns one entry per slot in
-- the game's own slot order, so the index has to be turned back into an inventory slot.
function MVLink:ReadOutfit(outfitID)
  local okName, name = pcall(function()
    return C_TransmogCollection.GetOutfitInfo and C_TransmogCollection.GetOutfitInfo(outfitID)
  end)
  if not okName then name = nil end
  local look = {
    name = name or "Unbenannt",
    raceID = select(3, UnitRace("player")) or 0,
    sex = (UnitSex("player") or 2) - 2,
    pieces = {},
    missing = 0,
  }

  local okList, list = pcall(function()
    return C_TransmogCollection.GetOutfitItemTransmogInfoList
           and C_TransmogCollection.GetOutfitItemTransmogInfoList(outfitID)
  end)
  if not okList or not list then
    return look
  end
  -- The list is indexed by transmog slot, which is the inventory slot for everything we
  -- care about. Anything outside SLOT_MAP (rings, trinkets, neck) has no appearance and
  -- is skipped rather than mapped to a wrong slot.
  for invSlot, info in pairs(list) do
    local mvSlot = self.SLOT_MAP[invSlot]
    if mvSlot and info then
      local piece = pieceFromSource(info.appearanceID)
      if piece then
        look.pieces[mvSlot] = piece
      end
    end
  end
  return look
end

-- --------------------------------------------------------------------------------------
-- Encoding

-- MVM1:R=<race>:S=<sex>:<mvSlot>=<itemID>.<modID>:...
--
-- Deliberately not a rebuilt Wowhead hash: that one is base-58 and strictly positional
-- over 130+ fields, so every Wowhead change would break both ends at once. This is
-- readable, and the version tag lets the receiver refuse a format it does not know
-- instead of misreading it -- the same lesson the dressing-room decoder had to learn.
function MVLink:Encode(look)
  local out = { self.FORMAT, ("R=%d"):format(look.raceID), ("S=%d"):format(look.sex) }
  -- Emitted in SLOT_ORDER, not pairs(), so the same look always yields the same string.
  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local mvSlot = self.SLOT_MAP[invSlot]
    local p = look.pieces[mvSlot]
    if p then
      out[#out + 1] = ("%d=%d.%d"):format(mvSlot, p.itemID, p.modID)
    end
  end
  return table.concat(out, ":")
end

function MVLink:CountPieces(look)
  local n = 0
  for _ in pairs(look.pieces) do n = n + 1 end
  return n
end

-- --------------------------------------------------------------------------------------
-- SavedVariables

-- Written flat and one value per line on purpose: ModelViewer reads this with a line
-- pattern, not a Lua interpreter, so nothing here may become a nested table.
function MVLink:Store()
  MVLinkDB = MVLinkDB or {}
  MVLinkDB.version = 1
  MVLinkDB.updatedAt = date("%Y-%m-%d %H:%M:%S")
  MVLinkDB.current = self:Encode(self:ReadWornLook())

  MVLinkDB.outfits = {}
  for _, id in ipairs(self:OutfitIDs()) do
    local look = self:ReadOutfit(id)
    if self:CountPieces(look) > 0 then
      MVLinkDB.outfits[look.name] = self:Encode(look)
    end
  end
end

-- The list of saved outfits, whatever this client calls it.
--
-- C_TransmogCollection.GetOutfits() does NOT exist -- calling it is what took the addon
-- down with "attempt to call a nil value" in both Store() and AllLooks(). The name varies
-- between clients, so it is resolved once at runtime instead of assumed, and a client
-- that has none of them simply offers no saved outfits rather than erroring.
function MVLink:OutfitIDs()
  if self.outfitFn == nil then
    self.outfitFn = false
    for _, name in ipairs({ "GetOutfits", "GetOutfitIDs", "GetAllOutfitIDs" }) do
      if C_TransmogCollection and type(C_TransmogCollection[name]) == "function" then
        self.outfitFn = name
        break
      end
    end
    if not self.outfitFn then
      print("|cffa855f7MVLink|r: gespeicherte Outfits sind in dieser Spielversion nicht "
            .. "abrufbar — der getragene Look funktioniert normal.")
    end
  end
  if not self.outfitFn then
    return {}
  end
  local ok, ids = pcall(C_TransmogCollection[self.outfitFn])
  return (ok and type(ids) == "table") and ids or {}
end

-- Collect everything the window offers: the worn look first, then saved outfits by name.
function MVLink:AllLooks()
  local looks = { self:ReadWornLook() }
  for _, id in ipairs(self:OutfitIDs()) do
    local look = self:ReadOutfit(id)
    if self:CountPieces(look) > 0 then
      looks[#looks + 1] = look
    end
  end
  return looks
end

-- --------------------------------------------------------------------------------------
-- Lifecycle

local f = CreateFrame("Frame")
f:RegisterEvent("PLAYER_LOGIN")
f:RegisterEvent("PLAYER_LOGOUT")
f:SetScript("OnEvent", function(_, event)
  if event == "PLAYER_LOGIN" then
    MVLink:BuildUI()
    MVLink:BuildMinimapButton()
    -- Refreshed at login too, so the file holds something sensible even if the window is
    -- never opened this session.
    MVLink:Store()
  elseif event == "PLAYER_LOGOUT" then
    -- The last chance to catch changes made this session; SavedVariables are flushed
    -- immediately after this returns.
    MVLink:Store()
  end
end)

SLASH_MVLINK1 = "/mvlink"
SlashCmdList["MVLINK"] = function(msg)
  local cmd = (msg or ""):lower():match("^%s*(%S*)")
  if cmd == "store" then
    MVLink:Store()
    print("|cffa855f7MVLink|r: bereitgelegt — wirksam nach /reload oder Ausloggen.")
  elseif cmd == "debug" then
    -- One line per slot, for when the window says something the code does not.
    local look = MVLink:ReadWornLook()
    print(("|cffa855f7MVLink|r: race=%d sex=%d, %d Teile, %d ohne Aussehen")
            :format(look.raceID, look.sex, MVLink:CountPieces(look), look.missing))
    for _, invSlot in ipairs(MVLink.SLOT_ORDER) do
      local p = look.pieces[MVLink.SLOT_MAP[invSlot]]
      if p then
        print(("  %-12s inv=%d mv=%d item=%d mod=%d  [%s]")
                :format(MVLink.SLOT_NAME[invSlot] or "?", invSlot,
                        MVLink.SLOT_MAP[invSlot], p.itemID, p.modID, p.src or "?"))
      end
    end
    print(MVLink:Encode(look))
  else
    MVLink:Toggle()
  end
end
