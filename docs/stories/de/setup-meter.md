---
id: setup-meter
title: Das Zählerprofil bestätigen und den Schlüssel eingeben
chapter: setup
screen: wizard-meter
order: 30
mock: 2026-10-13
hardware: gPlugK 2026-09-14
---
## Story
Als Besitzer eines gPlug möchte ich das passende Zählerprofil wählen und den Schlüssel meines
Netzbetreibers hinterlegen, damit der gPlug die Werte meines Zählers lesen kann.

## Steps
1. Im Schritt {{S.meter}} hört der gPlug zuerst mit, was Ihr Zähler sendet. Die Karte
   {{S.detectTitle}} zeigt nach wenigen Sekunden {{S.detectFound}} und das erkannte Protokoll.
2. Passt genau ein Profil, ist es bereits ausgewählt. Passen mehrere, wählen Sie das Profil Ihres
   Netzbetreibers. Mit {{S.otherProfile}} sehen Sie weitere Profile.
3. Sendet der Zähler verschlüsselt, geben Sie unter {{S.key}} den Schlüssel (GUEK) ein: 32 Zeichen
   aus 0–9 und A–F, wie im Brief Ihres Netzbetreibers. Sendet er unverschlüsselt, fragt der
   Assistent nicht nach einem Schlüssel.
   {{img:wizard-meter|Erkannter Zähler, vorgeschlagenes Profil und Schlüsselfeld}}
4. Unter {{S.values}} sehen Sie, welche Werte das Profil liest.
5. Tippen Sie auf {{S.next}}.

## Acceptance
- {{S.next}} ist erst aktiv, wenn ein Profil gewählt und bei einem verschlüsselten Zähler der
  Schlüssel gültig ist.
- Der Assistent wechselt zu {{S.done}} und prüft dort die Verbindung zum Zähler.

## Notes
- Der Schlüssel wird nur auf dem gPlug gespeichert und nie wieder angezeigt.
- Kann der gPlug noch nicht erkennen, ob der Zähler verschlüsselt, erscheint das Schlüsselfeld,
  darf aber leer bleiben.
- Erscheint nach einer halben Minute noch kein Signal, prüfen Sie das Kabel. Manche
  Netzbetreiber müssen die Kundenschnittstelle erst freischalten.
