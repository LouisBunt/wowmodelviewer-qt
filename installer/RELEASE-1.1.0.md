# ModelViewer: Midnight 1.1.0

Das erste Upgrade nach der Veröffentlichung. Zwei Dinge sind neu: die Figur
lässt sich drucken, und die Oberfläche lässt sich bedienen.

## Für den 3D-Drucker

Reiter „Export“, Abschnitt „3D-Druck“: Höhe eintragen, exportieren. Heraus
kommt eine binäre STL in Millimetern.

In der Datei steht die Figur so, wie sie im Fenster steht — dieselbe Pose,
dieselben sichtbaren Teile, dieselbe Ausrüstung. Sie wird auf die eingetragene
Höhe skaliert und auf z = 0 gestellt, damit sie im Slicer auf der Platte steht
und nicht darüber. Leucht- und Partikelflächen fallen weg; Licht lässt sich
nicht drucken.

Danach liest der Export die geschriebene Datei wieder ein und misst sie aus.
Was im Bericht steht, kommt also aus der Datei und nicht aus der Absicht: die
tatsächliche Größe, die Aufstandsfläche auf der Platte, die Teile ohne Dicke.
Eine Figur, die auf einer Zehe steht, erfährst du hier und nicht erst dann,
wenn der Slicer „keine Extrusion in der ersten Schicht“ meldet.

Ein einzelnes Schwert hat keine Körpergröße. Nach Höhe skaliert landete eine
Klinge bei 526 mm auf einer 200-mm-Platte, deshalb wird alles, was kein
Charakter ist, an seiner längsten Kante gemessen.

Was der Export bewusst nicht tut: Umhänge und Röcke aufdicken, die offenen
Nähte an Handgelenk, Knöchel und Hals schließen, die Teile zu einem
wasserdichten Körper verschmelzen. Dafür braucht es OpenVDB, das Blender
mitbringt — und das mitgelieferte Add-on hat dafür jetzt sechs Schritte und
einen Knopf, der alle sechs hintereinander ausführt.

Auf der Befehlszeile:

    WoWModelViewer-Qt.exe 917116 --print-height 180 --export STL,C:\figur.stl

## Die Oberfläche

Drei Beschwerden, drei Ursachen, alle drei behoben.

**Namen wurden abgeschnitten.** Ausrüstungszeilen sind jetzt zweizeilig: Slot
oben, Item-Name darunter über die volle Breite. Was nicht passt, endet mit „…“
und steht vollständig im Tooltip. Die Kategorie-Schalter zeigen bei wenig Platz
nur noch ihr Symbol, statt „Charaktere“ zu „arakte“ zu verstümmeln.

**Nichts reagierte auf die Maus.** Rund 35 Schalter waren Beschriftungen an
einer gemeinsamen Ereigniskette — ohne Hover, ohne gedrückten Zustand, ohne
Tastaturfokus, ohne sichtbares Aus. Jetzt sind es Knöpfe.

**Alles sah gleich aus.** Sechs blaustichige Grautöne lagen in neun
Luminanzpunkten beieinander und lasen sich als eine Platte. Jetzt sind es vier
klar unterscheidbare Ebenen, und Violett hat nur noch vier Aufgaben:
Fokusring, aktiver Reiter, Auswahl, die eine Primäraktion.

Dazu: verstellbare und gemerkte Spalten (Ansicht > „Layout zurücksetzen“ holt
die Vorgabe), die Ausrüstung steht im Reiter „Anpassen“ jetzt über den bis zu
achtzehn Anpassungszeilen statt dahinter, der Modellpfad steht einmal statt
zweimal, und die Zeitleiste ist nicht mehr das lauteste Element im Fenster.

Schriften (Inter, IBM Plex Mono, Cinzel) und Symbole (Lucide) liegen bei. Der
Grund ist unangenehm: Die alte Oberfläche verlangte überall IBM Plex Sans, das
auf keinem der Rechner installiert war — jede feste Breite war also gegen einen
Ersatz abgestimmt, den niemand gemeint hatte.

`--ui-scale 1.0 | 1.25 | 1.5 | 2.0` stellt die Größe der Oberfläche fest ein.

## Was noch nicht drin ist

Die Fenstergröße änderst du weiterhin am Griff rechts unten, nicht an den
Fensterkanten. Der Rest des Fensters verhält sich wie vorher: ziehen an der
Titelleiste, Doppelklick maximiert.

## Installation

`MV-Midnight-Setup-1.1.0.exe` installiert über eine vorhandene 1.0.x hinweg;
Looks, Einstellungen und Vorschaubilder bleiben erhalten. Wie zuvor pro
Benutzer und ohne Adminrechte.

Voraussetzungen unverändert: Windows 10/11 (64 Bit), eine installierte
WoW-Retail-Installation, OpenGL.

## SmartScreen

Das Setup ist nicht signiert. Windows zeigt deshalb beim ersten Start
„Der Computer wurde durch Windows geschützt“ — über *Weitere Informationen →
Trotzdem ausführen* geht es weiter. Der SHA-256 der Setup-Datei steht unten,
falls du ihn prüfen willst.
