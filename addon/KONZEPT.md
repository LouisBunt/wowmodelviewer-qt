# Patch 1 — MVLink: Aussehen aus WoW an ModelViewer übergeben

Konzeptstand: 09.08.2026 · Zielversion WoW Retail 12.x · MV 1.7.0

---

## Vision

Ein WoW-Addon, das das aktuell getragene Aussehen und die gespeicherten
Kleiderschrank-Sets als kurzen Code ausgibt, den ModelViewer: Midnight direkt
übernimmt — ohne Umweg über Wowhead.

**Warum das den Unterschied macht:** Heute führt der Weg über die Wowhead-Anprobe.
Man muss den Look dort nachbauen oder speichern, den Link kopieren und einfügen. Was
man im Spiel bereits trägt, ist der Anwendung nicht zugänglich. Der Armory-Import
liefert zwar den eigenen Charakter, hängt aber an einem Vermittlerdienst, ist auf den
zuletzt ausgeloggten Stand beschränkt und kennt keine ungespeicherten Anproben.

**Abgrenzung zu vorhandenen Addons:** Transmog-Addons wie *TransmogOutfitManager* oder
*Transmog Outfits* verwalten Sets **innerhalb** des Spiels. Keines davon exportiert nach
außen. Der Export-per-Code-Weg ist dagegen etabliert — WeakAuras, TSM und Details
machen es seit Jahren genauso; neu ist nur das Ziel.

---

## Zielnutzer und Leitgedanken

Wer MV benutzt, um seinen eigenen Charakter zu rendern oder nach Blender zu bringen.
Erfahren genug für `/befehle`, aber nicht bereit, Item-IDs abzuschreiben.

1. **Zwei Handgriffe, nicht zehn.** Knopf im Transmog-Fenster → Code → einfügen.
2. **Nichts verschweigen.** Was das Addon nicht liefern kann (siehe Machbarkeit),
   sagt es dem Nutzer, statt es stillschweigend wegzulassen.
3. **Kein Netz, keine Fremdserver.** Alles bleibt zwischen Spiel und Anwendung.
4. **Kein Eingriff ins Spiel.** Reines Lesen und Anzeigen.

---

## Technische Machbarkeit

### Der harte Rahmen

Ein Addon hat **kein Dateisystem** (außer seiner eigenen SavedVariables-Datei) und
**keinen Netzwerkzugriff**. Es gibt keinen Weg, MV direkt anzusprechen. Deshalb die
zwei Wege, die es tatsächlich gibt — beide werden gebaut:

| Weg | Wie | Wirkt |
|---|---|---|
| **Kopieren** | Addon zeigt den Code in einem markierten Textfeld, Strg+C | sofort |
| **Datei** | Addon legt den Code in seine SavedVariables, MV liest sie | erst nach `/reload` oder Logout |

Der zweite Weg ist bequemer, aber **WoW schreibt SavedVariables nur beim Ausloggen und
bei `/reload`** — das ist keine Einschränkung des Addons, sondern des Spiels. Deshalb
ist Kopieren der Hauptweg und die Datei die Bequemlichkeitsvariante.

### Was die API hergibt — geprüft

| Feature | Status | Grundlage |
|---|---|---|
| Getragenes Aussehen je Slot | ✅ | `TransmogUtil.GetInfoForEquippedSlot(slot)` → `appliedSourceID` |
| Source → Item + Farbvariante | ✅ | `C_TransmogCollection.GetSourceInfo(sourceID)` → `itemID`, `itemModID` |
| Gespeicherte Sets | ✅ | `C_TransmogCollection.GetOutfits()`, `GetOutfitInfo(id)`, `GetOutfitItemTransmogInfoList(id)` |
| Rasse, Geschlecht | ✅ | `UnitRace("player")`, `UnitSex("player")` |
| **Charakter-Anpassungen** (Gesicht, Frisur, Hautfarbe) | ❌ | nur über `C_BarberShop`, und das ausschließlich im geöffneten Friseur-Fenster |
| Aussehen anderer Spieler | ⚠️ | nur sichtbare Item-IDs ohne Farbvariante — bewusst weggelassen |
| Combat-Beschränkungen | — | trifft nicht zu, das Addon liest und zeigt nur an |

**`itemModID` ist der Glücksfall:** `0=Normal, 1=Heroic, 3=Mythic/Elite, 4=Raid Finder`
— exakt der Wertebereich, den MV heute schon aus den Wowhead-Bonus-IDs auflöst
(`ItemAppearanceModifierID`, sichtbar im Trace als `slot 0: item 207200 modifier 4`).
Die Farbvariante kommt also ohne jede Umrechnung an.

**Die Lücke ehrlich benannt:** Anpassungen sind nicht auslesbar. Der Import setzt Rasse
und Geschlecht und legt die Ausrüstung an; Gesicht und Frisur bleiben, wie sie in MV
eingestellt sind. Für Rüstungs- und Transmog-Ansichten ist das folgenlos, für ein
Porträt des eigenen Charakters nicht. Das gehört in den Hinweistext beider Seiten und
in die LIESMICH.

---

## Aufbau

### Im Spiel

- **Knopf im Transmogrifikations-Fenster.** Dort, wo der Look entsteht. Angehängt an
  `WardrobeFrame`, nachgeladen über `ADDON_LOADED` von `Blizzard_Collections`.
- **Fenster „An ModelViewer senden".** Oben eine Liste: *Aktuell getragen* plus jedes
  gespeicherte Outfit mit Namen. Darunter das Textfeld mit dem Code, bereits markiert,
  sodass Strg+C genügt. Eine Zeile darunter sagt, wie viele Teile erkannt wurden und
  weist auf die fehlenden Anpassungen hin.
- **Slash-Befehl `/mvlink`** öffnet dasselbe Fenster, auch ohne Transmog-NPC.
- **Minimap-Knopf** — optional, Phase 4.

### In ModelViewer

- Im Reiter *Import/Export* ein Feld **„Code aus WoW"** neben dem vorhandenen
  Wowhead-Feld. Derselbe Ablauf, andere Quelle.
- Knopf **„Aus WoW übernehmen"**, der die SavedVariables-Datei liest. Pfad wird aus dem
  bereits bekannten WoW-Ordner abgeleitet (`userSettings\qt-frontend.ini`,
  Schlüssel `game/installFolder`) und in der INI gemerkt, falls mehrere Accounts
  existieren.
- Neues Flag `--mvlink <code|datei>` für Skripte, analog zu `--dressing-room`.

---

## Das Codeformat

Kein Wowhead-Hash nachbauen. Der ist base-58-kodiert und streng positionell über 130+
Felder — in Lua erzeugbar, aber viel Code für nichts, und jede Wowhead-Änderung bräche
beide Seiten. Stattdessen ein eigenes, lesbares Format:

```
MVM1:R=6:S=1:0=207200.4:1=202459.1:2=207202.4:6=229256.0:9=186410.3
```

| Teil | Bedeutung |
|---|---|
| `MVM1` | Kennung und Formatversion. MV lehnt eine unbekannte Version ab, statt sie zu raten — dieselbe Lehre wie beim Dressing-Room-Decoder. |
| `R=` | Rasse als `raceID` aus `select(3, UnitRace("player"))` |
| `S=` | Geschlecht, `UnitSex("player")` − 2 (0 = männlich, 1 = weiblich) |
| `<slot>=<itemID>.<modID>` | je getragenem Teil, Slot in **MV-Zählung** |

Slot-Zuordnung (WoW `INVSLOT_*` → MV `CharSlots` aus `wow_enums.h`):

| WoW | | MV | | WoW | | MV | |
|---|---|---|---|---|---|---|---|
| 1 | Head | 0 | `CS_HEAD` | 9 | Wrist | 7 | `CS_BRACERS` |
| 3 | Shoulder | 1 | `CS_SHOULDER` | 10 | Hands | 8 | `CS_GLOVES` |
| 8 | Feet | 2 | `CS_BOOTS` | 16 | MainHand | 9 | `CS_HAND_RIGHT` |
| 6 | Waist | 3 | `CS_BELT` | 17 | OffHand | 10 | `CS_HAND_LEFT` |
| 4 | Shirt | 4 | `CS_SHIRT` | 15 | Back | 11 | `CS_CAPE` |
| 7 | Legs | 5 | `CS_PANTS` | 19 | Tabard | 12 | `CS_TABARD` |
| 5 | Chest | 6 | `CS_CHEST` | | | | |

Die Zuordnung gehört **auf beide Seiten** als eine Tabelle mit demselben Kommentar —
sie ist die einzige Stelle, an der die Formate auseinanderlaufen können.

Rassen-IDs sind auf beiden Seiten dieselben (`ChrRaces.ID`), da braucht es keine
Übersetzung.

---

## Lua-Datenschema

```lua
-- db/slots.lua -- die einzige Wahrheit über die Slot-Zuordnung.
-- WoW-Inventarplatz -> MV-CharSlots. Aus upstream/Source/games/wow/wow_enums.h;
-- die Reihenfolge dort ist NICHT die Anzeigereihenfolge, also abschreiben statt raten.
MVLink.SLOT_MAP = {
  [1]  = 0,   -- Head      -> CS_HEAD
  [3]  = 1,   -- Shoulder  -> CS_SHOULDER
  [8]  = 2,   -- Feet      -> CS_BOOTS
  [6]  = 3,   -- Waist     -> CS_BELT
  [4]  = 4,   -- Shirt     -> CS_SHIRT
  [7]  = 5,   -- Legs      -> CS_PANTS
  [5]  = 6,   -- Chest     -> CS_CHEST
  [9]  = 7,   -- Wrist     -> CS_BRACERS
  [10] = 8,   -- Hands     -> CS_GLOVES
  [16] = 9,   -- MainHand  -> CS_HAND_RIGHT
  [17] = 10,  -- OffHand   -> CS_HAND_LEFT
  [15] = 11,  -- Back      -> CS_CAPE
  [19] = 12,  -- Tabard    -> CS_TABARD
}

-- Ein aufgesammelter Look, bevor er zu Text wird.
-- pieces ist nach MV-Slot indiziert, damit doppelte Einträge nicht möglich sind.
---@class MVLinkLook
---@field name    string            -- "Aktuell getragen" oder der Outfit-Name
---@field raceID  number
---@field sex     number            -- 0 männlich, 1 weiblich
---@field pieces  table<number, {itemID:number, modID:number}>
---@field missing number            -- Slots mit Aussehen, das nicht auflösbar war

-- SavedVariables. Bewusst flach: MV muss das ohne Lua-Interpreter lesen können.
MVLinkDB = {
  version   = 1,
  updatedAt = "2026-08-09 14:22:01",   -- date(), nur zur Anzeige
  current   = "MVM1:R=6:S=1:0=207200.4:…",
  outfits   = {
    ["Todesritter Schwarz"] = "MVM1:R=6:S=1:…",
  },
}
```

MV liest die Datei mit einem **Zeilenmuster**, nicht mit einem Lua-Parser: gesucht wird
`current = "…"` und die Einträge unter `outfits`. Das Format ist deshalb absichtlich
einzeilig je Wert und ohne Verschachtelung.

---

## Bauabschnitte

Jeder Abschnitt ist für sich benutzbar; die riskanteren Teile kommen zuletzt.

| # | Inhalt | Ergebnis |
|---|---|---|
| **1** | Addon-Gerüst: `.toc` mit `## Interface:` passend zu 12.x, `/mvlink`, leeres Fenster | lädt und meldet sich |
| **2** | Auslesen des getragenen Looks, Codeerzeugung, Textfeld mit Autoauswahl | Kopieren funktioniert |
| **3** | MV-Seite: Feld „Code aus WoW" + Decoder + `--mvlink` | der Kreis schließt sich |
| **4** | Gespeicherte Outfits als Liste | Sets übertragbar |
| **5** | SavedVariables + „Aus WoW übernehmen" in MV, Pfadermittlung, Accountwahl | ohne Kopieren |
| **6** | Knopf im Transmog-Fenster, Minimap-Knopf, Hinweistexte, englische Texte | rund |

**Reihenfolge mit Absicht:** Abschnitt 3 kommt vor den Outfits, damit früh eine
vollständige Kette steht, an der sich alles Weitere messen lässt.

### Prüfung je Abschnitt

- **2:** Code für einen bekannten Look erzeugen und mit dem vergleichen, was der
  Wowhead-Import desselben Looks in MV ergibt — Item-IDs und Modifier müssen
  übereinstimmen. Das ist der eigentliche Beweis, dass die Zuordnung stimmt.
- **3:** `--mvlink "<code>" --shot` headless, danach dasselbe über
  `--dressing-room` — die beiden Bilder müssen gleich aussehen.
- **5:** SavedVariables nach `/reload` einlesen; mit fehlender Datei, leerer Datei und
  Datei aus einer neueren Formatversion gegenprüfen.

---

## Offene Punkte

1. **Waffen.** Das Transmog-Fenster führt Haupt- und Nebenhand getrennt, MV kennt
   `CS_HAND_RIGHT`/`CS_HAND_LEFT`. Bei Schilden und Nebenhand-Gegenständen ist zu
   prüfen, ob MV sie am richtigen Arm zeigt — beim Wowhead-Import gab es hier schon
   einmal eine Verwechslung (Slot 14).
2. **Verhüllte Slots.** „Kopfbedeckung ausblenden" und „Umhang ausblenden" sind
   Anzeigeeinstellungen. Soll der Code sie mitführen (MV kann Geosets schalten) oder
   ignorieren?
3. **Accountwahl.** Wer mehrere WoW-Accounts hat, hat mehrere WTF-Ordner. Erste
  Fassung: den zuletzt geänderten nehmen und den gewählten Pfad merken.
4. **Name des Addons.** `MVLink` ist ein Arbeitstitel.

---

## Quellen

- [C_TransmogCollection.GetSourceInfo](https://warcraft.wiki.gg/wiki/API_C_TransmogCollection.GetSourceInfo)
  — Feldliste inklusive `itemID` und `itemModID`
- [C_TransmogCollection.GetOutfitInfo](https://warcraft.wiki.gg/wiki/API_C_TransmogCollection.GetOutfitInfo)
- [Kategorie C_TransmogCollection](https://warcraft.wiki.gg/wiki/Category:API_namespaces/C_TransmogCollection)
- [World of Warcraft API](https://warcraft.wiki.gg/wiki/World_of_Warcraft_API)

Die API-Angaben sind gegen die Wiki-Stände vom 09.08.2026 geprüft. Vor Abschnitt 2
gehört ein kurzer Test im laufenden Spiel: `/dump TransmogUtil.GetInfoForEquippedSlot(1)`
zeigt in einer Zeile, ob die Aufrufe im aktuellen Client so heißen.
