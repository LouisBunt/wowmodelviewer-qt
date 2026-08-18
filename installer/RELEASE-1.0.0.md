# ModelViewer: Midnight 1.0.0

Die erste öffentliche Fassung. Intern liefen vorher die Versionen 1.5 bis 1.8 —
öffentlich beginnt die Zählung dort, wo die Öffentlichkeit beginnt.

## Was es ist

Ein neues Frontend für WoW Model Viewer: die bewährte Engine (core.dll, wow.dll)
bleibt, die Oberfläche ist neu — dunkel, aufgeräumt, auf Qt statt wxWidgets.
Es liest deine eigene World-of-Warcraft-Installation; Spieldaten werden nicht
mitgeliefert.

## Die Wege zu deinem Charakter

- **MVLink**: Das beiliegende WoW-Addon liest deinen angezogenen Charakter im
  Spiel aus; ein kopierter Code in der Zwischenablage genügt, ModelViewer stellt
  ihn nach — Ausrüstung, Anpassungen, Farbvarianten.
- **Wowhead-Ankleideraum-Link** einfügen, fertig. Gespeicherte Outfits und
  Transmog-Set-Seiten funktionieren ebenfalls.
- **Armory-Import** über Realm und Charaktername.
- **Meine Looks**: eine Bibliothek gespeicherter Looks mit Vorschaubildern.

## Ansehen, exportieren, weiterbauen

- Item-Ansicht: ein einzelnes getragenes Teil isoliert drehen und exportieren.
- Export nach **Blender** als FBX mit Material-Sidecar; das mitgelieferte
  Blender-Add-on baut die Materialien automatisch nach — Alpha-Modi, Glühen,
  Effektebenen. Es installiert sich aus dem Programm heraus.
- Item- und Transmog-Browser mit Kategorien und Suche, NPC-Suche nach Name,
  Licht- und Animationssteuerung.

## Installation

`MV-Midnight-Setup-1.0.0.exe` installiert pro Benutzer, ohne Adminrechte, im
eigenen dunklen Setup. Deinstallation über die Windows-Einstellungen entfernt
alles restlos.

Voraussetzungen: Windows 10/11 (64 Bit), eine installierte
WoW-Retail-Installation, OpenGL; rund 350 MB Platz plus Datenbank-Cache.
Details und bekannte Lücken: LIESMICH.txt im Installationsordner.
