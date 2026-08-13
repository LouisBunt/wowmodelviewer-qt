Diese Fassung repariert etwas, das seit dem Umbau auf die neue Oberfläche kaputt war und
niemandem aufgefallen ist — und liefert das WoW-Addon endlich mit.

## Animationen spielen jetzt die Animation, die du auswählst

Bisher lief immer dieselbe. Das Frontend hat `currentAnim` nie gesetzt, gerendert wird aber
ausschließlich diese eine Zahl — sie stand seit dem Programmstart auf 0. Die Zeitleiste lief
also auf dem Clip, den du angeklickt hast, die Figur posierte aus Clip 0. Das sah aus wie ein
kurzer Loop, der sich nicht ändern lässt, und war genau das.

Aus derselben Ecke kamen zwei weitere Fehler:

- **Pause hat nur ihr eigenes Symbol umgeschaltet.** Der Anhaltepunkt lag in der alten
  wxWidgets-Zeichenfläche und wurde beim Portieren nicht mitgenommen. Ohne Clip-Auswahl steht
  das Modell jetzt wirklich still — das ist der Zustand für Standbilder und Export.
- **Waffen und Schultern standen auf Frame 0**, während sich der Körper bewegte. Angehängte
  Modelle bekamen nie einen Takt.

Nachweisbar ohne Augenmaß: mit `--shot-frame <n>` liegt Bild *n* immer auf demselben Moment.
Vorher lieferten zwei verschiedene Clips dieselbe Prüfsumme, jetzt nicht mehr.

## MVLink wird mitgeliefert

Das WoW-Addon war bisher in keinem Paket enthalten — die Hälfte in ModelViewer, die andere
nirgends. Es liegt jetzt bei und wird über **Import → „MVLink-Addon in WoW installieren"** ins
Spiel kopiert.

Der Reiter heißt auch nicht mehr „Charakter". Das Wort bezeichnete gleichzeitig den Reiter,
einen Knopf in der Werkzeugzeile und ein Menü — wer den MVLink-Import suchte, hatte keinen
Grund, dort nachzusehen. Er heißt **Import**.

`/mvlink debug` zeigt im Spiel jetzt jeden Slot, auch die leeren, mit dem Rohwert der
Abfrage. Vorher druckte es nur, was funktioniert hat — bei null erkannten Teilen also nichts,
und damit ausgerechnet dann nichts, wenn man es braucht. Und das Addon überschreibt einen
funktionierenden Code nicht mehr mit einem leeren: es speichert bei jedem Login, ein Login auf
einem nackten Zweitcharakter genügte bisher, um den letzten guten Stand zu verlieren.

## Aussehen

Die Titelleiste war der hellste Streifen im Fenster und stand damit vor dem Inhalt statt
dahinter. Jetzt ist sie der dunkelste, mit einer feinen Struktur statt einer glatten Fläche.

Der Schriftzug läuft in **MORPHEUS**, der Zierschrift von World of Warcraft — direkt aus
deiner Installation gelesen. Mitgeliefert wird sie nicht, sie gehört Blizzard; ist sie nicht
auffindbar, bleibt es bei der bisherigen Schrift.

Die Anzeige oben rechts zeigt `CASC · 12.1.0` statt `CASC · 12.1.0.69273`. Die fünfstellige
Build-Nummer sagte niemandem etwas.

## Kleinigkeiten

- Wird in einen schreibgeschützten Ordner installiert, sagt das Programm das jetzt beim Start
  klar, statt „Die Datendefinitionen konnten nicht geladen werden" zu melden — das eine, was
  dann nicht kaputt ist.
- Neue Schalter für Skripte: `--mvlink`, `--install-mvlink-addon`, `--shot-frame`.
- Die Beschreibung führte noch durch einen Wowhead-Import, den es seit einer Fassung nicht
  mehr gibt.

## Wohin installieren

Das Setup schlägt einen Ordner im Benutzerprofil vor, du kannst aber jeden beschreibbaren
Ordner angeben — `D:\Programme\ModelViewer Midnight` etwa. **`C:\Programme` funktioniert
nicht:** das Programm legt seinen Datenbank-Cache von rund 80 MB neben sich ab und braucht
dort Schreibrecht. Der Pfad steckt fest in `core.dll`; das zu ändern ist für eine spätere
Fassung vorgemerkt.
