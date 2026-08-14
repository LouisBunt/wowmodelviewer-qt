-- MVLink -- the window. One job: pick a look, copy its code.
--
-- SPDX-License-Identifier: GPL-3.0-or-later
--
-- Rebuilt with the rest of the addon. What is gone on purpose: the PlayerModel preview
-- (ModelViewer IS the preview -- a second, worse one in-game was a source of errors and
-- nothing else) and every settings toggle nobody read. What remains is a list of looks on
-- the left and the code on the right, already highlighted, because the entire flow is
-- Strg+C -- ModelViewer picks the code up from the clipboard by itself.

local ADDON, MVLink = ...

-- The violet the desktop side uses (Theme.h kAccent), so the two halves read as one tool.
local ACCENT = { 0.66, 0.33, 0.97 }
local FONT = STANDARD_TEXT_FONT or [[Fonts\FRIZQT__.TTF]]

local frame

local function setFont(fs, size, r, g, b)
  -- Three arguments always: SetFont(file, height, nil) is an error on 12.x, and because
  -- every label runs through here, that single mistake once rendered the whole window
  -- empty. "" means no flags.
  fs:SetFont(FONT, size, "")
  if r then
    fs:SetTextColor(r, g, b)
  end
end

local function label(parent, size, r, g, b)
  local fs = parent:CreateFontString(nil, "OVERLAY")
  setFont(fs, size, r, g, b)
  return fs
end

-- --------------------------------------------------------------------------------------
-- Window

local ROW_H = 22

local function buildFrame()
  local f = CreateFrame("Frame", "MVLinkFrame", UIParent, "BackdropTemplate")
  f:SetSize(600, 420)
  f:SetPoint("CENTER")
  f:SetMovable(true)
  f:EnableMouse(true)
  f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:SetClampedToScreen(true)
  -- HIGH + Toplevel, or the window vanishes behind every Blizzard panel that opens after
  -- it -- "the layers don't work" was a real complaint against the first version.
  f:SetFrameStrata("HIGH")
  f:SetToplevel(true)
  f:SetBackdrop({
    bgFile = [[Interface\Buttons\WHITE8x8]],
    edgeFile = [[Interface\Buttons\WHITE8x8]],
    edgeSize = 1,
  })
  f:SetBackdropColor(0.055, 0.05, 0.09, 0.97)
  f:SetBackdropBorderColor(ACCENT[1], ACCENT[2], ACCENT[3], 0.65)

  -- ESC closes it like every other panel; the global name makes that work.
  tinsert(UISpecialFrames, "MVLinkFrame")

  local title = label(f, 15, ACCENT[1], ACCENT[2], ACCENT[3])
  title:SetPoint("TOPLEFT", 14, -12)
  title:SetText("MVLink")

  local sub = label(f, 11, 0.75, 0.73, 0.82)
  sub:SetPoint("LEFT", title, "RIGHT", 10, -1)
  sub:SetText("Look kopieren — ModelViewer übernimmt ihn automatisch")

  local close = CreateFrame("Button", nil, f, "UIPanelCloseButton")
  close:SetPoint("TOPRIGHT", 2, 2)

  -- Left: the looks. First the worn one, then every saved outfit.
  local list = CreateFrame("Frame", nil, f, "BackdropTemplate")
  list:SetPoint("TOPLEFT", 14, -40)
  list:SetPoint("BOTTOMLEFT", 14, 14)
  list:SetWidth(210)
  list:SetBackdrop({ bgFile = [[Interface\Buttons\WHITE8x8]] })
  list:SetBackdropColor(0.09, 0.08, 0.14, 0.9)
  f.list = list
  f.rows = {}

  -- Right: the code, pre-highlighted, and what it contains.
  local codeHead = label(f, 11, 0.75, 0.73, 0.82)
  codeHead:SetPoint("TOPLEFT", list, "TOPRIGHT", 14, -2)
  codeHead:SetText("Code — Strg+C, mehr ist nicht nötig:")

  local codeBox = CreateFrame("Frame", nil, f, "BackdropTemplate")
  codeBox:SetPoint("TOPLEFT", codeHead, "BOTTOMLEFT", 0, -6)
  codeBox:SetPoint("RIGHT", f, "RIGHT", -14, 0)
  codeBox:SetHeight(170)
  codeBox:SetBackdrop({
    bgFile = [[Interface\Buttons\WHITE8x8]],
    edgeFile = [[Interface\Buttons\WHITE8x8]],
    edgeSize = 1,
  })
  codeBox:SetBackdropColor(0.03, 0.03, 0.06, 1)
  codeBox:SetBackdropBorderColor(0.25, 0.2, 0.4, 1)

  local scroll = CreateFrame("ScrollFrame", "MVLinkCodeScroll", codeBox,
                             "UIPanelScrollFrameTemplate")
  scroll:SetPoint("TOPLEFT", 8, -8)
  scroll:SetPoint("BOTTOMRIGHT", -28, 8)
  local edit = CreateFrame("EditBox", nil, scroll)
  edit:SetMultiLine(true)
  edit:SetAutoFocus(false)
  edit:SetWidth(300)
  setFont(edit, 11, 0.92, 0.9, 0.98)
  edit:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
  -- The box is a viewer, not an input: typing would silently break the code, so any
  -- edit is thrown away and the real text restored.
  edit:SetScript("OnTextChanged", function(self, user)
    if user and f.currentCode then
      self:SetText(f.currentCode)
      self:HighlightText()
    end
  end)
  scroll:SetScrollChild(edit)
  f.edit = edit

  f.info = label(f, 11, 0.6, 0.58, 0.68)
  f.info:SetPoint("TOPLEFT", codeBox, "BOTTOMLEFT", 2, -10)
  f.info:SetPoint("RIGHT", f, "RIGHT", -16, 0)
  f.info:SetJustifyH("LEFT")
  f.info:SetWordWrap(true)

  local hint = label(f, 10, 0.45, 0.43, 0.52)
  hint:SetPoint("BOTTOMLEFT", list, "BOTTOMRIGHT", 16, 2)
  hint:SetPoint("RIGHT", f, "RIGHT", -16, 0)
  hint:SetJustifyH("LEFT")
  hint:SetWordWrap(true)
  hint:SetText("Gesicht und Frisur kann ein Addon nicht auslesen — die bleiben in "
               .. "ModelViewer, wie sie dort eingestellt sind. Diagnose: /mvlink dump")
  return f
end

local function selectRow(f, index)
  for i, row in ipairs(f.rows) do
    if i == index then
      row.bg:SetColorTexture(ACCENT[1], ACCENT[2], ACCENT[3], 0.25)
    else
      row.bg:SetColorTexture(1, 1, 1, 0.03)
    end
  end
end

local function showLook(f, entry, index)
  selectRow(f, index)
  local look = MVLink:ReadLook(entry.id)          -- nil id = the worn look
  local code = MVLink:Encode(look)
  local count = MVLink:CountPieces(look)
  f.currentCode = code
  f.edit:SetText(code)
  f.edit:HighlightText()
  f.edit:SetFocus()

  local from
  if look.fromOutfit then
    from = "aus Outfit " .. look.fromOutfit .. " gelesen"
  elseif entry.id then
    from = "|cffff5555Outfit-Ebene nicht lesbar — Slot-Daten stattdessen|r"
  else
    from = "getragene Ausrüstung samt Slot-Transmog"
  end
  f.info:SetText(count .. " Teile — " .. from .. ".")
end

local function rebuildRows(f)
  for _, row in ipairs(f.rows) do
    row:Hide()
  end
  wipe(f.rows)

  local entries = { { id = nil, name = "Aktuell getragen" } }
  local active = MVLink:ActiveOutfitID()
  for _, o in ipairs(MVLink:Outfits()) do
    entries[#entries + 1] = {
      id = o.id,
      name = (o.id == active and "|cffa855f7●|r " or "") .. o.name,
    }
  end

  for i, entry in ipairs(entries) do
    local row = CreateFrame("Button", nil, f.list)
    row:SetSize(f.list:GetWidth() - 8, ROW_H)
    row:SetPoint("TOPLEFT", 4, -4 - (i - 1) * (ROW_H + 2))
    row.bg = row:CreateTexture(nil, "BACKGROUND")
    row.bg:SetAllPoints()
    row.bg:SetColorTexture(1, 1, 1, 0.03)
    row.text = label(row, 11, 0.85, 0.83, 0.9)
    row.text:SetPoint("LEFT", 8, 0)
    row.text:SetPoint("RIGHT", -4, 0)
    row.text:SetJustifyH("LEFT")
    row.text:SetText(entry.name)
    row:SetScript("OnClick", function()
      showLook(f, entry, i)
    end)
    f.rows[i] = row
  end
  return entries
end

function MVLink:Toggle()
  frame = frame or buildFrame()
  if frame:IsShown() then
    frame:Hide()
    return
  end
  frame:Show()
  local entries = rebuildRows(frame)
  -- The worn look immediately: the most common case should cost zero clicks.
  showLook(frame, entries[1], 1)
end

-- --------------------------------------------------------------------------------------
-- Minimap button

function MVLink:InitUI()
  if self.minimapButton then
    return
  end
  local b = CreateFrame("Button", "MVLinkMinimapButton", Minimap)
  b:SetSize(31, 31)
  b:SetFrameLevel(8)
  b:RegisterForClicks("AnyUp")
  b:RegisterForDrag("LeftButton")
  b:SetHighlightTexture([[Interface\Minimap\UI-Minimap-ZoomButton-Highlight]])

  local overlay = b:CreateTexture(nil, "OVERLAY")
  overlay:SetSize(53, 53)
  overlay:SetTexture([[Interface\Minimap\MiniMap-TrackingBorder]])
  overlay:SetPoint("TOPLEFT")
  local icon = b:CreateTexture(nil, "BACKGROUND")
  icon:SetSize(20, 20)
  icon:SetTexture([[Interface\Icons\INV_Misc_Book_09]])
  icon:SetPoint("CENTER", -1, 1)
  icon:SetVertexColor(ACCENT[1] + 0.2, ACCENT[2] + 0.2, ACCENT[3])

  local function position(angle)
    local rad = math.rad(angle or 200)
    b:SetPoint("CENTER", Minimap, "CENTER",
               80 * math.cos(rad), 80 * math.sin(rad))
  end
  position(MVLinkDB and MVLinkDB.minimapAngle)

  b:SetScript("OnDragStart", function(self)
    self:SetScript("OnUpdate", function(self)
      local mx, my = Minimap:GetCenter()
      local cx, cy = GetCursorPosition()
      local scale = Minimap:GetEffectiveScale()
      local angle = math.deg(math.atan2(cy / scale - my, cx / scale - mx))
      MVLinkDB = MVLinkDB or {}
      MVLinkDB.minimapAngle = angle
      position(angle)
    end)
  end)
  b:SetScript("OnDragStop", function(self)
    self:SetScript("OnUpdate", nil)
  end)
  b:SetScript("OnClick", function()
    MVLink:Toggle()
  end)
  b:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("MVLink", ACCENT[1], ACCENT[2], ACCENT[3])
    GameTooltip:AddLine("Look für ModelViewer kopieren", 0.8, 0.8, 0.8)
    GameTooltip:Show()
  end)
  b:SetScript("OnLeave", function()
    GameTooltip:Hide()
  end)
  self.minimapButton = b
end
