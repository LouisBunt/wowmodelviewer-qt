Erste Fassung, die bewusst für andere Rechner als den Entwicklungsrechner gedacht ist.
Ein Audit über den gesamten Baum hat 43 bestätigte Befunde geliefert; die 15 Blocker
darunter sind hier abgearbeitet.

## Neu: Item-Ansicht

Jede belegte Zeile der Ausrüstungsliste hat rechts zwei Schalter: „×" legt das Teil ab,
das Auge zeigt **nur dieses Teil** und blendet die Figur samt allem anderen aus. Die
Kamera stellt sich selbst darauf ein, drehen und zoomen wie gewohnt, erneutes Klicken
holt den Charakter zurück. Ein Export in dieser Ansicht enthält nur das gezeigte Teil.

Das Auge erscheint nur, wo es etwas zu zeigen gibt — Brust, Beine, Hände und Gürtel sind
in WoW auf die Körpertextur gemalt und haben kein eigenes Modell.

## Export enthält nur noch, was sichtbar ist

Der FBX-Export ließ versteckte Flächen weg, schrieb aber trotzdem jeden Vertex des
Modells in die Datei. Ein einzeln exportierter Helm schleppte damit rund 317.000 fremde
Punkte mit — das blähte die Datei auf und verdarb Bounding-Box und Objektursprung in
Blender.

| | vorher | jetzt |
|---|---|---|
| Helm einzeln | 319.688 Verts, 9,6 MB | 1.929 Verts, 1,19 MB |
| ganzer Charakter | 319.688 Verts | 20.065 Verts |

Inhalt und Skinning bleiben gleich: null ungewichtete Vertices, Gewichtssumme exakt 1,0.

## Startfehler sagen jetzt, was los ist

Vier Fehlerwege schrieben ihre Meldung bisher in ein Textfeld, das nie angezeigt wurde.
Bei falschem WoW-Ordner sah man drei Sekunden ein leeres Fenster und dann nichts mehr —
ausgerechnet der wahrscheinlichste Fehler beim ersten Start. Betroffen waren außerdem:

- **Kein OpenGL** (Remotedesktop, VM, alter Treiber): Das Programm verschwand nach zwei
  Sekunden wortlos. Jetzt mit Erklärung und den üblichen Ursachen.
- **Keine Datendefinitionen für die Client-Version**: Das Programm startete, konnte aber
  nichts anlegen, texturieren oder importieren. Jetzt wird abgebrochen statt so zu tun.
- **Kein Protokoll**: Es wurde überhaupt keines geschrieben, während Fehlerdialoge auf
  „siehe Log" verwiesen. `userSettings\log.txt` gibt es jetzt — rund 126 KB pro Lauf.

Skriptgesteuerte Läufe (`--shot`, `--export`) erkennen sich selbst und brechen mit
Exit-Code 1 ab, statt auf einem Dialog stehenzubleiben.

## Weitere behobene Fehler

- **Absturz beim zweiten Modell.** Beim Modellwechsel wurden zwei Zeiger-Listen der
  Ausrüstungszeilen nie geleert; ab dem zweiten Charakter schrieb jede Aktualisierung
  durch Zeiger auf bereits gelöschte Zeilen.
- **Rückweg aus der Item-Ansicht.** Das Ausschalten stellte die versteckten Teile nie
  wieder her — der Charakter blieb zerstückelt. Und wer das fokussierte Teil ablegte,
  landete in einem leeren Bild ohne Bedienelement, das den Zustand hätte aufheben können.
- **Datenpfade** hingen am Arbeitsverzeichnis. Der dokumentierte Aufruf aus einer Shell
  fand dadurch weder Dateiliste noch Datenbank noch Exporter.
- **Nur die 12.0-Datendefinitionen** waren im Paket, obwohl 9.2, 10.0 und 10.1 vorliegen.
  Jetzt sind alle vier dabei, und ältere Clients bekommen die nächstpassende Fassung.
- **Kamerarahmung** maß versteckte Effektgeometrie mit; ein Zweihänder wurde als winziger
  Fleck gerahmt.

## Lizenzhinweise und Doku

`THIRD-PARTY-NOTICES.txt` liegt jetzt im Paket und nennt, was mitgeliefert wird: Autodesks
FBX-Pflichthinweis, LGPL-Hinweis und Austauschbarkeit für Qt, OpenSSL, StormLib/CascLib,
GLEW, die Herkunft von `listfile.csv` und den TACT-Schlüsseln, dazu das GPL-Quellenangebot
für dieses Repository **und** das Untermodul.

Die beiliegende `LIESMICH.txt` war zwei Ausgaben alt und behauptete, Anprobe-Links ließen
sich „nicht importieren" — die Hauptfunktion von 1.6. Sie beschreibt jetzt den echten
Funktionsumfang, die Systemvoraussetzungen, wohin Fehlerberichte gehen und was das
Programm über das Netz sendet. Die Anwendung nennt ihre Version außerdem endlich selbst:
in der Titelleiste, im Über-Dialog und in der ersten Protokollzeile.

## Geprüft

Aus dem entpackten Paket heraus, nicht aus dem Entwicklungsbaum: Anprobe-Import,
Item-Ansicht an und aus, Ablegen des fokussierten Teils, ungültiger `--focus`-Wert,
fokussierter FBX-Export, 56/56 Fälle des Decoder-Korpus. Der FBX-Export ist in Blender 5.1
gegengeprüft — Geometrie, Materialien und Skinning korrekt.

## Installation

Das Setup installiert pro Benutzer ohne Adminrechte und aktualisiert eine vorhandene
Installation an Ort und Stelle. Wer nichts installieren will, nimmt das ZIP und startet
`WoWModelViewer-Qt.exe` aus dem entpackten Ordner.

**Voraussetzungen:** Windows 10/11 64 Bit, eine WoW-Installation, OpenGL (über
Remotedesktop läuft die 3D-Ansicht in der Regel nicht), rund 350 MB Platz.

Early Access heißt: Das Beschriebene ist geprüft und tut, was hier steht. Es fehlen aber
Bereiche der alten wxWidgets-Oberfläche, und es sind Fehler zu erwarten, die nur auf
fremder Hardware auftreten. Was fehlt, steht in der `LIESMICH.txt`.
