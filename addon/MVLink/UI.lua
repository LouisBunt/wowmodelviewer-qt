-- MVLink -- the window, and the button in the transmog frame.
--
-- SPDX-License-Identifier: GPL-3.0-or-later

local ADDON, MVLink = ...

local WIDTH, HEIGHT = 460, 300

-- --------------------------------------------------------------------------------------

local function makeLabel(parent, text, template)
  local fs = parent:CreateFontString(nil, "ARTWORK", template or "GameFontNormal")
  fs:SetText(text)
  return fs
end

function MVLink:BuildUI()
  if self.frame then
    return
  end

  local f = CreateFrame("Frame", "MVLinkFrame", UIParent, "BasicFrameTemplateWithInset")
  f:SetSize(WIDTH, HEIGHT)
  f:SetPoint("CENTER")
  f:SetMovable(true)
  f:EnableMouse(true)
  f:RegisterForDrag("LeftButton")
  f:SetScript("OnDragStart", f.StartMoving)
  f:SetScript("OnDragStop", f.StopMovingOrSizing)
  f:SetFrameStrata("DIALOG")
  f:Hide()
  f.title = makeLabel(f, "An ModelViewer senden")
  f.title:SetPoint("TOP", 0, -5)
  tinsert(UISpecialFrames, "MVLinkFrame")     -- Escape closes it, like every other window

  -- Which look. A dropdown rather than a list: with one worn look plus a handful of
  -- outfits a list would be mostly empty space.
  local picker = CreateFrame("Frame", "MVLinkPicker", f, "UIDropDownMenuTemplate")
  picker:SetPoint("TOPLEFT", 4, -30)
  UIDropDownMenu_SetWidth(picker, WIDTH - 60)
  f.picker = picker

  -- The code. A multi-line edit box, because a long code in a single line scrolls out of
  -- sight and people copy half of it.
  local scroll = CreateFrame("ScrollFrame", "MVLinkScroll", f, "UIPanelScrollFrameTemplate")
  scroll:SetPoint("TOPLEFT", 14, -62)
  scroll:SetSize(WIDTH - 46, 120)

  local edit = CreateFrame("EditBox", nil, scroll)
  edit:SetMultiLine(true)
  edit:SetFontObject(ChatFontNormal)
  edit:SetWidth(WIDTH - 50)
  edit:SetAutoFocus(false)
  -- Escape must leave the box before UISpecialFrames can close the window.
  edit:SetScript("OnEscapePressed", function(self) self:ClearFocus(); f:Hide() end)
  -- Nothing may edit the code by accident: any change puts the real one back.
  edit:SetScript("OnTextChanged", function(self, user)
    if user then
      self:SetText(f.code or "")
      self:HighlightText()
    end
  end)
  scroll:SetScrollChild(edit)
  f.edit = edit

  f.hint = makeLabel(f, "", "GameFontHighlightSmall")
  f.hint:SetPoint("TOPLEFT", 14, -190)
  f.hint:SetWidth(WIDTH - 30)
  f.hint:SetJustifyH("LEFT")
  f.hint:SetJustifyV("TOP")
  f.hint:SetHeight(60)

  local copy = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
  copy:SetSize(150, 22)
  copy:SetPoint("BOTTOMLEFT", 14, 14)
  copy:SetText("Markieren (Strg+C)")
  copy:SetScript("OnClick", function()
    f.edit:SetFocus()
    f.edit:HighlightText()
  end)

  local store = CreateFrame("Button", nil, f, "UIPanelButtonTemplate")
  store:SetSize(180, 22)
  store:SetPoint("BOTTOMRIGHT", -14, 14)
  store:SetText("Für ModelViewer ablegen")
  store:SetScript("OnClick", function()
    MVLink:Store()
    -- Saying "saved" would be a lie: WoW only writes SavedVariables on /reload or logout.
    -- Promising otherwise is how people conclude the addon is broken.
    print("|cffa855f7MVLink|r: bereitgelegt. Wirksam nach |cffffffff/reload|r oder Ausloggen "
          .. "-- danach in ModelViewer auf \"Aus WoW übernehmen\".")
  end)

  self.frame = f
  self:Refresh()
end

-- --------------------------------------------------------------------------------------

function MVLink:ShowLook(look)
  local f = self.frame
  if not f or not look then
    return
  end
  f.code = self:Encode(look)
  f.edit:SetText(f.code)
  f.edit:HighlightText()

  local n = self:CountPieces(look)
  local hint = ("%d Teile erkannt."):format(n)
  if look.missing > 0 then
    hint = hint .. (" %d getragene Teile haben kein eigenes Aussehen und fehlen.")
                     :format(look.missing)
  end
  -- Stated every time, not hidden in a readme: the API does not expose face, hair or skin
  -- outside the barber shop, so those cannot travel with the look.
  hint = hint .. "\n\nGesicht, Frisur und Hautfarbe kann ein Addon nicht auslesen -- "
              .. "in ModelViewer bleiben sie, wie sie dort eingestellt sind."
  f.hint:SetText(hint)
end

function MVLink:Refresh()
  local f = self.frame
  if not f then
    return
  end

  local looks = self:AllLooks()
  self.looks = looks

  UIDropDownMenu_Initialize(f.picker, function(_, level)
    for i, look in ipairs(looks) do
      local info = UIDropDownMenu_CreateInfo()
      info.text = ("%s (%d Teile)"):format(look.name, MVLink:CountPieces(look))
      info.func = function()
        UIDropDownMenu_SetSelectedID(f.picker, i)
        MVLink:ShowLook(looks[i])
      end
      UIDropDownMenu_AddButton(info, level)
    end
  end)
  UIDropDownMenu_SetSelectedID(f.picker, 1)
  UIDropDownMenu_SetText(f.picker, looks[1] and looks[1].name or "-")
  self:ShowLook(looks[1])
end

function MVLink:Toggle()
  self:BuildUI()
  if self.frame:IsShown() then
    self.frame:Hide()
  else
    self:Refresh()          -- re-read: the look may have changed since it was last open
    self.frame:Show()
  end
end

-- --------------------------------------------------------------------------------------
-- The button in the transmog window, where the look is actually made.
--
-- Blizzard_Collections is load-on-demand, so the frame does not exist at login. Waiting
-- for its ADDON_LOADED is the supported way in; poking at WardrobeFrame before that
-- silently does nothing.

local loader = CreateFrame("Frame")
loader:RegisterEvent("ADDON_LOADED")
loader:SetScript("OnEvent", function(_, _, name)
  if name ~= "Blizzard_Collections" or MVLink.wardrobeButton then
    return
  end
  local host = _G.WardrobeFrame or _G.WardrobeCollectionFrame
  if not host then
    return
  end

  local b = CreateFrame("Button", nil, host, "UIPanelButtonTemplate")
  b:SetSize(150, 22)
  b:SetPoint("BOTTOMRIGHT", -8, 4)
  b:SetText("An ModelViewer")
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
