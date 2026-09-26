# ModelViewer: Midnight 1.5.0

ModelViewer braucht keine WoW-Installation mehr. Die Spieldaten können jetzt
direkt von Blizzards öffentlichen Download-Servern kommen — so wie im CDN-Modus
von wow.export.

Die Nummer springt von 1.1 auf 1.5. Mit den frühen Testfassungen „1.5-beta“ und
„1.6-beta“ aus dem Sommer hat sie nichts zu tun: 1.5.0 ist neuer als beide.

## Ohne WoW-Installation

Findet ModelViewer beim ersten Start kein WoW am üblichen Ort, fragt es, woher
die Spieldaten kommen sollen:

- **WoW-Ordner wählen** — die eigene Installation, wie bisher.
- **Online laden** — die Dateien kommen von Blizzards Servern in einen Cache auf
  deinem Rechner.

Wer WoW am üblichen Ort installiert hat, sieht die Frage nicht; bei ihm ändert
sich nichts. Wechseln lässt sich die Quelle jederzeit unter Datei ›
„Spieldaten-Quelle …“. Der Wechsel gilt nach einem Neustart, den der Dialog
gleich anbietet.

Online wählst du dort auch die Region (Europa, Amerika, Korea, Taiwan) und die
Sprache der Spieldaten, also der Namen von Items, NPCs und Anpassungen.
Vorgabe ist die Sprache von Windows; die Oberfläche bleibt deutsch.

**Der erste Start lädt einmalig rund 400 MB**: die Verzeichnisse und Tabellen,
ohne die sich keine einzige Datei finden lässt. Auf einer schnellen Leitung
dauert das unter einer Minute, sonst einige Minuten. Das Ladebild zeigt
Fortschritt, verstrichene Zeit und erwartete Größe und hat „Abbrechen“ — beim
nächsten Start geht es dort weiter. Danach wird nur noch geladen, was du dir neu
ansiehst; ein Charakter mit Texturen sind etwa 20 MB. Mit jeder neuen
WoW-Version kommen die Tabellen einmal neu, rund 280 MB.

**Ohne Internet** startet das Programm mit dem zuletzt vollständig geladenen
Stand. Die Titelleiste zeigt dann „CDN · Version · offline“ mit einem
bernsteinfarbenen Punkt, und es geht alles, was schon einmal geladen wurde. Ein
Modell, das noch nicht im Cache liegt, sagt das.

Der Cache liegt im Ordner `cdn-cache` neben dem Programm. Im Dialog lässt sich
ein anderer Ort wählen; dort stehen auch die Größe des Caches und „Leeren“. Die
Deinstallation entfernt `cdn-cache` neben dem Programm.

Die Dateien bleiben Blizzards Eigentum: ModelViewer lädt sie bei Bedarf in den
Cache auf deinem Rechner und gibt sie nicht weiter. Welche Server das Programm
dabei anspricht, steht in der LIESMICH unter „Datenuebertragung“.

Was online nicht geht: das MVLink-Addon in WoW installieren und seine Ablage
lesen — beides braucht eine WoW-Installation. Codes aus dem Addon einfügen geht.

Auf der Befehlszeile, nur für diesen einen Lauf und ohne etwas zu speichern:

    WoWModelViewer-Qt.exe 917116 --online eu --locale deDE

## Behoben

- Der Statuspunkt in der Titelleiste wurde nie gezeichnet.
- Eine FileDataID allein auf der Befehlszeile wurde für einen WoW-Ordner
  gehalten. Das Druck-Beispiel aus der 1.1.0 scheiterte genau daran.
- Ein Skriptlauf ohne Ordner blieb an einem Dialog hängen. Jetzt endet er mit
  Fehlercode 1 und schreibt den Grund nach `userSettings\qt-frontend-trace.txt`.

## Was noch nicht drin ist

- Online nur Retail; PTR und Classic lassen sich nicht online laden.
- Einzelne Spieldateien kommen über einfaches HTTP auf Port 80, ohne Proxy. Netze,
  die Port 80 sperren oder nur über einen Proxy ins Internet lassen, bleiben außen
  vor, auch wenn die Startdateien noch ankommen.
- Der Cache wird nie von selbst kleiner. „Leeren“ unter Datei ›
  „Spieldaten-Quelle …“ räumt auf.

## Installation

`MV-Midnight-Setup-1.5.0.exe` installiert über eine vorhandene 1.1.x oder 1.0.x
hinweg; Looks, Einstellungen und Vorschaubilder bleiben erhalten. Wie zuvor pro
Benutzer und ohne Adminrechte.

Voraussetzungen: Windows 10/11 (64 Bit), OpenGL und entweder eine
WoW-Retail-Installation oder eine Internetverbindung für den Online-Modus.

## SmartScreen

Das Setup ist nicht signiert. Windows zeigt deshalb beim ersten Start
„Der Computer wurde durch Windows geschützt“ — über *Weitere Informationen →
Trotzdem ausführen* geht es weiter. Der SHA-256 der Setup-Datei steht unten,
falls du ihn prüfen willst.
