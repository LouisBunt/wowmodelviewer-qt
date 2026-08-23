# ModelViewer: Midnight 1.0.1

Ein Fehlerbehebungs-Release. Nichts Neues, eine Sache repariert — die aber
betraf jeden, der einen NPC über die NPC-Liste geöffnet hat.

## Behoben

- **NPC aus der Liste öffnen ließ „Kein Modell geladen" stehen.** Das Modell
  wurde geladen und gezeichnet, aber der Hinweis, der eine leere Ansicht
  erklärt, blieb als Kasten mitten darüber liegen — bei Chromie und anderen
  kleinen Modellen genau vor der Figur. Gleichzeitig blieben „Exportieren" und
  die vom Modell abhängigen Menüeinträge ausgegraut, obwohl etwas da war.

  Ursache: Die NPC-Liste lud am Signal vorbei, mit dem jeder andere Weg
  ankündigt, dass ein Modell angekommen ist. Sie geht jetzt denselben Weg wie
  Dateibaum, Item-Liste und alle Importe.

- **Ein NPC, dessen Modelldatei nicht in der Installation liegt, tat beim
  Doppelklick gar nichts.** Die Liste kommt aus der Datenbank, die Dateien aus
  der Installation, und die beiden können auseinandergehen — meist bei NPCs aus
  einem neueren Patch. Statt Schweigen steht der Grund jetzt in der Statuszeile.

## Für Skripte

`--npc` nimmt jetzt auch einen Namen statt einer Creature-ID:

    WoWModelViewer-Qt.exe --npc Chromie

Creature-IDs von Datenbankseiten passen oft nicht zu dem, was der Client
mitliefert; der Name ist das, wonach die Liste sucht und was in einem
Fehlerbericht steht.

## Installation

`MV-Midnight-Setup-1.0.1.exe` installiert über eine vorhandene 1.0.0 hinweg;
Looks, Einstellungen und Vorschaubilder bleiben erhalten. Wie zuvor pro
Benutzer und ohne Adminrechte.

Voraussetzungen unverändert: Windows 10/11 (64 Bit), eine installierte
WoW-Retail-Installation, OpenGL.

## SmartScreen

Das Setup ist nicht signiert. Windows zeigt deshalb beim ersten Start
„Der Computer wurde durch Windows geschützt" — über *Weitere Informationen →
Trotzdem ausführen* geht es weiter. Der SHA-256 der Setup-Datei steht unten,
falls du ihn prüfen willst.
