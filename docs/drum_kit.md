# Lumen KIT 0.1 — profil 808 / proof of concept

Jeden zestaw inspirowany Roland TR-808 na jednej partii SYN. KFM został usunięty
z wyboru modeli i rejestru silników. Lista modeli: ACD, FAT, VAP, WTFM, PERC,
DIRT, CHIP, **KIT**. KIT ma 6 wewnętrznych bębnów; ustaw liczbę głosów partii na 1.

## Aktualizacja i start

1. Wgraj `/Firmware/TrackerMini_KIT_0.1.0.ptf`: **Config → Firmware → Firmware update**.
   Oczekiwany Build: **808p03**.
2. W nowym projekcie wybierz **SYN 1 → KIT → Edit Patch → Init**.
   Init ustawia nowy zestaw 808. Już zapisany patch zachowuje swoje parametry,
   więc po aktualizacji użyj Init, żeby usłyszeć nowe ustawienia początkowe.
3. W edytorze wybierz sekcję bębna, przejdź do listy parametrów i naciśnij
   środkowy przycisk odsłuchu. Każda sekcja wyzwala teraz własny instrument.
4. W patternie używaj tego samego SYN 1 na kilku ścieżkach, np. 9–14:

| Nuta / MIDI | Sekcja | Brzmienie |
| --- | --- | --- |
| C4 / 60 | C4 BD | Bass Drum — niski, sinusoidalny kick |
| D4 / 62 | D4 SD | Snare — krótki korpus i osobna warstwa szumu Snappy |
| E4 / 64 | E4 CH | Closed Hi-Hat — krótki metaliczny hi-hat |
| F4 / 65 | F4 CP | Hand Clap — trzy impulsy szumu i osobny ogon |
| G4 / 67 | G4 OH | Open Hi-Hat — dłuższy metaliczny ogon |
| A4 / 69 | A4 CB | Cowbell — dwa prostokątne oscylatory i dwustopniowe wybrzmiewanie |

Mapowanie powtarza się w każdej oktawie. C/C# = BD, D/D# = SD, E = CH,
F/F# = CP, G/G# = OH, A/A#/B = CB. Dzięki temu zmiana oktawy lub skali klawiatury
Mini nie powoduje ciszy. To własne mapowanie KIT, nie standard General MIDI.
Nuty wybierają bęben; Pitch reguluje wysokość niezależnie od oktawy klawiatury.

## Poprawiony odsłuch

W p02 przycisk odsłuchu wysyłał nutę ze stałego pada, bez uwzględnienia sekcji.
Dlatego Snare, Clap i Hat mogły w edytorze odtwarzać kick. W p03 sekcje wysyłają
bezpośrednio C4/D4/E4/F4/G4/A4. Zwolnienie przycisku pamięta nutę i partię
z chwili naciśnięcia, nawet po zmianie sekcji. Poza edytorem sekcji odsłuch KIT
domyślnie wyzwala Bass Drum.

Podgląd KIT poza REC omija przydzielanie wolnej ścieżki sekwencera i nie wpisuje
nut do patternu. Działa tą samą bezpośrednią ścieżką również przy odtwarzaniu
Song Mode. Wejście podczas REC korzysta z dotychczasowego mechanizmu nagrywania.
Podstawowe działanie po tej poprawce potwierdził KRUChY na fizycznym Mini.
Pełna stabilność i wszystkie tryby nadal wymagają testów.

## Charakterystyka i ustawienia startowe

To oryginalna synteza DSP inspirowana opisami generatorów TR-808. Nie jest kopią
oprogramowania Rolanda, emulacją obwodów ACB ani bankiem fabrycznych presetów.
Wartości poniżej dobrano dla naszego DSP; pozycje gałek analogowego TR-808 nie
przekładają się liczbowo na parametry tego silnika.

| Instrument | Pitch | Decay do -60 dB | Tone / Snappy / Metal / Balance | Drive | Level | Pan |
| --- | --- | --- | --- | --- | --- | --- |
| BD | 0 (~55 Hz) | 600 ms | Tone 50% | 0% | 90% | środek |
| SD | 0 | 320 ms | Snappy 72% | 0% | 75% | środek |
| CH | 0 | 100 ms | Metal 70% | 0% | 55% | środek |
| CP | 0 | 320 ms | Tone 50% | 0% | 70% | środek |
| OH | 0 | 800 ms | Metal 65% | 0% | 50% | środek |
| CB | 0 | 220 ms | Balance 50% | 0% | 55% | środek |

BD używa krótkiej obwiedni wysokości i korpusu sinusoidalnego, bez FM.
SD ma dwa rezonanse tonalne oraz niezależną obwiednię filtrowanego szumu.
CH/OH korzystają z sześciu prostokątnych oscylatorów i filtra pasmowego;
CH tłumi wybrzmiewający OH. CP ma trzy ataki co 10 ms i oddzielny ogon.
CB miesza oscylatory około 540/800 Hz; Balance zmienia ich proporcję.
Pitch CP przesuwa pasmo szumu. Wszystkie brzmienia mają własny Drive/Level/Pan.
Nowe uderzenie pobiera ustawienia; poprzedni ogon tego samego bębna jest
zastępowany. Inne bębny zachowują swój stan.

Źródła odniesienia:

- Roland: instrumenty i kontrolki TR-808 — https://support.roland.com/hc/en-us/articles/201963539-TR-808-Technical-Specifications
- Oryginalne Roland TR-808 Service Notes, opisy generatorów BD/SD/CP/CB/CH/OH — https://www.florian-anwander.de/roland_tr808/roland_TR-808_service-manual.pdf
- Elektron Syntakt, Appendix A, porównanie kategorii maszyn — https://www.elektron.se/wp-content/uploads/2026/03/Syntakt-User-Manual_ENG_OS1.40_260304.pdf

## Modulacja podczas grania

Efekty a…f w patternie są kierowane do bębna danej ścieżki:
**a Pitch, b Decay, c Tone/Snappy/Metal/Balance, d Drive, e Level, f Pan**.
63 jest neutralne; 0 i 127 to skrajne przesunięcia. Dla Level wartość 0 wycisza.
Dwa sloty są niezależne dla każdej ścieżki; FX1 ma pierwszeństwo przed FX2.
Pusty efekt podtrzymuje ustawienie, OFF czyści dany slot. Efekt bez nuty
odnosi się do poprzedniego bębna tej ścieżki. Zmiana bębna czyści jej blokady.

Kontrolki makr na ekranie SYN są wspólnymi przesunięciami KIT. Efekty a…f
pozostają przypisane do bębna ścieżki, nawet jeśli użytkownik przemapuje makra.
Efekt Panning ustawia absolutną panoramę uderzenia 0/50/100. Delay/Reverb Send
ustawiają niezależne wysyłki bębna; podnieś też send całej partii i efekt w mikserze.
Wewnętrzne wysyłki KIT domyślnie wynoszą zero.

Mute wygasza aktualny bęben ścieżki, Stop cały KIT; CUT/FADE używają krótkiego
release. Zwykłe MIDI note-off nie ucina one-shotów. Dwie ścieżki grające ten sam
bęben dzielą jego ogon; mute jednej z nich dotyczy wspólnego bębna.

## DIRT i CHIP — wynik kontroli

Na karcie są 299 plików `.patch` dla ACD/FAT/VAP/WTFM/PERC. Nie ma banków DIRT
ani CHIP; nie potwierdzono błędu ładowania. Wybór modelu tworzy Init niezależnie
od plików SD. CHIP Init ma działający model 2A03 Pulse oraz osiem algorytmów
wybieranych parametrem Model. Test komputerowy CHIP dał skończone, niezerowe audio.
DIRT pozostaje fabrycznym silnikiem z biblioteki PolySynth. Oficjalny bank
60 presetów Play+ nie został pobrany (HTTP 403), a zgodność między produktami
nie została potwierdzona. Pusta lista pokazuje teraz komunikat
`No presets for this model - use Init`.

## Zgodność i weryfikacja

ID KIT w patchach pozostaje 10. KFM nie jest już rejestrowany; dawne miejsce 9
pozostaje zarezerwowane. Indeks KIT w ustawieniach projektu zmienia się z 8 na 7
po usunięciu pozycji KFM i jest normalizowany podczas odczytu. Stare projekty
z KFM przechodzą na miejsce KIT, lecz stare brzmienie KFM nie jest zachowywane;
użyj nowego Init. Pliki poprzednich prototypów są zachowane w kopiach na komputerze.

```sh
make all NPROC=6 BUILD_TAG=808p03
bash tools/drum-kit/verify.sh
```

Testy: 6 algorytmów/1 głos SDK, równoczesne wybrzmiewanie, routing FX, izolacja
partii, stereo/sendy, velocity, choke/mute/Stop, 48 skrajnych/losowych zestawów,
wszystkie 128 nut klawiatury, odsłuch 6 sekcji i zapamiętywanie note-off,
migracja indeksu modelu oraz widmowe różnice domyślnych BD/SD/CH/CP/CB i różna
długość CH/OH. Włączone ASan/UBSan; LSan wyłączony z powodu ograniczenia ptrace.
Testy nie obejmują zamkniętego alokatora/serializera PolySynth ani fizycznego UI.
Na Mini trzeba potwierdzić odsłuch podczas Song Mode, zapis/odczyt i obciążenie CPU.
Aliasowanie jest możliwe; nie stosujemy pełnego modelowania obwodów ani oversamplingu.

## Autorzy

**KRUChY i Lumen** — [role i podziękowania](../AUTHORS.md). KIT 0.1 jest próbą
możliwości platformy, nie gotowym produktem.
