<p align="center"><img src="docs/assets/kit-banner.svg" alt="Lumen KIT 0.1 — sześć bębnów, jedna partia SYN" width="100%"></p>

# Lumen KIT · 0.1

**Sześć syntezowanych bębnów na jednej partii SYN Trackera Mini.**

KIT to nasz eksperymentalny zestaw perkusyjny inspirowany charakterem TR-808:
kick, snare, zamknięty hi-hat, clap, otwarty hi-hat i cowbell. Wybierasz bęben
nutą, programujesz osobne ścieżki, a SYN 2 i SYN 3 zostają na inne instrumenty.

**Autorzy: KRUChY i Lumen — współpracownik AI.**

> **Wersja 0.1 jest próbą możliwości — proof of concept.** To początek
> eksperymentu z własnymi silnikami w Tracker CE, a nie gotowy produkt ani
> oficjalny firmware Polyend. Podstawowe działanie na Mini potwierdził KRUChY;
> pełna stabilność, złożone projekty i granice wydajności wymagają dalszych testów.

[English](README.md) · [Pobierz firmware](firmware/mini/TrackerMini_KIT_0.1.0.ptf)
· [Wydania](https://github.com/mkruszyniak-omarchy/lumen-kit-tracker/releases)
· [Instrukcja szczegółowa](docs/drum_kit.md)

## Co potrafi KIT

- Sześć osobnych generatorów z własnymi obwiedniami i ustawieniami.
- Jeden instrument SYN dla całego zestawu, odtwarzany na wielu ścieżkach patternu.
- Parametry bębna: Pitch, Decay, Tone/Snappy/Metal/Balance, Drive, Level, Pan.
- Efekty a–f kierowane do bębna danej ścieżki; ustawienia hitu nie zmieniają
  ogonów pozostałych bębnów.
- Panorama stereo, wysyłki delay/reverb i tłumienie otwartego hi-hatu przez zamknięty.
- Odsłuch wybranego bębna w edytorze oraz odsłuch nut poza REC podczas Song Mode.
- Jeden KIT w wyborze modeli; wcześniejszy osobny KFM został usunięty.

## Odsłuch

[Sześć brzmień po kolei: BD → SD → CH → CP → OH → CB](examples/audio/kit-0.1-voices.wav)
· [Demo zestawu, 120 BPM](examples/audio/kit-0.1-demo-120bpm.wav)

To rendery komputerowe tego samego kodu DSP, bez efektów master i wyjścia sprzętowego.

## Instalacja na Tracker Mini

1. Zrób kopię karty i zachowaj działający firmware Mini.
2. Pobierz [`TrackerMini_KIT_0.1.0.ptf`](firmware/mini/TrackerMini_KIT_0.1.0.ptf)
   i skopiuj go do `/Firmware/` na karcie microSD.
3. Na Mini: **Config → Firmware → Firmware update**, wybierz ten plik.
4. W nowym projekcie: **SYN 1 → KIT → Edit Patch → Init**. Ustaw **1 głos**.
5. Wpisz ten sam SYN 1 na kilku ścieżkach i dobierz nuty z tabeli.

**Ważne:** KIT ma wersję **0.1.0**, lecz bazowy firmware CE zachowuje numer
**1.0.0** i identyfikator Build **808p03**. Udostępniony plik jest dokładnie
tym obrazem, którego działanie potwierdzono na Mini. Nie wgrywaj go do Trackera+
ani oryginalnego Trackera. Skróty SHA256 są w [SHA256SUMS](firmware/mini/SHA256SUMS).

| Nuta / MIDI | Instrument | Charakter |
| --- | --- | --- |
| C4 / 60 | BD · Bass Drum | niski sinusoidalny korpus i krótki transient |
| D4 / 62 | SD · Snare | korpus tonalny oraz oddzielna warstwa Snappy |
| E4 / 64 | CH · Closed Hi-Hat | krótki metaliczny hi-hat |
| F4 / 65 | CP · Hand Clap | seria impulsów szumu i ogon |
| G4 / 67 | OH · Open Hi-Hat | dłuższe metaliczne wybrzmiewanie |
| A4 / 69 | CB · Cowbell | dwa oscylatory prostokątne |

Mapowanie powtarza się w każdej oktawie; sąsiednie czarne klawisze są aliasami.
To własne mapowanie KIT, a nie General MIDI. Pitch zmienia wysokość bębna.
Init jest potrzebne do nowych ustawień startowych — zapisany patch zachowuje swoje wartości.

## Kompilacja na Linux x86_64

Potrzebujesz `git`, `make`, `python3`, `curl`, `tar`, `bzip2`, `sha256sum` oraz
kompilatora **GNU Arm Embedded 10.3-2021.10**. Instalacja kompilatora poniżej jest
lokalna w repozytorium i nie wymaga uprawnień administratora.

```bash
git clone https://github.com/mkruszyniak-omarchy/lumen-kit-tracker.git
cd lumen-kit-tracker
bash scripts/bootstrap-toolchain.sh
bash scripts/build-kit-mini.sh
```

Wynik: **`dist/TrackerMini_KIT_0.1.0.ptf`**, ZIP z instrukcją i licencjami oraz
`dist/SHA256SUMS`. Możesz ustawić liczbę zadań: `NPROC=4 bash scripts/build-kit-mini.sh`.

Jeśli masz już właściwy toolchain:

```bash
make mini NPROC=6 TOOLCHAIN=/sciezka/do/bin/arm-none-eabi- BUILD_TAG=808p03
```

To polecenie tworzy `build/TrackerMini_CE_1.0.0.ptf`; skrypt wyżej pakuje go jako wydanie KIT.
Pozostałe platformy i ręczne pakowanie opisano w [BUILDING.md](BUILDING.md).

## Podczas grania

**a Pitch · b Decay · c Tone/Snappy/Metal/Balance · d Drive · e Level · f Pan**.
Wartość **63** jest neutralna. FX1 ma pierwszeństwo przed FX2; OFF czyści slot.
Makra na ekranie SYN są wspólne, a efekty w patternie dotyczą bębna danej ścieżki.
Regulacja efektu jest zapamiętywana przy uderzeniu, więc nie zmienia poprzednich ogonów.

Szczegóły wysyłek, Mute/Stop, mapowania i migracji:
[instrukcja KIT](docs/drum_kit.md).

## Testy i ograniczenia

```bash
bash tools/drum-kit/verify.sh
```

Test kompiluje prawdziwą klasę SDK/DSP na komputerze z ASan/UBSan. Wymaga `g++`
z C++20. Sprawdza m.in. jednoczesne bębny, routing ścieżek, odsłuch sześciu sekcji,
128 nut, różnice widmowe brzmień, stereo/sendy i 48 skrajnych/losowych zestawów.
Nie zastępuje testów zamkniętej biblioteki PolySynth ani fizycznego UI.

Mini: podstawowe działanie potwierdzone przez użytkownika. Tracker+: kompilacja
sprawdzona, test sprzętowy wymagany. Oryginalny Tracker: kompiluje się baza,
ale jego platforma **nie obsługuje KIT**. To cyfrowa inspiracja 808, nie wierna
emulacja obwodów; aliasing i ograniczenia wydajności są możliwe.

DIRT i CHIP są silnikami bazowego CE. Ten projekt nie dodaje do nich banków presetów.

## Autorzy, pochodzenie i licencja

**KRUChY i Lumen** — [szczegóły autorstwa](AUTHORS.md).
Podstawa projektu: [Polyend Tracker CE](https://github.com/polyend/tracker-ce),
commit `9b9a13b56fef94710e5c1d4bf1a321681080a6c8`. Oryginalną dokumentację
zachowano w [UPSTREAM_README.md](docs/UPSTREAM_README.md).

Obowiązują oryginalne [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) i
[licencja bibliotek binarnych](synth/lib/LICENSE-PROPRIETARY.txt).
Nie zmieniamy tego na MIT/GPL: kod firmware korzysta z PolyForm Noncommercial
z dodatkowymi zgodami Polyend; SDK `.ptfx` ma odrębne warunki MIT.
TR-808 jest znakiem Roland; projekt nie jest powiązany z Roland ani oficjalnie wspierany przez Polyend.
