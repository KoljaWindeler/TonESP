#!/bin/bash
# replace google api key with your key
GOOGLE_API_KEY="ADDYOURKEYHERE"


s="$(cat << EOF
0300|Alles klar, wähle nun mit den Lautstärke-Tasten den Ordner aus.
0310|Diese Karte kenne ich noch nicht! Wähle nun mit den Lautstärke-Tasten den Wiedergabemodus aus. Drücke die Pause-Taste um die Auswahl zu speichern.
0311|Hörspiel-Modus: Eine zufällige Datei aus dem Ordner wiedergeben.
0312|Album-Modus: Den kompletten Ordner wiedergeben.
0313|Party-Modus: Ordner zufällig wiedergeben.
0314|Einzel-Modus: Eine bestimmte Datei im Ordner wiedergeben.
0315|Hörbuch-Modus: Einen Ordner wiedergeben und den Fortschritt speichern.
0320|OK, wähle nun bitte die Datei mit den Lautstärke-Tasten aus.
0316|Administrator-Funktionen.
0317|Spezialmodus Von-Bis, Hörspiel: Eine zufällige Datei zwischen der Start und Enddatei wiedergeben.
0318|Spezialmodus Von-Bis, Album: Alle Dateien zwischen der Start und Enddatei wiedergeben.
0800|Bitte lege die Karte, die du löschen möchtest, auf! Zum Abbrechen einfach eine der Lautstärke-Tasten drücken.
0801|OK, du kannst die Karte nun wieder neu konfigurieren.
0802|OK, ich habe den Vorgang abgebrochen.
0905|EQ konfigurieren.
0920|Bitte wähle eine Einstellung für den EQ mit den Lautstärketasten aus und bestätige sie mit der Pausetaste.
0921|Normal
0922|Pop
0923|Rock
0924|Jazz
0925|Classic
0926|Bass
0961|5 Minuten Timer.
0962|15 Minuten Timer.
0963|30 Minuten Timer.
0964|60 Minuten Timer.
0965|nicht automatisch abschalten
0930|Maximale Lautstärke wählen und mit der Pausetaste bestätigen.
0931|Minimale Lautstärke wählen und mit der Pausetaste bestätigen.
0932|Lautstärke beim Start wählen und mit der Pausetaste bestätigen.
0999|Reset wurde durchgeführt!
0330|Soll ich vor einer Datei jeweils die Nummer ansagen? Du kannst jederzeit durch einen langen Druck auf die Pause-Taste die aktuelle Nummer abfragen.
0331|Nein, Nummer nicht ansagen.
0332|Ja, Nummer ansagen.
0321|OK, wähle nun bitte die Startdatei mit den Lautstärketasten aus.
0322|Wähle nun bitte die Enddatei mit den Lautstärketasten aus.
0400|OK. Ich habe die Karte konfiguriert.
0401|Schade! Das hat leider nicht geklappt!
0500|Hallo Carlo. lege eine Karte auf zum Beginnen.
0502|Hallo Carlo. 
0503|Hey Carlo! 
0504|Na Carlo? 
0501|Hallo Carlo. schoen das du wieder da bist.
0969|ich bin online.
0970|ich bin offline.
EOF)"
IFS=$'\n'
for a in `echo "$s"`; do
  j=`echo $a | cut -f1 -d'|'`
  i=`echo $a | cut -f2 -d'|'`
  curl --header "Content-Type: application/json; charset=utf-8" --data "{
    'audioConfig': {'audioEncoding': 'MP3','speakingRate': 1.00,'sampleRateHertz': 44100,'pitch': 0},
    'voice': {'languageCode': 'de-DE','name': 'de-DE-Wavenet-C'},
    'input': {'text': '${i}'}
  }" "https://texttospeech.googleapis.com/v1/text:synthesize?fields=audioContent&key=${GOOGLE_API_KEY}" | php -r "print(json_decode(file_get_contents('php://stdin'),true)['audioContent']);" | base64 --decode > SD-Karte/mp3/${j}.mp3
done

exit


for i in {1..1}
do
  j=$(printf %04d $i)
  echo $j
  curl --header "Content-Type: application/json; charset=utf-8" --data "{
    'audioConfig': {'audioEncoding': 'MP3','speakingRate': 1.00,'sampleRateHertz': 44100,'pitch': 0},
    'voice': {'languageCode': 'de-DE','name': 'de-DE-Wavenet-C'},
    'input': {'text': '${i}'}
  }" "https://texttospeech.googleapis.com/v1/text:synthesize?fields=audioContent&key=${GOOGLE_API_KEY}" | php -r "print(json_decode(file_get_contents('php://stdin'),true)['audioContent']);" | base64 --decode > SD-Karte/mp3/${j}.mp3
done




