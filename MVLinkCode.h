#ifndef MVLINKCODE_H
#define MVLINKCODE_H

#include <vector>

#include <QString>

// Decoder for the code the MVLink in-game addon produces.
//
//   MVM1:R=6:S=1:0=207200.4:1=202459.1:9=186410.3
//
// Deliberately NOT a Wowhead hash. That one is base-58 and strictly positional over 130+
// fields, so a Wowhead change breaks both ends at once and a shifted layout still parses
// into a plausible but wrong character. This format is readable, order-independent, and
// carries a version so an unknown one is refused rather than guessed at.
//
// The slots are already ModelViewer's own CharSlots values: the addon maps WoW's
// inventory slots on its side, using the table in addon/MVLink/db/slots.lua. That table
// and this decoder are the two halves of one contract -- change one, change the other.
struct MVLinkLook
{
  int version = 0;
  int race = 0;             // ChrRaces.ID, same numbering both sides
  int gender = 0;           // 0 male, 1 female

  struct Piece
  {
    int slot = 0;           // CharSlots
    int itemId = 0;
    int modifier = 0;       // ItemAppearanceModifierID: 0 normal, 1 heroic, 3 mythic, 4 LFR
  };
  std::vector<Piece> pieces;
};

// Reads a code. Returns false and fills `error` with something a person can act on.
// Leading and trailing whitespace is tolerated -- the code arrives via copy and paste.
bool mvlink_parse_code(const QString& code, MVLinkLook* out, QString* error);

// Pulls the code out of the addon's SavedVariables file.
//
// Read by line pattern rather than with a Lua interpreter, which is why the addon writes
// that file flat and one value per line. `outfitName` empty means the worn look
// (`current`); otherwise the named entry from `outfits`.
bool mvlink_read_saved_variables(const QString& path, const QString& outfitName,
                                 QString* codeOut, QString* error);

// Where WoW keeps the file, derived from the installation folder. Returns every candidate
// found, newest first -- one per account, and someone with two accounts has two.
std::vector<QString> mvlink_saved_variable_paths(const QString& wowInstallFolder);

// Copies the bundled addon into the game's AddOns folder.
//
// The setup cannot do this: at install time nobody knows where World of Warcraft lives, and
// the game folder is only chosen later, inside the application. So the addon travels beside
// the exe and is put in place from here -- the same arrangement the Blender add-on uses.
//
// Returns the destination on success, an empty string on failure with `error` filled in.
QString mvlink_install_addon(const QString& wowInstallFolder, QString* error);

#endif
