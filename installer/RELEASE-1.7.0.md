Erste öffentliche Fassung unter dem neuen Namen. Was vorher als `better Model Viewer`
lief, heißt jetzt **ModelViewer: Midnight**.

## Charakter aus der Wowhead-Anprobe

Anprobe-Links (die mit dem `#`) werden importiert — mit Ausrüstung, Anpassungen und den
richtigen Farbvarianten. Der Look steckt komplett in der Adresse und wird örtlich
ausgewertet; dafür geht nichts ins Netz. Gegen einen Korpus von 56 echten Links geprüft.

## Einzelnes Teil ansehen und exportieren

Oben in der Werkzeugzeile schaltet **„Charakter | Nur Teil"** zwischen ganzer Figur und
einem einzelnen Stück um. Trägst du mehrere Teile mit eigenem Modell, fragt ein Menü,
welches gemeint ist. Die Kamera stellt sich selbst darauf ein, und ein Export in dieser
Ansicht enthält nur das gezeigte Teil.

Das Auge in der Ausrüstungsliste tut dasselbe für eine bestimmte Zeile.

## Nach Blender

FBX mit Netz, Skelett und Gewichtung, dazu eine Datei `.wmvmat.json`, aus der das
mitgelieferte Blender-Add-on die Materialien originalgetreu aufbaut — Durchsichtigkeit,
Leuchten und scrollende Effektflächen inbegriffen. Das Add-on installiert sich aus dem
Programm heraus.

Der Export schreibt nur noch, was sichtbar ist. Ein einzeln exportierter Helm zog vorher
rund 317.000 fremde Punkte mit; ein ganzer Charakter fällt von 319.688 auf 20.065 Punkte.

## Suchen und finden

Der Modellbrowser filtert nach Slot, Erweiterung, Qualität, **Rüstungsklasse** (Platte,
Kette, Leder, Stoff) und Name. Item-Sets lassen sich zusätzlich zur getragenen Ausrüstung
anlegen.

## Neues Aussehen

Violettes Farbschema statt Gold, Name und Version in der Titelleiste, Menü als Kacheln,
deutlichere Fensterknöpfe. Beim Start wird nichts mehr geladen — die Ansicht bleibt leer,
bis du ein Modell wählst.

## Wenn etwas nicht geht

Das Programm schreibt jetzt ein Protokoll (`userSettings\log.txt`). Startfehler — falscher
Ordner, fehlendes OpenGL, fehlende Datendefinitionen — sagen, was los ist, statt wortlos
zu verschwinden.

Fehler bitte melden: <https://github.com/LouisBunt/wowmodelviewer-qt/issues> — mit
`userSettings\log.txt` und `userSettings\qt-frontend-trace.txt`.

## Voraussetzungen

Windows 10/11 64 Bit, eine WoW-Installation (Datendefinitionen für 9.2, 10.0, 10.1 und
12.x liegen bei), OpenGL, rund 350 MB Platz. WoW darf dabei laufen.

## Installation

`MV-Midnight-Setup-1.7.0.exe` installiert pro Benutzer, ohne Adminrechte. Wer nichts
installieren will, nimmt das ZIP und startet `WoWModelViewer-Qt.exe` aus dem entpackten
Ordner.

**Hinweis für Umsteiger:** Diese Fassung hat eine eigene Kennung und installiert in einen
eigenen Ordner. Eine ältere Installation bleibt daneben stehen und kann normal
deinstalliert werden.

## Kontakt

Discord `peppawutz69` · Battle.net `peppawutz131#2465` · [Reddit](https://www.reddit.com/user/PeppaWutZ21/)
· [Patreon](https://www.patreon.com/c/LouisBunt) · Musik: [ルイス・ブント](https://on.soundcloud.com/cF8PsOmBw2fhkbSlbq)

Auch im Programm unter *Hilfe → Kontakt & Unterstützen*.

## Lizenz

GPLv3. Das Paket enthält Bibliotheken und Daten Dritter — Autodesks FBX SDK, Qt, OpenSSL,
die Dateiliste und die TACT-Schlüssel; Herkunft und Bedingungen stehen in
`THIRD-PARTY-NOTICES.txt` im Paket.

World of Warcraft gehört Blizzard Entertainment. Dieses Programm enthält keine Spieldaten,
es liest die Installation auf dem eigenen Rechner. Das Projekt steht in keiner Verbindung
zu Blizzard.
