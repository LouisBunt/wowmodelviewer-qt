-- MVLink -- reads the look the character actually shows and turns it into a code.
--
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- Rebuilt from scratch after three rounds of patching the old reader. What follows is not
-- written against documentation -- documentation was wrong twice -- but against two kinds
-- of ground truth: return values measured in this player's running 12.1 client, and the
-- shipped source of addons that demonstrably do this (Narcissus reads outfits exactly this
-- way in Preload.lua, MountsJournal runs the same view/read/restore cycle from a dropdown
-- with the transmog window closed).
--
-- The one idea that matters: since 12.0 the look does NOT live on the equipment slots.
-- C_Transmog.GetSlotVisualInfo answers appliedSourceID=0 for every slot of a visibly
-- transmogged character, and that answer is honest -- the appearance lives in an OUTFIT
-- (C_TransmogOutfitInfo), a layer the old per-slot API cannot see. Reading it means:
-- bring the outfit into view, read each slot, put the view back.
--
-- The primary route to ModelViewer is the clipboard: the player copies the code and the
-- application picks it up by itself. SavedVariables are still written, but WoW only
-- flushes them on /reload or logout -- with the code that was loaded BEFORE the reload --
-- so the file is a stale-by-design fallback and is treated as such.

local ADDON, MVLink = ...

MVLink.FORMAT = "MVM1"

-- Bumped on every change a diagnosis depends on; written into MVLinkDB and shown by
-- /mvlink dump. WoW's flush order (old code writes, then new code loads) means a freshly
-- installed build never reports on its first reload -- the stamp turns "which version wrote
-- this?" from a deduction into a glance.
MVLink.BUILD = 10

-- --------------------------------------------------------------------------------------
-- Small helpers

local NO_TRANSMOG = (Constants and Constants.Transmog and Constants.Transmog.NoTransmogID)
                    or 0

-- Enum.TransmogOutfitDisplayType, verified against the generated API docs and Narcissus:
-- 0 Unassigned, 1 Assigned, 2 Equipped, 3 Hidden, 4 Disabled.
local DISPLAY_HIDDEN = 3

-- A sourceID (ItemModifiedAppearanceID) resolved to what the code carries. itemModID is
-- the same ItemAppearanceModifierID vocabulary ModelViewer resolves out of Wowhead bonus
-- ids (0 normal, 1 heroic, 3 mythic, 4 LFR), so no translation happens anywhere.
local function pieceFromSource(sourceID)
  if type(sourceID) ~= "number" or sourceID <= 0 or sourceID == NO_TRANSMOG then
    return nil
  end
  if not (C_TransmogCollection and C_TransmogCollection.GetSourceInfo) then
    return nil
  end
  local ok, info = pcall(C_TransmogCollection.GetSourceInfo, sourceID)
  if not ok or type(info) ~= "table" or not info.itemID or info.itemID == 0 then
    return nil
  end
  -- "Hidden helm"-style entries are real sources whose whole purpose is to show nothing.
  -- Passing one on would equip an invisible item in ModelViewer.
  if info.isHideVisual then
    return nil
  end
  return { itemID = info.itemID, modID = info.itemModID or 0 }
end

-- --------------------------------------------------------------------------------------
-- The outfit layer

-- Outfit slot numbers are their own vocabulary (CHESTSLOT is 4 there, 5 in the inventory).
-- Read from the client at runtime -- a transcribed table would be one patch from wrong.
-- Only primary appearance entries: the secondary shoulder and the illusion list (second
-- return value) are more detail than one MVM1 slot can carry.
function MVLink:OutfitSlotMap()
  if self._outfitSlots then
    return self._outfitSlots
  end
  local map = {}
  local api = C_TransmogOutfitInfo
  if type(api) == "table" and type(api.GetAllSlotLocationInfo) == "function" then
    local ok, list = pcall(api.GetAllSlotLocationInfo)
    if ok and type(list) == "table" then
      for _, e in ipairs(list) do
        if type(e) == "table" and e.slotName and e.slot ~= nil
           and not e.isSecondary and (e.type == nil or e.type == 0) then
          map[e.slotName] = e.slot
        end
      end
    end
  end
  self._outfitSlots = map
  return map
end

-- Whether it is safe to move the viewed outfit around. ChangeViewedOutfit only changes
-- what the transmog FRAME looks at, never what the player wears (Blizzard's own comment in
-- Blizzard_Transmog.lua draws exactly that line) -- but if the player has the window open
-- with unsaved changes, yanking the view would visibly discard their selection. Reading
-- silently is not worth eating someone's pending transmog.
local function viewSwitchIsSafe()
  local api = C_TransmogOutfitInfo
  if type(api) == "table" and type(api.HasPendingOutfitTransmogs) == "function" then
    local ok, pending = pcall(api.HasPendingOutfitTransmogs)
    if ok and pending then
      return false
    end
  end
  return true
end

-- Brings an outfit into view, runs fn, restores the previous view -- restore also on
-- error, which is why fn runs under pcall. The restore is the etiquette every shipped
-- addon that does this follows (MountsJournal, ConditionsUI.lua).
function MVLink:WithOutfitViewed(outfitID, fn)
  local api = C_TransmogOutfitInfo
  if type(api) ~= "table" or type(api.ChangeViewedOutfit) ~= "function"
     or type(outfitID) ~= "number" or outfitID <= 0 or not viewSwitchIsSafe() then
    return false
  end
  local okPrev, prev = pcall(api.GetCurrentlyViewedOutfitID)
  local changed = false
  if not okPrev or prev ~= outfitID then
    changed = pcall(api.ChangeViewedOutfit, outfitID)
    if not changed then
      return false
    end
  end
  local okRun = pcall(fn)
  if changed and okPrev and type(prev) == "number" then
    pcall(api.ChangeViewedOutfit, prev)
  end
  return okRun
end

-- One slot of the outfit currently in view.
--
-- The option argument is per slot, not a constant: armour takes None (0), weapons take
-- whatever GetEquippedSlotOptionFromTransmogSlot answers for THEIR slot (1 for this
-- player's main hand). Reusing the main hand's option for the off hand is how both
-- weapons once came back identical, so each slot strictly asks for its own.
function MVLink:ReadViewedSlot(outfitSlot)
  local api = C_TransmogOutfitInfo
  if type(api) ~= "table" or type(api.GetViewedOutfitSlotInfo) ~= "function" then
    return nil
  end
  local option = 0
  if type(api.GetEquippedSlotOptionFromTransmogSlot) == "function" then
    local ok, o = pcall(api.GetEquippedSlotOptionFromTransmogSlot, outfitSlot)
    if ok and type(o) == "number" then
      option = o
    end
  end
  local t = (Enum and Enum.TransmogType and Enum.TransmogType.Appearance) or 0
  local ok, info = pcall(api.GetViewedOutfitSlotInfo, outfitSlot, t, option)
  if not ok or type(info) ~= "table" then
    return nil
  end
  if info.displayType == DISPLAY_HIDDEN then
    return { hidden = true }
  end
  local p = pieceFromSource(info.transmogID)
  if p then
    p.src = "outfit"
    return p
  end
  -- displayType Equipped (2) with no usable id: the outfit shows the worn item there.
  if info.displayType == 2 then
    return { useEquipped = true }
  end
  return nil
end

-- --------------------------------------------------------------------------------------
-- The fallback: per-slot transmog, then the equipped item

-- CreateTransmogLocation works with the slot name and with the inventory number; measured.
-- GetTransmogLocation exists too but answers nil without erroring -- never use it.
local function slotLocation(invSlot)
  if not (TransmogUtil and TransmogUtil.CreateTransmogLocation) then
    return nil
  end
  local t = (Enum and Enum.TransmogType and Enum.TransmogType.Appearance) or 0
  local m = (Enum and Enum.TransmogModification and Enum.TransmogModification.Main) or 0
  local ok, loc = pcall(TransmogUtil.CreateTransmogLocation, invSlot, t, m)
  if ok and type(loc) == "table" then
    return loc
  end
  return nil
end

local function readSlotTransmog(invSlot)
  local loc = slotLocation(invSlot)
  if not loc or not (TransmogUtil and TransmogUtil.GetInfoForEquippedSlot) then
    return nil
  end
  local ok, info = pcall(TransmogUtil.GetInfoForEquippedSlot, loc)
  if not ok or type(info) ~= "table" then
    return nil
  end
  local p = pieceFromSource(info.appliedSourceID) or pieceFromSource(info.selectedSourceID)
  if p then
    p.src = "transmog"
  end
  return p
end

local function readEquipped(invSlot)
  local itemID = GetInventoryItemID("player", invSlot)
  if type(itemID) == "number" and itemID > 0 then
    -- No appearance modifier travels this way; 0 is the item's base look.
    return { itemID = itemID, modID = 0, src = "equipped" }
  end
  return nil
end

-- --------------------------------------------------------------------------------------
-- Reading a whole look

-- outfitID nil = the look the character shows right now (its active outfit, or plain gear
-- when none is active). A specific outfitID reads that saved outfit instead.
--
-- The fallback rules differ on purpose: for the CURRENT look an untouched slot shows the
-- worn item, so falling back to it is correct. For a saved outfit an untouched slot means
-- "this outfit says nothing here" -- padding it with whatever the reader's character wears
-- would invent pieces the outfit does not contain.
function MVLink:ReadLook(outfitID)
  local api = C_TransmogOutfitInfo
  local isCurrent = (outfitID == nil)
  local activeID
  if type(api) == "table" and type(api.GetActiveOutfitID) == "function" then
    local ok, id = pcall(api.GetActiveOutfitID)
    if ok and type(id) == "number" then
      activeID = id
    end
  end
  local targetID = outfitID or activeID

  local look = {
    raceID = select(3, UnitRace("player")) or 0,
    sex = (UnitSex("player") or 2) - 2,        -- UnitSex: 2 male, 3 female -> 0/1
    pieces = {},
    sources = {},
    worn = 0,
  }

  -- One view switch around all thirteen slots, not thirteen switches.
  local viewed = {}
  local outfitRead = false
  if type(targetID) == "number" and targetID > 0 then
    local slotsByName = self:OutfitSlotMap()
    outfitRead = self:WithOutfitViewed(targetID, function()
      for _, invSlot in ipairs(MVLink.SLOT_ORDER) do
        local name = MVLink.SLOT_APINAME and MVLink.SLOT_APINAME[invSlot]
        local outfitSlot = name and slotsByName[name]
        if outfitSlot ~= nil then
          viewed[invSlot] = MVLink:ReadViewedSlot(outfitSlot)
        end
      end
    end)
  end

  for _, invSlot in ipairs(self.SLOT_ORDER) do
    if GetInventoryItemID("player", invSlot) then
      look.worn = look.worn + 1
    end
    local v = outfitRead and viewed[invSlot] or nil
    local piece
    if v and v.hidden then
      piece = nil                                -- deliberately shows nothing
    elseif v and v.itemID then
      piece = v
    elseif v and v.useEquipped then
      piece = readEquipped(invSlot)
    elseif isCurrent then
      piece = readSlotTransmog(invSlot) or readEquipped(invSlot)
    end
    if piece and piece.itemID then
      look.pieces[self.SLOT_MAP[invSlot]] = piece
      look.sources[invSlot] = piece.src or "?"
    end
  end
  look.fromOutfit = outfitRead and targetID or nil
  return look
end

-- --------------------------------------------------------------------------------------
-- Encoding

-- MVM1:R=<race>:S=<sex>:<mvSlot>=<itemID>.<modID>:...  Emitted in SLOT_ORDER so the same
-- look always yields the same string; the decoder in ModelViewer's MVLinkCode.cpp is the
-- other half of this contract.
function MVLink:Encode(look)
  local out = { self.FORMAT, ("R=%d"):format(look.raceID), ("S=%d"):format(look.sex) }
  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local p = look.pieces[self.SLOT_MAP[invSlot]]
    if p then
      out[#out + 1] = ("%d=%d.%d"):format(self.SLOT_MAP[invSlot], p.itemID, p.modID)
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
-- Saved outfits

-- 12.0 emptied C_TransmogCollection of every outfit function and moved the lot into
-- C_TransmogOutfitInfo; the probe counted zero left behind. Several outfits genuinely
-- share a name ("Outfit" six times on this account), so the player-facing index goes into
-- the label -- two entries a user cannot tell apart are worse than a clumsy name.
function MVLink:Outfits()
  local api = C_TransmogOutfitInfo
  if type(api) ~= "table" or type(api.GetOutfitsInfo) ~= "function" then
    return {}
  end
  local ok, list = pcall(api.GetOutfitsInfo)
  if not ok or type(list) ~= "table" then
    return {}
  end
  local out = {}
  for _, e in ipairs(list) do
    if type(e) == "table" and e.outfitID then
      out[#out + 1] = {
        id = e.outfitID,
        name = ((e.name and e.name ~= "") and e.name or "Ohne Namen")
               .. " #" .. tostring(e.playerFacingOutfitIndex or e.outfitID),
      }
    end
  end
  return out
end

function MVLink:ActiveOutfitID()
  local api = C_TransmogOutfitInfo
  if type(api) == "table" and type(api.GetActiveOutfitID) == "function" then
    local ok, id = pcall(api.GetActiveOutfitID)
    if ok and type(id) == "number" and id > 0 then
      return id
    end
  end
  return nil
end

-- --------------------------------------------------------------------------------------
-- SavedVariables

-- The file is the FALLBACK route, not the main one: WoW flushes it only on /reload or
-- logout, and with the previously loaded code at that. The clipboard route exists because
-- of exactly this. Still written, so "Direkt aus WoW holen" in ModelViewer has something
-- to find -- with updatedAt beside it so the receiving side can say how old it is.
function MVLink:Store()
  MVLinkDB = MVLinkDB or {}
  MVLinkDB.version = 2
  MVLinkDB.build = self.BUILD

  local look = self:ReadLook(nil)
  local count = self:CountPieces(look)

  -- Never trade a good code for an empty one. Store() runs at logout on every character,
  -- and one logout on an undressed bank alt used to destroy the last usable look.
  if count == 0 and type(MVLinkDB.current) == "string"
     and MVLinkDB.current:find("=%d+%.%d+") then
    MVLinkDB.lastEmpty = date("%Y-%m-%d %H:%M:%S")
    return
  end

  MVLinkDB.current = self:Encode(look)
  MVLinkDB.currentPieces = count
  MVLinkDB.updatedAt = date("%Y-%m-%d %H:%M:%S")
  MVLinkDB.lastEmpty = nil
end

-- --------------------------------------------------------------------------------------
-- Diagnosis: a window to copy from, never chat, never the file

local dumpFrame

local function createDumpFrame()
  local f = CreateFrame("Frame", "MVLinkDumpFrame", UIParent, "BasicFrameTemplateWithInset")
  f:SetSize(760, 520)
  f:SetPoint("CENTER")
  f:SetMovable(true)
  f:EnableMouse(true)
  f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:SetFrameStrata("DIALOG")
  f:SetToplevel(true)
  f.title = f:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
  f.title:SetPoint("TOP", f, "TOP", 0, -5)
  f.title:SetText("MVLink-Diagnose — Strg+A, Strg+C")
  local sf = CreateFrame("ScrollFrame", "MVLinkDumpScroll", f, "UIPanelScrollFrameTemplate")
  sf:SetPoint("TOPLEFT", f.Inset, "TOPLEFT", 6, -6)
  sf:SetPoint("BOTTOMRIGHT", f.Inset, "BOTTOMRIGHT", -26, 6)
  local eb = CreateFrame("EditBox", nil, sf)
  eb:SetMultiLine(true)
  eb:SetFontObject(GameFontHighlightSmall)
  eb:SetWidth(700)
  eb:SetAutoFocus(false)
  eb:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
  sf:SetScrollChild(eb)
  f.editBox = eb
  f:Hide()
  return f
end

function MVLink:Dump()
  local look = self:ReadLook(nil)
  local lines = {
    "MVLink build " .. tostring(self.BUILD),
    "aktives Outfit: " .. tostring(self:ActiveOutfitID())
      .. (look.fromOutfit and ("  (gelesen aus Outfit " .. look.fromOutfit .. ")")
                          or "  (Outfit-Ebene NICHT gelesen)"),
    "",
    "CODE: " .. self:Encode(look),
    "",
    "--- Quelle pro Slot ---",
  }
  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local p = look.pieces[self.SLOT_MAP[invSlot]]
    lines[#lines + 1] = ("%-12s inv=%2d  equipped=%s  ->  %s"):format(
      self.SLOT_NAME[invSlot] or "?", invSlot,
      tostring(GetInventoryItemID("player", invSlot)),
      p and (p.itemID .. "." .. p.modID .. " [" .. (p.src or "?") .. "]") or "nichts")
  end
  lines[#lines + 1] = ""
  lines[#lines + 1] = "--- gespeicherte Outfits ---"
  for _, o in ipairs(self:Outfits()) do
    lines[#lines + 1] = ("  %d  %s"):format(o.id, o.name)
  end
  dumpFrame = dumpFrame or createDumpFrame()
  dumpFrame.editBox:SetText(table.concat(lines, "\n"))
  dumpFrame.editBox:HighlightText()
  dumpFrame:Show()
  dumpFrame.editBox:SetFocus()
end

-- --------------------------------------------------------------------------------------
-- Lifecycle

local f = CreateFrame("Frame")
f:RegisterEvent("PLAYER_LOGIN")
f:RegisterEvent("PLAYER_LOGOUT")
f:SetScript("OnEvent", function(_, event)
  if event == "PLAYER_LOGIN" then
    if MVLink.InitUI then
      MVLink:InitUI()
    end
    MVLink:Store()
  elseif event == "PLAYER_LOGOUT" then
    -- Last chance this session; the flush happens right after this returns. autoStore off
    -- means the player asked us not to touch the file.
    if not MVLinkDB or MVLinkDB.autoStore ~= false then
      MVLink:Store()
    end
  end
end)

SLASH_MVLINK1 = "/mvlink"
SlashCmdList["MVLINK"] = function(msg)
  local cmd = (msg or ""):lower():match("^%s*(%S*)")
  if cmd == "dump" then
    MVLink:Dump()
  elseif cmd == "store" then
    MVLink:Store()
    print("|cffa855f7MVLink|r: abgelegt — die Datei schreibt WoW erst beim /reload "
          .. "oder Ausloggen. Schneller: /mvlink, Code kopieren.")
  else
    if MVLink.Toggle then
      MVLink:Toggle()
    end
  end
end
