#ifndef THEME_H
#define THEME_H

// The one place colours are defined.
//
// They used to live in seven near-identical blocks -- one per panel -- plus about 155 raw
// hex values scattered through the stylesheet strings. Changing the accent meant finding
// all of them, and near-duplicates (#1c2229 against #1c222a, six shades of border) made
// that a losing game. Everything below is referenced by name; a new scheme is this file.
//
// NOT here on purpose: the item quality colours in CharacterPanel and ItemBrowser. Those
// are World of Warcraft's own semantics -- purple means epic, and it means that whatever
// the interface looks like around it.
namespace tok {

// --- surfaces, darkest to lightest -------------------------------------------
const char* const kApp      = "#0b0d10";   // window background
const char* const kVoid     = "#080a0d";   // behind the viewport, darkest thing there is
const char* const kPanel    = "#0e1114";   // side columns
const char* const kBar      = "#0f1216";   // title bar bottom, tool bar, status bar
const char* const kCard     = "#14181e";   // raised surfaces, inputs, menus
const char* const kCardAlt  = "#12161b";   // inactive chips
const char* const kRaised   = "#1c2229";   // buttons, hovered menu entries
const char* const kRaised2  = "#232a33";   // and their hover

// --- lines --------------------------------------------------------------------
const char* const kBorder   = "#23282f";
const char* const kBorder2  = "#1c2128";   // subtler, for the title bar's underline

// --- type ---------------------------------------------------------------------
const char* const kText     = "#e8eaee";
const char* const kTextSoft = "#b6bdc8";
const char* const kMuted    = "#8a93a0";
const char* const kDim      = "#5f6874";
const char* const kFaint    = "#4c545e";   // disabled

// --- accent -------------------------------------------------------------------
// Violet, replacing the gold this started with. It carries selection, active chips,
// the export button and text highlighting, so it changes the whole feel of the app.
const char* const kAccent   = "#a855f7";
const char* const kAccentHi = "#c084fc";   // hover / border on the accent button
const char* const kAccentBg = "#1e1030";   // accent on a surface, e.g. an active chip
const char* const kAccentBr = "#4c2a75";   // that surface's border
const char* const kAccentSel= "#1a1226";   // list row selection tint
// White, not near-black: the old #17130a only worked because gold is a light colour.
const char* const kOnAccent = "#ffffff";

// --- title bar ------------------------------------------------------------------
// Deliberately the darkest band in the window. It used to be the lightest (#14181e over
// #0f1216), which put the frame in front of the content -- the opposite of what a frame
// is for. The grain over it is drawn at runtime, not shipped as an image: a few lines of
// paintEvent against a .qrc, a binary blob and a build step.
const char* const kTitleTop  = "#0a0c10";
const char* const kTitleBot  = "#050709";

// --- signals ------------------------------------------------------------------
const char* const kDanger   = "#ef4444";   // close button on hover
const char* const kOk       = "#5bbd7a";   // the CASC pill's dot

}  // namespace tok

#endif  // THEME_H
