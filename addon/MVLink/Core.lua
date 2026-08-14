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

-- Building a TransmogLocation, by trying rather than by believing.
--
-- Measured on 12.1: TransmogUtil.GetTransmogLocation(5, 0, 0) returns nil. No error, no
-- warning -- it simply answers nothing, the pcall reports success, and every slot drops to
-- the equipped item. That is what "the wrong transmog" was: not a wrong appearance, the
-- real gear.
--
-- Two likely reasons, and no way to pick between them from documentation that is paywalled
-- or truncated at exactly the signature: "Get" reads an existing location and has nothing
-- to return when none is cached, and "Create" is documented as taking the slot NAME
-- ("CHESTSLOT") where this passed the number 5.
--
-- So all four combinations are tried in order, cheapest and most likely first, and whichever
-- produces a table wins. Getting this wrong twice already cost two rounds; a list that tries
-- everything cannot be wrong in the same way, and it writes down which entry worked so the
-- next version can throw the rest away.
local LOC_METHODS = {
  { "create-name", function(invSlot, apiName, t, m)
      if not (TransmogUtil and TransmogUtil.CreateTransmogLocation and apiName) then return nil end
      return TransmogUtil.CreateTransmogLocation(apiName, t, m)
    end },
  { "create-id", function(invSlot, apiName, t, m)
      if not (TransmogUtil and TransmogUtil.CreateTransmogLocation) then return nil end
      return TransmogUtil.CreateTransmogLocation(invSlot, t, m)
    end },
  { "get-name", function(invSlot, apiName, t, m)
      if not (TransmogUtil and TransmogUtil.GetTransmogLocation and apiName) then return nil end
      return TransmogUtil.GetTransmogLocation(apiName, t, m)
    end },
  { "get-id", function(invSlot, apiName, t, m)
      if not (TransmogUtil and TransmogUtil.GetTransmogLocation) then return nil end
      return TransmogUtil.GetTransmogLocation(invSlot, t, m)
    end },
}

function MVLink:BuildLocation(invSlot)
  local apiName = self.SLOT_APINAME and self.SLOT_APINAME[invSlot]
  local t = (Enum and Enum.TransmogType and Enum.TransmogType.Appearance) or 0
  local m = (Enum and Enum.TransmogModification and Enum.TransmogModification.Main) or 0
  for _, cand in ipairs(LOC_METHODS) do
    local ok, loc = pcall(cand[2], invSlot, apiName, t, m)
    if ok and type(loc) == "table" then
      return loc, cand[1]
    end
  end
  return nil, nil
end

-- One slot, from whichever source this client actually offers. /mvlink debug names the
-- source per slot, so a wrong path shows up as [equipped] where [transmog] belongs.
function MVLink:ReadSlot(invSlot)
  local loc, locMethod = self:BuildLocation(invSlot)
  if locMethod then
    self.locMethod = locMethod          -- recorded once, for the saved diagnosis
  end

  -- A source id can arrive as a plain number or, on newer clients, inside a table. Both
  -- shapes are unwrapped here, INSIDE the guarded path -- doing it afterwards would hand
  -- a table to GetSourceInfo and turn a quiet fallback into a hard error.
  local function fromAny(v)
    if type(v) == "table" then
      v = v.appliedSourceID or v.sourceID or v.baseSourceID or v.appearanceID
    end
    if type(v) ~= "number" then return nil end
    return pieceFromSource(v)
  end

  if loc then
    -- 1) the applied transmog appearance
    if TransmogUtil.GetInfoForEquippedSlot then
      local ok, a, b = pcall(TransmogUtil.GetInfoForEquippedSlot, loc)
      if ok then
        -- Returns appliedSourceID first; b is the visual id and only used as a fallback
        -- when the first came back empty.
        local p = fromAny(a) or fromAny(b)
        if p then p.src = "transmog/" .. locMethod; return p end
      elseif not self.warned then
        self.warned = true
        print("|cffa855f7MVLink|r: Transmog-Abfrage fehlgeschlagen: " .. tostring(a))
      end
    end

    -- 2) the slot's visual info
    if C_Transmog and C_Transmog.GetSlotVisualInfo then
      local ok, r = pcall(C_Transmog.GetSlotVisualInfo, loc)
      if ok then
        local p = fromAny(r)
        if p then p.src = "slotvisual/" .. locMethod; return p end
      end
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

-- --------------------------------------------------------------------------------------
-- Probe
--
-- Writes what THIS client actually offers into SavedVariables, as plain strings.
--
-- The transmog reading has now been wrong twice, both times because it was written against
-- a signature taken from documentation instead of from the running game -- and both times
-- the pcall turned that into silence rather than an error. 12.0 moved things again:
-- C_Transmog.GetSlotVisualInfo returns ONE table where it used to return seven values, and
-- a whole C_TransmogOutfitInfo namespace appeared. Rather than guess a third time, this
-- records the ground truth where it can be read off disk, without anyone retyping chat.

-- tostring() on a secret value is not safe to assume, and a probe that errors is worthless.
local function describe(v)
  if issecretvalue and issecretvalue(v) then
    return "<SECRET>"
  end
  local t = type(v)
  if t == "table" then
    local keys = {}
    for k, sub in pairs(v) do
      local sv
      if issecretvalue and issecretvalue(sub) then
        sv = "<SECRET>"
      elseif type(sub) == "table" then
        sv = "<table>"
      else
        sv = tostring(sub)
      end
      keys[#keys + 1] = tostring(k) .. "=" .. sv
    end
    table.sort(keys)
    return "{" .. table.concat(keys, ", ") .. "}"
  end
  return t .. ":" .. tostring(v)
end

function MVLink:Probe()
  local out = {}
  local function note(s) out[#out + 1] = s end

  note("client=" .. tostring((select(4, GetBuildInfo()))))

  -- Which of the candidates exist at all. A missing name here explains a silent fallback
  -- more cleanly than any amount of reading return values.
  local names = {
    { "TransmogUtil", "GetTransmogLocation" },
    { "TransmogUtil", "GetInfoForEquippedSlot" },
    { "TransmogUtil", "CreateTransmogLocation" },
    { "C_Transmog", "GetSlotVisualInfo" },
    { "C_Transmog", "GetSlotInfo" },
    { "C_TransmogCollection", "GetSourceInfo" },
    { "C_TransmogCollection", "GetAppearanceSourceInfo" },
    { "C_TransmogOutfitInfo", "GetViewedOutfitSlotInfo" },
    { "C_TransmogOutfitInfo", "GetActiveOutfitID" },
  }
  for _, n in ipairs(names) do
    local tbl = _G[n[1]]
    if type(tbl) ~= "table" then
      note(n[1] .. " = FEHLT")
    else
      note(n[1] .. "." .. n[2] .. " = " .. type(tbl[n[2]]))
    end
  end
  note("Enum.TransmogType=" .. describe(Enum and Enum.TransmogType))
  note("Enum.TransmogModification=" .. describe(Enum and Enum.TransmogModification))

  -- The whole namespace, listed rather than guessed at.
  --
  -- Saved outfits have been arriving empty the entire time, and the reason is almost
  -- certainly that 12.0 moved outfit handling out of C_TransmogCollection into
  -- C_TransmogOutfitInfo -- OutfitIDs() still hunts for GetOutfits/GetOutfitIDs/
  -- GetAllOutfitIDs, none of which exist any more. pairs() over the table answers what the
  -- replacements are called without anyone reading a paywalled wiki page.
  local function dumpNamespace(name, filter)
    local t = _G[name]
    if type(t) ~= "table" then
      note(name .. " = FEHLT")
      return
    end
    local fns = {}
    for k, v in pairs(t) do
      if type(v) == "function" and (not filter or tostring(k):find(filter)) then
        fns[#fns + 1] = tostring(k)
      end
    end
    table.sort(fns)
    note(name .. " (" .. #fns .. "): " .. table.concat(fns, ", "))
  end
  dumpNamespace("C_TransmogOutfitInfo")
  dumpNamespace("C_TransmogCollection", "Outfit")
  dumpNamespace("C_TransmogSets", nil)

  -- One slot, end to end, with every return value spelled out. Chest (5) rather than head:
  -- a hidden helm is a common setting and would make an empty answer look like a fault.
  local invSlot = 5
  note("--- Slot " .. invSlot .. " (Brust) ---")
  note("GetInventoryItemID=" .. describe(GetInventoryItemID("player", invSlot)))

  -- Every candidate, reported individually. The winner is what ReadSlot uses, but the
  -- losers are the interesting part when the winner is "none".
  local t = (Enum and Enum.TransmogType and Enum.TransmogType.Appearance) or 0
  local m = (Enum and Enum.TransmogModification and Enum.TransmogModification.Main) or 0
  local apiName = self.SLOT_APINAME and self.SLOT_APINAME[invSlot]
  note("apiName=" .. tostring(apiName) .. " type=" .. tostring(t) .. " mod=" .. tostring(m))
  for _, cand in ipairs(LOC_METHODS) do
    local ok, r = pcall(cand[2], invSlot, apiName, t, m)
    note("loc " .. cand[1] .. ": ok=" .. tostring(ok) .. " -> " .. describe(r))
  end

  local loc = self:BuildLocation(invSlot)

  if loc then
    if TransmogUtil and type(TransmogUtil.GetInfoForEquippedSlot) == "function" then
      local r = { pcall(TransmogUtil.GetInfoForEquippedSlot, loc) }
      note("GetInfoForEquippedSlot n=" .. #r)
      for i = 1, #r do note("  [" .. i .. "] " .. describe(r[i])) end
    end
    if C_Transmog and type(C_Transmog.GetSlotVisualInfo) == "function" then
      local r = { pcall(C_Transmog.GetSlotVisualInfo, loc) }
      note("GetSlotVisualInfo n=" .. #r)
      for i = 1, #r do note("  [" .. i .. "] " .. describe(r[i])) end
    end
  else
    note("loc ist nil -- beide Transmog-Pfade werden uebersprungen, "
         .. "das ist die Ursache fuer das falsche Aussehen")
  end

  MVLinkDB = MVLinkDB or {}
  MVLinkDB.probe = out
  return out
end

-- What the character is wearing right now, including unsaved changes in the transmog
-- window -- which is the whole point, and the one thing the armory route cannot do.
function MVLink:ReadWornLook()
  local look = {
    name = "Aktuell getragen",
    raceID = select(3, UnitRace("player")) or 0,
    sex = (UnitSex("player") or 2) - 2,      -- UnitSex: 2 male, 3 female -> 0/1
    pieces = {},
    worn = 0,
  }

  for _, invSlot in ipairs(self.SLOT_ORDER) do
    -- Counted separately from the pieces, because the two can disagree and the difference
    -- is the only useful diagnosis. The old "missing" counter could not: it incremented
    -- only when ReadSlot returned nil AND an item was equipped, and ReadSlot's last path
    -- returns a piece for every equipped item unconditionally -- so it was always 0.
    if GetInventoryItemID("player", invSlot) then
      look.worn = look.worn + 1
    end
    local piece = self:ReadSlot(invSlot)
    if piece then
      look.pieces[self.SLOT_MAP[invSlot]] = piece
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

  local look = self:ReadWornLook()
  local code = self:Encode(look)
  local count = self:CountPieces(look)

  -- A look with no pieces is useless to ModelViewer, which refuses it outright. Writing it
  -- anyway would destroy a good code that is already in the file -- and Store() runs on
  -- every login, so one login on a bank alt was enough to lose it. The old one is kept and
  -- the reason recorded, rather than quietly leaving the newer timestamp on stale content.
  if count == 0 and type(MVLinkDB.current) == "string"
     and MVLinkDB.current:find("=%d+%.%d+") then
    MVLinkDB.lastEmpty = date("%Y-%m-%d %H:%M:%S")
    MVLinkDB.lastEmptyWorn = look.worn
    print("|cffa855f7MVLink|r: Aussehen nicht lesbar (" .. look.worn
          .. " Slots belegt, 0 erkannt) — der zuletzt gute Code bleibt erhalten. "
          .. "/mvlink debug zeigt warum.")
    return
  end

  MVLinkDB.updatedAt = date("%Y-%m-%d %H:%M:%S")
  MVLinkDB.current = code
  MVLinkDB.currentPieces = count
  MVLinkDB.lastEmpty = nil
  MVLinkDB.lastEmptyWorn = nil

  -- Which path each slot actually came from, written down rather than printed. "equipped"
  -- everywhere means the transmog was never read and the code carries the gear instead of
  -- the look -- the difference between "no code" and "wrong code", and invisible until it
  -- is recorded somewhere that survives the session.
  local srcs = {}
  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local p = look.pieces[self.SLOT_MAP[invSlot]]
    if p then
      srcs[#srcs + 1] = (self.SLOT_NAME[invSlot] or invSlot) .. "=" .. (p.src or "?")
    end
  end
  MVLinkDB.sources = table.concat(srcs, " ")
  MVLinkDB.locMethod = self.locMethod or "KEINE -- alle vier Wege lieferten nil"
  self:Probe()

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
    -- immediately after this returns. The settings checkbox writes autoStore and until now
    -- nothing read it, so unticking it did nothing at all.
    if not MVLinkDB or MVLinkDB.autoStore ~= false then
      MVLink:Store()
    end
  end
end)

SLASH_MVLINK1 = "/mvlink"
SlashCmdList["MVLINK"] = function(msg)
  local cmd = (msg or ""):lower():match("^%s*(%S*)")
  if cmd == "probe" then
    local out = MVLink:Probe()
    print("|cffa855f7MVLink|r: " .. #out .. " Zeilen aufgezeichnet. Jetzt /reload — "
          .. "danach steht alles in MVLink.lua unter SavedVariables.")
  elseif cmd == "store" then
    MVLink:Store()
    print("|cffa855f7MVLink|r: bereitgelegt — wirksam nach /reload oder Ausloggen.")
  elseif cmd == "debug" then
    -- EVERY slot, empty ones included, with the raw API answer beside the result.
    --
    -- The old version printed only slots that produced a piece, so an empty look printed
    -- one header line and nothing else -- it could not tell "this slot is bare" from "the
    -- API returned nothing for a slot that is not". That is exactly the question this
    -- command exists to answer, so the raw GetInventoryItemID value is printed too, whether
    -- or not anything came of it.
    local look = MVLink:ReadWornLook()
    print(("|cffa855f7MVLink|r: race=%d sex=%d — %d/%d Slots belegt, %d Teile erkannt")
            :format(look.raceID, look.sex, look.worn, #MVLink.SLOT_ORDER,
                    MVLink:CountPieces(look)))
    for _, invSlot in ipairs(MVLink.SLOT_ORDER) do
      local p = look.pieces[MVLink.SLOT_MAP[invSlot]]
      local rawID = GetInventoryItemID("player", invSlot)
      local rawLink = GetInventoryItemLink("player", invSlot)
      local state
      if p then
        state = ("item=%d mod=%d [%s]"):format(p.itemID, p.modID, p.src or "?")
      elseif rawID then
        state = "|cffff5555TEIL GETRAGEN, ABER NICHTS ERKANNT|r"
      else
        state = "leer"
      end
      print(("  %-12s inv=%2d  GetInventoryItemID=%s  link=%s  ->  %s")
              :format(MVLink.SLOT_NAME[invSlot] or "?", invSlot,
                      tostring(rawID), rawLink and "ja" or "nein", state))
    end
    print(MVLink:Encode(look))
    if MVLink:CountPieces(look) == 0 then
      print("|cffa855f7MVLink|r: Kein einziges Teil. Steht oben ueberall "
            .. "GetInventoryItemID=nil, obwohl der Charakter angezogen ist, dann liefert "
            .. "diese Spielversion die Werte nicht mehr so — bitte diese Ausgabe melden.")
    end
  else
    MVLink:Toggle()
  end
end
