# TibiaBot – Product TODO

**Architektura:**
- Launcher: Python / PySide6
- Engine: C++
- Scripting: Lua
- Communication: IPC
- Configuration: JSON

---

# 1. Launcher

## Dashboard
- [x] Główny dashboard aplikacji
- [x] Wyświetlanie statusu silnika
- [x] Wyświetlanie statusu połączenia z klientem gry
- [x] Wyświetlanie aktywnych modułów
- [x] Wyświetlanie podstawowych statystyk działania
- [x] Wyświetlanie ostatnich zdarzeń
- [x] Wyświetlanie logów silnika

## Engine Management
- [x] Uruchamianie Engine
- [x] Zatrzymywanie Engine
- [x] Restartowanie Engine
- [x] Monitorowanie stanu Engine
- [x] Wykrywanie awarii Engine
- [x] Automatyczne wykrywanie utraty komunikacji
- [x] Obsługa wielu instancji Engine

## Configuration
- [x] Zarządzanie globalną konfiguracją
- [x] Zarządzanie konfiguracją poszczególnych modułów
- [x] Zapisywanie konfiguracji
- [x] Import konfiguracji
- [x] Eksport konfiguracji
- [x] Tworzenie profili użytkownika
- [x] Przełączanie profili
- [x] Przywracanie domyślnych ustawień

## Account & Updates
- [ ] Logowanie użytkownika
- [ ] Wylogowywanie użytkownika
- [ ] Zarządzanie kontem
- [ ] Weryfikacja licencji
- [x] Informacja o wersji aplikacji
- [ ] Automatyczne sprawdzanie aktualizacji
- [ ] Aktualizacja Launchera
- [ ] Aktualizacja Engine
- [ ] Informowanie o zmianach w aktualizacjach

---

# 2. Bot Engine – Core

## Engine Lifecycle
- [x] Inicjalizacja silnika
- [x] Zatrzymywanie silnika
- [x] Restart silnika
- [x] Zarządzanie stanem silnika
- [x] Obsługa błędów krytycznych
- [x] Obsługa wyjątków modułów
- [x] Monitorowanie aktywności silnika

## Module Manager
- [x] Rejestrowanie modułów
- [x] Uruchamianie modułów
- [x] Zatrzymywanie modułów
- [x] Włączanie i wyłączanie modułów
- [x] Pobieranie statusu modułów
- [x] Zarządzanie zależnościami modułów
- [x] Izolowanie błędów modułów
- [x] Przekazywanie konfiguracji do modułów

## Event Manager
- [x] System publikowania zdarzeń
- [x] System subskrypcji zdarzeń
- [x] Obsługa zdarzeń wewnętrznych
- [x] Obsługa zdarzeń pochodzących z Game Adapter
- [x] Obsługa zdarzeń modułów
- [x] Zarządzanie kolejnością zdarzeń

## Configuration Manager
- [x] Odczytywanie konfiguracji
- [x] Zapisywanie konfiguracji
- [x] Aktualizowanie konfiguracji w czasie działania
- [x] Walidacja konfiguracji
- [x] Obsługa konfiguracji per moduł
- [x] Obsługa wielu profili

---

# 3. Game Integration

## Game Adapter
- [x] Integracja z autoryzowanym interfejsem klienta gry
- [x] Wykrywanie dostępnej sesji gry
- [x] Odczytywanie stanu gry
- [x] Odczytywanie informacji o postaci
- [x] Odczytywanie pozycji postaci
- [x] Odczytywanie informacji o otoczeniu
- [x] Odczytywanie informacji o potworach
- [x] Odczytywanie informacji o przedmiotach
- [x] Odczytywanie informacji o ekwipunku
- [x] Obsługa zdarzeń zmiany stanu gry
- [x] Obsługa utraty połączenia
- [x] Synchronizacja Game State z Engine

## Game State
- [x] Aktualny stan postaci
- [x] Aktualna pozycja postaci
- [x] Aktualne HP
- [x] Aktualne MP
- [x] Aktualny poziom postaci
- [x] Aktualny ekwipunek
- [x] Lista widocznych jednostek
- [x] Lista widocznych przedmiotów
- [x] Aktualny stan otoczenia
- [x] Historia zmian stanu gry

---

# 4. Bot Modules

## Waypoints
- [x] Tworzenie waypointów
- [x] Usuwanie waypointów
- [x] Edytowanie waypointów
- [x] Ustalanie kolejności waypointów
- [x] Zapisywanie tras
- [x] Wczytywanie tras
- [x] Obsługa różnych typów waypointów
- [x] Obsługa warunków przejścia
- [x] Obsługa punktów oczekiwania
- [x] Obsługa punktów akcji
- [x] Obsługa wielu tras
- [ ] Wizualizacja trasy

## Cavebot
- [x] Automatyczne poruszanie się po trasie
- [x] Obsługa waypointów
- [x] Obsługa warunków na trasie
- [x] Obsługa zmiany pięter
- [x] Obsługa przeszkód w kontrolowanym środowisku
- [x] Wznawianie trasy
- [x] Pauzowanie trasy
- [x] Zatrzymywanie trasy
- [x] Obsługa pętli tras
- [x] Obsługa wielu tras
- [x] Statystyki przebytej trasy

## Targeting
- [x] Rozpoznawanie dostępnych celów
- [x] Lista dostępnych potworów
- [x] Konfiguracja priorytetów celów
- [x] Wybór celu
- [x] Zmiana celu
- [x] Obsługa reguł targetowania
- [x] Konfiguracja ataków
- [x] Konfiguracja dystansu
- [x] Obsługa wielu typów potworów
- [x] Konfiguracja osobna dla każdego potwora

## Looting
- [x] Rozpoznawanie przedmiotów
- [x] Konfiguracja listy przedmiotów
- [x] Lista przedmiotów do zbierania
- [x] Lista przedmiotów ignorowanych
- [x] Konfiguracja kontenerów
- [x] Konfiguracja zasad zbierania
- [x] Obsługa wielu kontenerów
- [x] Statystyki zebranego lootu

## Healing
- [x] Monitorowanie HP
- [x] Monitorowanie MP
- [x] Konfiguracja progów leczenia
- [x] Reguły leczenia postaci
- [x] Reguły uzupełniania many
- [x] Priorytety akcji
- [x] Konfiguracja różnych metod leczenia
- [x] Konfiguracja indywidualnych warunków

## Smart AOE
- [x] Rozpoznawanie celów w obszarze
- [x] Analiza rozmieszczenia celów
- [x] Ocena potencjalnej efektywności ataku obszarowego
- [x] Konfiguracja minimalnej liczby celów
- [x] Konfiguracja priorytetów ataków obszarowych

---

# 5. Lua Scripting

## Lua Engine
- [x] Integracja Lua z Bot Engine
- [x] Ładowanie skryptów
- [x] Zatrzymywanie skryptów
- [x] Restartowanie skryptów
- [x] Obsługa wielu skryptów
- [x] Obsługa błędów skryptów
- [x] Izolacja skryptów
- [x] Limity wykonywania skryptów
- [x] Obsługa zdarzeń Lua

## Lua API
- [x] API dostępu do Game State
- [x] API dostępu do modułów
- [x] API obsługi zdarzeń
- [x] API konfiguracji
- [x] API logowania
- [x] API zarządzania skryptami
- [x] API komunikacji z Engine
- [x] Dokumentacja Lua API

## Script Manager
- [x] Lista dostępnych skryptów
- [x] Włączanie skryptów
- [x] Wyłączanie skryptów
- [x] Konfiguracja skryptów
- [x] Zarządzanie wersjami skryptów
- [x] Obsługa własnych skryptów użytkownika

---

# 6. Multi-Client

- [ ] Obsługa wielu sesji gry
- [ ] Obsługa wielu instancji Engine
- [ ] Przypisywanie Engine do konkretnej sesji
- [ ] Oddzielna konfiguracja każdej instancji
- [ ] Oddzielne profile każdej instancji
- [ ] Oddzielne logi każdej instancji
- [ ] Monitorowanie wszystkich instancji
- [ ] Zarządzanie instancjami z Launchera
- [ ] Uruchamianie i zatrzymywanie wybranych instancji

---

# 7. Monitoring & Statistics

## Monitoring
- [ ] Aktualny status Engine
- [ ] Aktualny status modułów
- [ ] Monitorowanie błędów
- [ ] Monitorowanie czasu działania
- [ ] Historia zdarzeń
- [ ] Historia aktywności modułów

## Statistics
- [ ] Czas działania bota
- [ ] Czas działania poszczególnych modułów
- [ ] Liczba przetworzonych zdarzeń
- [ ] Statystyki lootu
- [ ] Statystyki sesji
- [ ] Historia sesji
- [ ] Raport podsumowujący sesję

---

# 8. Reliability & Safety

- [x] Bezpieczne zatrzymywanie Engine
- [x] Odzyskiwanie po utracie komunikacji
- [x] Obsługa nieoczekiwanej awarii
- [x] Odzyskiwanie konfiguracji
- [x] Walidacja danych wejściowych
- [x] Ograniczenie uprawnień skryptów Lua
- [x] Ochrona komunikacji Launcher – Engine
- [ ] Weryfikacja integralności aktualizacji
- [x] Obsługa błędów Game Adapter
- [x] Testy długotrwałego działania
- [x] Testy wielu instancji
- [x] Testy integracyjne modułów

---

# 9. User Experience

- [x] Nowoczesny interfejs Launchera
- [x] Dashboard
- [x] Panel konfiguracji modułów
- [x] Panel konfiguracji Cavebot
- [x] Panel Targeting
- [x] Panel Looting
- [x] Panel Healing
- [x] Panel Lua Scripts
- [x] Panel logów
- [x] Panel statystyk
- [x] Panel zarządzania instancjami
- [x] System powiadomień
- [x] Obsługa motywów UI
- [x] Responsywne aktualizowanie danych w interfejsie

---

# 10. Release & Distribution

- [x] Gotowy instalator / Standalone Executable (.exe)
- [ ] Automatyczna aktualizacja Launchera
- [ ] Automatyczna aktualizacja Engine
- [ ] Weryfikacja pobranych plików
- [x] Zarządzanie wersjami i skrypty budowania (build_windows.bat, build_mac.sh, GitHub Actions CI/CD)
- [ ] Informowanie o nowych wersjach
- [x] Dokumentacja użytkownika
- [x] Dokumentacja modułów
- [x] Dokumentacja Lua API
- [x] Przygotowanie wersji produkcyjnej (TibiaBot.spec bundle PySide6 + C++ Engine)

---

# Final Goal

Docelowo TibiaBot ma posiadać:

1. Niezależny Launcher z nowoczesnym UI.
2. Niezależny Engine napisany w C++.
3. Dwukierunkową komunikację Launcher – Engine.
4. Modularną architekturę Bot Engine.
5. System rozszerzeń oparty na Lua.
6. Integrację z autoryzowanym środowiskiem gry.
7. Cavebot i system Waypoints.
8. Targeting.
9. Looting.
10. Healing.
11. Smart AOE.
12. Obsługę wielu instancji.
13. System konfiguracji i profili.
14. Monitoring i statystyki.
15. System aktualizacji i zarządzania kontem.
