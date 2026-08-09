-- MVLink -- the window, styled after the MVLink mockups.
--
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- WoW has no stylesheets: everything below is frames, textures and font strings. The
-- mockup's look reduces to a few repeatable pieces -- a dark panel with a hairline
-- border, a cyan section heading numbered "01 ·", and rows that highlight on hover --
-- so those are built once here as helpers and reused.

local ADDON, MVLink = ...

-- Palette from the mockup.
local C = {
  bg      = { 0.039, 0.051, 0.071 },   -- #0a0d12
  panel   = { 0.016, 0.078, 0.122 },   -- #04141f
  row     = { 0.047, 0.063, 0.082 },   -- #0c1015
  line    = { 0.110, 0.329, 0.502 },   -- #1c5480
  cyan    = { 0.229, 0.886, 1.000 },   -- #3ae2ff
  cyanDim = { 0.490, 0.918, 1.000 },   -- #7deaff
  violet  = { 0.780, 0.490, 1.000 },   -- #c77dff
  text    = { 0.933, 0.953, 0.968 },   -- #eef3f7
  dim     = { 0.373, 0.490, 0.584 },   -- #5f7d95
  warn    = { 1.000, 0.239, 0.133 },   -- #ff3d22
}

local W, H = 620, 430

local function tint(f, c, a)
  local t = f:CreateTexture(nil, "BACKGROUND")
  t:SetAllPoints()
  t:SetColorTexture(c[1], c[2], c[3], a or 1)
  return t
end

-- A hairline border, drawn as four one-pixel textures. WoW's backdrop system draws
-- tiled edge art, which cannot produce the flat single-pixel line the mockup uses.
local function hairline(f, c, a)
  for _, p in ipairs({ { "TOPLEFT", "TOPRIGHT", 0, -1 }, { "BOTTOMLEFT", "BOTTOMRIGHT", 0, 1 },
                       { "TOPLEFT", "BOTTOMLEFT", 1, 0 }, { "TOPRIGHT", "BOTTOMRIGHT", -1, 0 } }) do
    local t = f:CreateTexture(nil, "BORDER")
    t:SetColorTexture(c[1], c[2], c[3], a or 0.55)
    t:SetPoint(p[1]); t:SetPoint(p[2])
    if p[3] == 0 then t:SetHeight(1) else t:SetWidth(1) end
  end
end

local function panel(parent, w, h)
  local f = CreateFrame("Frame", nil, parent)
  f:SetSize(w, h)
  tint(f, C.panel, 0.55)
  hairline(f, C.line, 0.5)
  return f
end

local function fs(parent, size, colour, flags)
  local t = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  t:SetFont(STANDARD_TEXT_FONT, size, flags)
  t:SetTextColor(unpack(colour or C.text))
  return t
end

-- "01 · QUELLE" -- the number in cyan, the word in dim caps, a rule to the right edge.
local function heading(parent, num, label, width)
  local h = CreateFrame("Frame", nil, parent)
  h:SetSize(width, 14)
  local n = fs(h, 10, C.cyan, "OUTLINE")
  n:SetPoint("LEFT")
  n:SetText(num)
  local l = fs(h, 10, C.dim)
  l:SetPoint("LEFT", n, "RIGHT", 6, 0)
  l:SetText("· " .. label)
  local rule = h:CreateTexture(nil, "ARTWORK")
  rule:SetColorTexture(C.line[1], C.line[2], C.line[3], 0.4)
  rule:SetHeight(1)
  rule:SetPoint("LEFT", l, "RIGHT", 8, 0)
  rule:SetPoint("RIGHT")
  return h
end

-- A row that lights up under the pointer and can be selected.
local function row(parent, w, h, onClick)
  local b = CreateFrame("Button", nil, parent)
  b:SetSize(w, h)
  b.bg = tint(b, C.row, 0.8)
  b.hl = b:CreateTexture(nil, "ARTWORK")
  b.hl:SetAllPoints()
  b.hl:SetColorTexture(C.cyan[1], C.cyan[2], C.cyan[3], 0.10)
  b.hl:Hide()
  b:SetScript("OnEnter", function(s) s.hl:Show() end)
  b:SetScript("OnLeave", function(s) if not s.selected then s.hl:Hide() end end)
  if onClick then b:SetScript("OnClick", onClick) end
  -- A cyan bar on the left edge marks the selected row.
  b.mark = b:CreateTexture(nil, "OVERLAY")
  b.mark:SetColorTexture(unpack(C.cyan))
  b.mark:SetWidth(2)
  b.mark:SetPoint("TOPLEFT"); b.mark:SetPoint("BOTTOMLEFT")
  b.mark:Hide()
  return b
end

local function setSelected(b, on)
  b.selected = on
  b.mark:SetShown(on)
  if on then b.hl:Show() else b.hl:Hide() end
end

-- --------------------------------------------------------------------------------------

function MVLink:BuildUI()
  if self.frame then return end

  local f = CreateFrame("Frame", "MVLinkFrame", UIParent)
  f:SetSize(W, H)
  f:SetPoint("CENTER")
  f:SetMovable(true); f:EnableMouse(true); f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:SetFrameStrata("DIALOG")
  f:Hide()
  tint(f, C.bg, 0.96)
  hairline(f, C.line, 0.8)
  tinsert(UISpecialFrames, "MVLinkFrame")      -- Escape closes, like every other window

  -- Title bar
  local title = fs(f, 12, C.text, "OUTLINE")
  title:SetPoint("TOPLEFT", 16, -14)
  title:SetText("AN MODELVIEWER: MIDNIGHT SENDEN")
  local tag = fs(f, 9, C.violet)
  tag:SetPoint("LEFT", title, "RIGHT", 8, 0)
  tag:SetText("MVM1")

  local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
  close:SetPoint("TOPRIGHT", -4, -4)
  close:SetScript("OnClick", function() f:Hide() end)

  -- --- left column: portrait and who this is -----------------------------------------
  local left = panel(f, 170, 250)
  left:SetPoint("TOPLEFT", 16, -40)

  -- A live 3D portrait costs one frame and shows immediately whether the addon read the
  -- right character -- cheaper to trust than a name in text.
  local model = CreateFrame("PlayerModel", nil, left)
  model:SetPoint("TOPLEFT", 6, -6)
  model:SetSize(158, 170)
  model:SetUnit("player")
  model:SetPortraitZoom(0.45)
  f.model = model

  f.charName = fs(left, 12, C.cyanDim, "OUTLINE")
  f.charName:SetPoint("TOPLEFT", model, "BOTTOMLEFT", 2, -8)
  f.charLine = fs(left, 9, C.dim)
  f.charLine:SetPoint("TOPLEFT", f.charName, "BOTTOMLEFT", 0, -4)
  f.charCode = fs(left, 9, C.violet)
  f.charCode:SetPoint("TOPLEFT", f.charLine, "BOTTOMLEFT", 0, -4)

  -- --- right column: source, then slots ----------------------------------------------
  local rightX, rightW = 200, W - 216
  f.srcHead = heading(f, "01", "QUELLE", rightW)
  f.srcHead:SetPoint("TOPLEFT", rightX, -44)

  local srcBox = panel(f, rightW, 74)
  srcBox:SetPoint("TOPLEFT", rightX, -62)
  f.srcRows = {}
  for i = 1, 3 do
    local r = row(srcBox, rightW - 8, 20, function()
      if f.srcRows[i].look then MVLink:Select(i) end
    end)
    r:SetPoint("TOPLEFT", 4, -4 - (i - 1) * 22)
    r.name = fs(r, 10, C.text); r.name:SetPoint("LEFT", 10, 0)
    r.count = fs(r, 9, C.dim);  r.count:SetPoint("RIGHT", -10, 0)
    f.srcRows[i] = r
  end

  f.slotHead = heading(f, "02", "SLOTS", rightW)
  f.slotHead:SetPoint("TOPLEFT", rightX, -146)

  local colHead = fs(f, 8, C.dim)
  colHead:SetPoint("TOPRIGHT", f.slotHead, "BOTTOMRIGHT", -10, -3)
  colHead:SetText("ITEM · MOD")

  local slotBox = panel(f, rightW, 132)
  slotBox:SetPoint("TOPLEFT", rightX, -176)
  local scroll = CreateFrame("ScrollFrame", "MVLinkSlotScroll", slotBox,
                             "UIPanelScrollFrameTemplate")
  scroll:SetPoint("TOPLEFT", 4, -4)
  scroll:SetSize(rightW - 30, 124)
  local content = CreateFrame("Frame", nil, scroll)
  content:SetSize(rightW - 30, 1)
  scroll:SetScrollChild(content)
  f.slotContent = content
  f.slotRows = {}

  -- --- the code, hidden until asked for ----------------------------------------------
  local codeBox = panel(f, W - 32, 46)
  codeBox:SetPoint("TOPLEFT", 16, -300)
  local edit = CreateFrame("EditBox", nil, codeBox)
  edit:SetPoint("TOPLEFT", 8, -6); edit:SetSize(W - 52, 34)
  edit:SetMultiLine(true)
  edit:SetFont(STANDARD_TEXT_FONT, 10)
  edit:SetTextColor(unpack(C.cyanDim))
  edit:SetAutoFocus(false)
  edit:SetScript("OnEscapePressed", function(s) s:ClearFocus(); f:Hide() end)
  -- Read-only by restoration: any edit puts the real code straight back.
  edit:SetScript("OnTextChanged", function(s, user)
    if user then s:SetText(f.code or ""); s:HighlightText() end
  end)
  f.edit = edit

  f.hint = fs(f, 9, C.dim)
  f.hint:SetPoint("TOPLEFT", 16, -352)
  f.hint:SetWidth(W - 32)
  f.hint:SetJustifyH("LEFT"); f.hint:SetJustifyV("TOP")
  f.hint:SetHeight(38)

  -- --- buttons -------------------------------------------------------------------------
  local function button(label, w, accent)
    local b = CreateFrame("Button", nil, f)
    b:SetSize(w, 24)
    tint(b, accent and C.cyan or C.row, accent and 0.22 or 0.9)
    hairline(b, accent and C.cyan or C.line, accent and 0.9 or 0.5)
    local t = fs(b, 10, accent and C.cyanDim or C.text)
    t:SetPoint("CENTER")
    t:SetText(label)
    b:SetScript("OnEnter", function(s) s:SetAlpha(0.8) end)
    b:SetScript("OnLeave", function(s) s:SetAlpha(1) end)
    return b
  end

  local showBtn = button("CODE ZEIGEN · STRG+C", 200, true)
  showBtn:SetPoint("BOTTOMLEFT", 16, 14)
  showBtn:SetScript("OnClick", function()
    f.edit:SetFocus(); f.edit:HighlightText()
  end)

  local setBtn = button("EINSTELLUNGEN", 130, false)
  setBtn:SetPoint("BOTTOMLEFT", showBtn, "BOTTOMRIGHT", 8, 0)
  setBtn:SetScript("OnClick", function() MVLink:ToggleSettings() end)

  local storeBtn = button("FÜR MODELVIEWER ABLEGEN", 220, false)
  storeBtn:SetPoint("BOTTOMRIGHT", -16, 14)
  storeBtn:SetScript("OnClick", function()
    MVLink:Store()
    -- "Gespeichert" would be a lie: WoW flushes SavedVariables on /reload and logout and
    -- at no other time. Saying so is the difference between a wait and a bug report.
    print("|cff3ae2ffMVLink|r: bereitgelegt — wirksam nach |cffffffff/reload|r oder "
          .. "Ausloggen. Danach in ModelViewer auf \"Aus WoW übernehmen\".")
  end)

  self.frame = f
  self:Refresh()
end

-- --------------------------------------------------------------------------------------

function MVLink:Select(index)
  local f, look = self.frame, nil
  if not f then return end
  for i, r in ipairs(f.srcRows) do
    setSelected(r, i == index)
    if i == index then look = r.look end
  end
  if not look then return end

  f.code = self:Encode(look)
  f.edit:SetText(f.code)
  f.edit:HighlightText()

  -- Slot list. Rebuilt rather than pooled: at most thirteen rows, and reuse would keep
  -- stale names visible whenever a look has fewer pieces than the one before.
  for _, r in ipairs(f.slotRows) do r:Hide() end
  wipe(f.slotRows)

  local y, n = 0, 0
  for _, invSlot in ipairs(self.SLOT_ORDER) do
    local p = look.pieces[self.SLOT_MAP[invSlot]]
    if p then
      local r = row(f.slotContent, f.slotContent:GetWidth(), 18)
      r:SetPoint("TOPLEFT", 0, -y)
      local name = fs(r, 10, C.text)
      name:SetPoint("LEFT", 8, 0)
      name:SetText(self.SLOT_NAME[invSlot] or ("Slot " .. invSlot))
      local val = fs(r, 10, C.cyanDim)
      val:SetPoint("RIGHT", -8, 0)
      -- The modifier is the colour variant, and it is the part that silently goes wrong,
      -- so it is shown rather than hidden behind the item id.
      val:SetText(("%d · %d"):format(p.itemID, p.modID))
      f.slotRows[#f.slotRows + 1] = r
      y = y + 20
      n = n + 1
    end
  end
  f.slotContent:SetHeight(math.max(y, 1))

  local hint = ("%d Teile."):format(n)
  if look.missing > 0 then
    hint = hint .. (" %d getragene Teile haben kein eigenes Aussehen."):format(look.missing)
  end
  hint = hint .. "\nGesicht, Frisur und Hautfarbe kann ein Addon nicht auslesen — in "
              .. "ModelViewer bleiben sie, wie sie dort eingestellt sind."
  f.hint:SetText(hint)
end

function MVLink:Refresh()
  local f = self.frame
  if not f then return end

  local name = UnitName("player")
  local race = UnitRace("player")
  local sex = (UnitSex("player") == 3) and "WEIBLICH" or "MÄNNLICH"
  f.charName:SetText((name or "?"):upper())
  f.charLine:SetText(("%s · %s"):format((race or "?"):upper(), sex))
  f.charCode:SetText(("R=%d  S=%d")
                       :format(select(3, UnitRace("player")) or 0,
                               (UnitSex("player") or 2) - 2))
  if f.model then f.model:SetUnit("player") end

  local looks = self:AllLooks()
  for i, r in ipairs(f.srcRows) do
    local look = looks[i]
    r.look = look
    if look then
      r:Show()
      r.name:SetText(look.name:upper())
      r.count:SetText(("%d TEILE"):format(self:CountPieces(look)))
    else
      r:Hide()
    end
  end
  -- More outfits than rows is possible; the window shows the worn look plus the two most
  -- recent, and /mvlink store still writes every one of them to the file.
  self:Select(1)
end

function MVLink:Toggle()
  self:BuildUI()
  if self.frame:IsShown() then
    self.frame:Hide()
  else
    self:Refresh()        -- re-read: the look may have changed since it was last open
    self.frame:Show()
  end
end

-- --------------------------------------------------------------------------------------
-- Minimap button

function MVLink:BuildMinimapButton()
  if self.minimapButton then return end
  local b = CreateFrame("Button", "MVLinkMinimapButton", Minimap)
  b:SetSize(28, 28)
  b:SetPoint("TOPLEFT", Minimap, "TOPLEFT", 52, 6)   -- clear of the default clutter
  b:SetFrameStrata("MEDIUM")

  local bg = b:CreateTexture(nil, "BACKGROUND")
  bg:SetAllPoints()
  bg:SetColorTexture(C.bg[1], C.bg[2], C.bg[3], 0.85)
  local label = fs(b, 11, C.cyan, "OUTLINE")
  label:SetPoint("CENTER")
  label:SetText("MV")
  hairline(b, C.cyan, 0.7)

  b:RegisterForClicks("LeftButtonUp", "RightButtonUp")
  b:SetScript("OnClick", function(_, button)
    if button == "RightButton" then
      -- Straight to the code, for the second and every later time.
      MVLink:BuildUI(); MVLink:Refresh(); MVLink.frame:Show()
      MVLink.frame.edit:SetFocus(); MVLink.frame.edit:HighlightText()
    else
      MVLink:Toggle()
    end
  end)
  b:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("MVLink")
    GameTooltip:AddLine("Linksklick — Fenster öffnen", 1, 1, 1)
    GameTooltip:AddLine("Rechtsklick — Code direkt markieren", 1, 1, 1)
    if MVLinkDB and MVLinkDB.updatedAt then
      GameTooltip:AddLine("Abgelegt: " .. MVLinkDB.updatedAt, 0.37, 0.49, 0.58)
    else
      GameTooltip:AddLine("Nichts abgelegt", 1.0, 0.24, 0.13)
    end
    GameTooltip:Show()
  end)
  b:SetScript("OnLeave", GameTooltip_Hide)
  self.minimapButton = b
end

-- --------------------------------------------------------------------------------------
-- The button in the transmog window, where the look is actually made.
--
-- Blizzard_Collections is load-on-demand, so the frame does not exist at login. Waiting
-- for its ADDON_LOADED is the supported way in; touching WardrobeFrame before that
-- silently does nothing.

local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(_, _, name)
  if name ~= "Blizzard_Collections" or MVLink.wardrobeButton then return end
  local host = _G.WardrobeFrame or _G.WardrobeCollectionFrame
  if not host then return end

  local b = CreateFrame("Button", nil, host)
  b:SetSize(150, 24)
  b:SetPoint("BOTTOMRIGHT", -8, 4)
  local bg = b:CreateTexture(nil, "BACKGROUND")
  bg:SetAllPoints()
  bg:SetColorTexture(C.cyan[1], C.cyan[2], C.cyan[3], 0.20)
  local t = b:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  t:SetFont(STANDARD_TEXT_FONT, 11)
  t:SetTextColor(unpack(C.cyanDim))
  t:SetPoint("CENTER")
  t:SetText("AN MODELVIEWER")
  b:SetScript("OnClick", function() MVLink:Toggle() end)
  b:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_TOP")
    GameTooltip:AddLine("An ModelViewer senden")
    GameTooltip:AddLine("Erzeugt einen Code aus dem aktuellen Aussehen.", 1, 1, 1, true)
    GameTooltip:Show()
  end)
  b:SetScript("OnLeave", GameTooltip_Hide)
  MVLink.wardrobeButton = b
end)

-- --------------------------------------------------------------------------------------
-- Settings and history (mockups 1d / 1e), reachable from the main window.

local function checkbox(parent, label, key, default)
  local c = CreateFrame("CheckButton", nil, parent, "UICheckButtonTemplate")
  c:SetSize(20, 20)
  local t = fs(c, 10, C.text)
  t:SetPoint("LEFT", c, "RIGHT", 4, 0)
  t:SetText(label)
  c:SetScript("OnShow", function(s)
    MVLinkDB = MVLinkDB or {}
    if MVLinkDB[key] == nil then MVLinkDB[key] = default end
    s:SetChecked(MVLinkDB[key])
  end)
  c:SetScript("OnClick", function(s)
    MVLinkDB = MVLinkDB or {}
    MVLinkDB[key] = s:GetChecked() and true or false
  end)
  return c
end

function MVLink:BuildSettings()
  if self.settings then return self.settings end

  local f = CreateFrame("Frame", "MVLinkSettings", UIParent)
  f:SetSize(430, 300)
  f:SetPoint("CENTER", 120, 0)
  f:SetMovable(true); f:EnableMouse(true); f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:SetFrameStrata("DIALOG")
  f:Hide()
  tint(f, C.bg, 0.96)
  hairline(f, C.line, 0.8)
  tinsert(UISpecialFrames, "MVLinkSettings")

  local title = fs(f, 12, C.text, "OUTLINE")
  title:SetPoint("TOPLEFT", 16, -14)
  title:SetText("EINSTELLUNGEN")

  local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
  close:SetPoint("TOPRIGHT", -4, -4)
  close:SetScript("OnClick", function() f:Hide() end)

  local w = 430 - 32
  heading(f, "01", "VERHALTEN", w):SetPoint("TOPLEFT", 16, -44)

  local cb1 = checkbox(f, "Beim Ausloggen automatisch ablegen", "autoStore", true)
  cb1:SetPoint("TOPLEFT", 16, -62)
  local cb2 = checkbox(f, "Minimap-Knopf zeigen", "minimap", true)
  cb2:SetPoint("TOPLEFT", 16, -86)
  cb2:HookScript("OnClick", function(s)
    if MVLink.minimapButton then MVLink.minimapButton:SetShown(s:GetChecked()) end
  end)

  heading(f, "02", "ABLAGE FÜR MODELVIEWER", w):SetPoint("TOPLEFT", 16, -122)

  -- The path is built from the account name, which is the one piece ModelViewer has to
  -- find on its own -- showing it here turns "it cannot find the file" into something
  -- the user can compare against.
  local pathBox = panel(f, w, 40)
  pathBox:SetPoint("TOPLEFT", 16, -140)
  f.pathText = fs(pathBox, 9, C.cyanDim)
  f.pathText:SetPoint("TOPLEFT", 8, -6)
  f.pathText:SetWidth(w - 16)
  f.pathText:SetJustifyH("LEFT")
  f.stamp = fs(f, 9, C.dim)
  f.stamp:SetPoint("TOPLEFT", 16, -186)

  local storeNow = CreateFrame("Button", nil, f)
  storeNow:SetSize(140, 22)
  storeNow:SetPoint("TOPLEFT", 16, -206)
  tint(storeNow, C.cyan, 0.22); hairline(storeNow, C.cyan, 0.9)
  local sn = fs(storeNow, 10, C.cyanDim); sn:SetPoint("CENTER"); sn:SetText("JETZT ABLEGEN")
  storeNow:SetScript("OnClick", function()
    MVLink:Store()
    MVLink:RefreshSettings()
    print("|cff3ae2ffMVLink|r: bereitgelegt — wirksam nach /reload oder Ausloggen.")
  end)

  heading(f, "03", "SLASH", w):SetPoint("TOPLEFT", 16, -240)
  local cmds = fs(f, 9, C.dim)
  cmds:SetPoint("TOPLEFT", 16, -258)
  cmds:SetJustifyH("LEFT")
  cmds:SetText("/mvlink — Fenster\n/mvlink store — ablegen\n/mvlink debug — Slots im Chat")

  self.settings = f
  return f
end

function MVLink:RefreshSettings()
  local f = self.settings
  if not f then return end
  -- GetRealmName/account: the folder is WTF\Account\<ACCOUNT>\SavedVariables. The account
  -- name is not exposed to addons, so the placeholder is honest about that.
  f.pathText:SetText("WTF\Account\<Account>\SavedVariables\MVLink.lua")
  f.stamp:SetText(MVLinkDB and MVLinkDB.updatedAt
                    and ("Zuletzt geschrieben " .. MVLinkDB.updatedAt)
                    or "Noch nichts abgelegt")
end

function MVLink:ToggleSettings()
  local f = self:BuildSettings()
  if f:IsShown() then f:Hide() else self:RefreshSettings(); f:Show() end
end
