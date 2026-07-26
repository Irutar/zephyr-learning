# Specyfikacja Rozwoju Projektu — my_app

> **Rola dokumentu:** zlecenia architekta dla programisty.
> Każda sekcja opisuje JEDNĄ samodzielną funkcjonalność do zaimplementowania.
> Kolejność zgodna z rekomendowaną ścieżką nauczania.

---

## 1. Zephyr Shell — Interaktywna Konsola

### Cel

Zastąpienie pasywnego logowania interaktywną konsolą dostępną przez UART
(który i tak już masz podłączony). Użytkownik wpisuje komendy, system
odpowiada.

### Wymagania funkcjonalne

1.  **Rejestracja komend** przez Zephyr Shell API (nie ręczny parser).

2.  **Komendy do zaimplementowania:**

    | Komenda | Argumenty | Działanie |
    |---|---|---|
    | `device info` | — | Wypisuje: board, częstotliwość CPU, uptime, wersję firmware, slot bootowania |
    | `wifi status` | — | Wypisuje: stan połączenia, SSID, adres IP, siłę sygnału (RSSI) |
    | `voltage read` | — | Pobiera aktualny odczyt z voltage monitor i wypisuje w mV |
    | `slot info` | — | Wypisuje: z którego slota bootowaliśmy, czy sloty są zsynchronizowane |
    | `gpio count` | — | Wypisuje licznik triggerów GPIO event od startu |
    | `reboot` | `[cold|warm]` | Wykonuje reset. Domyślnie cold przez RTC_CNTL. `warm` = `sys_reboot(SYS_REBOOT_WARM)` |
    | `log level` | `<module> <level>` | Dynamicznie zmienia poziom logowania (`err/wrn/inf/dbg`) dla podanego modułu, np. `log level uart_comm dbg` |

3.  **Shell dostępny przez UART1** (ten sam co obecna konsola).
    Wystarczy podmienić `CONFIG_UART_CONSOLE` na `CONFIG_SHELL` + backends.

4.  **Prompt:** `my_app:~$ ` (ustawiony przez `shell_set_prompt`).

5.  **Autouzupełnianie** (TAB) — darmowe, Zephyr Shell robi to automatycznie
    przy poprawnej rejestracji komend.

### Czego się nauczysz

-   `CONFIG_SHELL`, `CONFIG_SHELL_BACKEND_UART`
-   `SHELL_CMD_REGISTER`, `SHELL_SUBCMD_SET_CREATE`, `SHELL_STATIC_SUBCMD_SET_CREATE`
-   Dynamiczna zmiana log levelu przez `log_filter_set()`
-   Integracja shella z istniejącymi modułami (ekspozycja wewnętrznego stanu)

### Pliki do utworzenia / zmiany

| Plik | Operacja |
|---|---|
| `src/shell/shell_commands.c` | Nowy — rejestracja wszystkich komend |
| `src/shell/shell_commands.h` | Nowy — deklaracja `shell_commands_init()` |
| `src/main.c` | Modyfikacja — wywołanie `shell_commands_init()`, usunięcie heartbeat logów (przeniesione do shella) |
| `prj.conf` | Modyfikacja — `CONFIG_SHELL=y`, wyłączenie `CONFIG_UART_CONSOLE`, dodanie shell backendów |
| `CMakeLists.txt` | Modyfikacja — dodanie `src/shell/shell_commands.c` |

### Kryteria akceptacji

-   [ ] Po resecie widać prompt `my_app:~$`
-   [ ] `device info` wypisuje poprawne dane
-   [ ] `voltage read` zwraca aktualny odczyt z czujnika
-   [ ] `reboot cold` resetuje urządzenie
-   [ ] `log level gpio_event dbg` włącza debug logi tylko dla modułu gpio_event
-   [ ] TAB uzupełnia komendy

---

## 2. BLE Peripheral — Serwis "Monitor Napięcia"

### Cel

ESP32 wystawia się jako urządzenie BLE, które udostępnia aktualne odczyty
napięcia przez GATT. Każdy klient BLE (telefon z nRF Connect, drugi ESP32,
Arduino + HM-10) może odczytać wartość i subskrybować powiadomienia o zmianach.

### Wymagania funkcjonalne

1.  **Nazwa urządzenia w advertising:** `"my_app Monitor"` (konfigurowalne
    przez Kconfig, domyślnie taka).

2.  **Serwis niestandardowy** — UUID: `12345678-1234-1234-1234-123456789ABC`

3.  **Charakterystyki w serwisie:**

    | Nazwa | UUID | Właściwości | Opis |
    |---|---|---|---|
    | Voltage | `12345678-...-0001` | READ \| NOTIFY | Aktualne napięcie w mV (jako `uint16`, little-endian). Klient może odczytać i subskrybować powiadomienia. |
    | Voltage Max | `12345678-...-0002` | READ \| WRITE | Próg alarmowy górny, `uint16` mV. Domyślnie 3600 (3.6V). Zapis zapamiętywany tylko do resetu (RAM). |
    | Voltage Min | `12345678-...-0003` | READ \| WRITE | Próg alarmowy dolny, `uint16` mV. Domyślnie 2800 (2.8V). |
    | Alert Status | `12345678-...-0004` | READ \| NOTIFY | `uint8`: 0 = OK, 1 = powyżej progu max, 2 = poniżej progu min. Aktualizowany automatycznie przy każdej zmianie napięcia. |

4.  **Advertising:**
    -   Interval: 100 ms (szybki, bo to urządzenie zasilane, nie bateryjne).
    -   Flagi: `BT_LE_AD_GENERAL`, `BT_LE_AD_NO_BREDR`.
    -   TX Power: 0 dBm.

5.  **Powiadomienia (Notify):**
    -   `Voltage` — wysyłane automatycznie gdy wartość zmieni się o ≥50 mV
        względem ostatnio wysłanej (histereza, żeby nie spamować).
    -   `Alert Status` — wysyłane natychmiast przy każdej zmianie stanu
        alarmowego.

6.  **Aktualizacja wartości:**
    -   Osobny wątek (`k_thread`) lub periodical work queue co 500 ms odczytuje
        voltage monitor, aktualizuje GATT i sprawdza progi.
    -   Nie w main loop — chcemy pokazać separację odpowiedzialności.

### Konfiguracja Kconfig

```kconfig
config APP_BLE_MONITOR_NAME
    string "BLE device name"
    default "my_app Monitor"

config APP_BLE_MONITOR_VOLTAGE_INTERVAL_MS
    int "Voltage sampling interval (ms)"
    default 500

config APP_BLE_MONITOR_NOTIFY_HYSTERESIS_MV
    int "Notify hysteresis (mV)"
    default 50
```

### Czego się nauczysz

-   `CONFIG_BT`, `CONFIG_BT_PERIPHERAL`, `CONFIG_BT_GATT_DYNAMIC_CLIENT_DB`
-   `bt_enable()`, `bt_le_adv_start()`
-   `BT_GATT_SERVICE_DEFINE`, `BT_GATT_CHARACTERISTIC`
-   `bt_gatt_notify()`
-   `BT_DATA_BYTES` dla advertising data
-   Callback `bt_gatt_attr_write` dla obsługi WRITE z klienta
-   Zarządzanie CCC (Client Characteristic Configuration) — sprawdzanie czy
    klient zasubskrybował notyfikacje

### Pliki do utworzenia / zmiany

| Plik | Operacja |
|---|---|
| `src/ble/monitor_service.c` | Nowy — serwis GATT + wątek aktualizacji |
| `src/ble/monitor_service.h` | Nowy — `ble_monitor_init()` |
| `src/ble/Kconfig` | Nowy — opcje konfiguracyjne BLE |
| `src/main.c` | Modyfikacja — wywołanie `ble_monitor_init()` |
| `boards/esp32_devkitc_esp32_procpu.overlay` | Bez zmian (BLE nie potrzebuje pinów w DTS) |
| `prj.conf` | Modyfikacja — `CONFIG_BT`, back-end ESP32 |
| `CMakeLists.txt` | Modyfikacja — dodanie `src/ble/monitor_service.c` |
| `Kconfig` | Modyfikacja — `rsource "src/ble/Kconfig"` |

### Testowanie

-   Telefon z **nRF Connect** (Android/iOS) — połącz, odczytaj Voltage,
    zasubskrybuj notyfikacje, zmień progi przez WRITE.
-   Arduino + HM-10 jako BLE central — niech Arduino loguje odczyty na Serial.

### Kryteria akceptacji

-   [ ] Urządzenie widoczne w skanie BLE jako `"my_app Monitor"`
-   [ ] Odczyt `Voltage` zwraca aktualną wartość z voltage monitora
-   [ ] Zapis do `Voltage Min` / `Voltage Max` działa i zmienia progi
-   [ ] Notyfikacje `Voltage` przychodzą przy zmianie ≥50 mV
-   [ ] Notyfikacje `Alert Status` przychodzą przy przekroczeniu progu
-   [ ] Po rozłączeniu klienta, urządzenie wraca do advertisingu

---

## 3. LittleFS + Settings — Trwała Pamięć

### Cel

Wykorzystanie partycji `storage` (256 KB na końcu flasha) na trwałe
przechowywanie danych: plików konfiguracyjnych, logów i klucz-wartość
przez Zephyr Settings subsystem.

### 3a. LittleFS — system plików

1.  **Zamontowanie LittleFS** na partycji `storage_partition` podczas bootu.
    -   Użyj `FS_LITTLEFS_DECLARE_CUSTOM_CONFIG` + `fs_mount()`.
    -   Pierwsze uruchomienie: automatyczne sformatowanie jeśli FS nie istnieje.

2.  **Plik logu awaryjnego** — `crash.log`:
    -   Przy starcie systemu sprawdź flagę w RTC memory (osobna komórka obok
        slot selector).
    -   Jeśli flaga NIE jest ustawiona → poprzedni boot był nieczystym resetem
        (crash / watchdog / power loss). Zapisz timestamp i przyczynę do pliku.
    -   Ustaw flagę "clean shutdown" w RTC memory.
    -   Przy poprawnym restarcie przez `image_update_reboot()` — zapisz flagę
        przed rebootem.
    -   Komenda shella: `crashlog show` — wypisuje ostatnie 10 wpisów z pliku.

3.  **Plik konfiguracyjny** — `config.json`:
    -   Zawiera parametry: `loop_period_ms`, `system_info_interval`,
        `ble_advertising_name`, `voltage_threshold_min`, `voltage_threshold_max`.
    -   Przy starcie system próbuje odczytać plik. Jeśli nie istnieje —
        tworzy go z wartościami domyślnymi z Kconfig.
    -   Komenda shella: `config show` / `config set <key> <value>` /
        `config save`.
    -   Format: JSON przez `zephyr/data/json.h` (nie string-parse ręczny).

### 3b. Zephyr Settings Subsystem

1.  **Backend:** NVS (Non-Volatile Storage) na partycji storage.
    -   `CONFIG_SETTINGS`, `CONFIG_SETTINGS_NVS`.

2.  **Trwałe klucze do zaimplementowania:**

    | Klucz | Typ | Opis |
    |---|---|---|
    | `my_app/boot_count` | `uint32` | Licznik bootów (inkrementowany w main przed czymkolwiek) |
    | `my_app/last_boot_slot` | `uint8` | Ostatni slot z którego bootowaliśmy |
    | `my_app/uptime_total` | `uint64` | Sumaryczny uptime wszystkich sesji w sekundach |

3.  **Komenda shella:** `settings show` — wypisuje wszystkie klucze z subtree
    `my_app/`.

4.  **Aktualizacja uptime_total:** nie co sekundę. Przy każdym `print_system_info`
    (co N iteracji) dodaj `uptime_delta` do `uptime_total` i zapisz przez
    `settings_save_one()`.

### Czego się nauczysz

-   `CONFIG_FILE_SYSTEM`, `CONFIG_FS_LITTLEFS`
-   `fs_mount()`, `fs_open()`, `fs_read()`, `fs_write()`, `fs_stat()`
-   Zephyr JSON parser (`zephyr/data/json.h`)
-   `CONFIG_SETTINGS`, `CONFIG_SETTINGS_RUNTIME`, `CONFIG_SETTINGS_NVS`
-   `SETTINGS_STATIC_HANDLER_DEFINE`, `settings_save_one()`, `settings_load()`
-   Wykrywanie nieczystego resetu przez RTC memory

### Pliki do utworzenia / zmiany

| Plik | Operacja |
|---|---|
| `src/storage/littlefs_mount.c` | Nowy — montowanie FS, formatowanie pierwszego montowania |
| `src/storage/crash_log.c` | Nowy — log awaryjny, detekcja nieczystego resetu |
| `src/storage/config_file.c` | Nowy — JSON config file read/write |
| `src/storage/app_settings.c` | Nowy — Settings subsystem handler |
| `src/storage/storage_init.c` | Nowy — `storage_init()` — wywołuje wszystkie powyższe |
| `src/storage/storage_init.h` | Nowy |
| `src/shell/shell_commands.c` | Modyfikacja — nowe komendy `crashlog`, `config`, `settings` |
| `prj.conf` | Modyfikacja — `CONFIG_FILE_SYSTEM`, `CONFIG_FS_LITTLEFS`, `CONFIG_SETTINGS`, `CONFIG_SETTINGS_NVS` |
| `Kconfig` | Modyfikacja — nowe opcje konfiguracyjne |

### Kryteria akceptacji

-   [ ] Po pierwszym bootcie FS jest montowany (log informuje „mounted” lub „formatted and mounted”)
-   [ ] `crashlog show` pokazuje wpisy (lub „no crashes recorded”)
-   [ ] `config show` wypisuje aktualną konfigurację
-   [ ] `config set loop_period_ms 500` + `config save` → po restarcie `config show` pokazuje 500
-   [ ] `settings show` pokazuje `boot_count`, `last_boot_slot`, `uptime_total`
-   [ ] `boot_count` rośnie o 1 przy każdym restarcie

---

## 4. Multi-threading — Wydzielenie Wątków

### Cel

Obecny `main()` to jedna wielka pętla. Rozdziel obowiązki na niezależne wątki
komunikujące się przez message queue. To jest esencja RTOS-a.

### Architektura wątków

```
┌─────────────────────────────────────────────────────────┐
│  Main Thread (prio 5, najniższy — inicjalizacja tylko)  │
│  → Inicjalizuje wszystkie podsystemy                    │
│  → Odpala pozostałe wątki                               │
│  → Kończy działanie (return) lub idle loop              │
└─────────────────────────────────────────────────────────┘
         │
         ├── Sensor Thread (prio 7)
         │   → Co CONFIG_APP_SENSOR_INTERVAL_MS odczytuje voltage monitor
         │   → Wysyła wynik przez k_msgq do BLE Thread i Log Thread
         │   → Sprawdza progi alarmowe, wysyła alert przez k_msgq
         │
         ├── BLE Thread (prio 7)
         │   → Czeka na dane z Sensor Thread przez k_msgq
         │   → Aktualizuje GATT, wysyła notyfikacje
         │   → Obsługuje zdarzenia BLE (connect/disconnect/WRITE)
         │
         ├── UART Comm Thread (prio 8)
         │   → Polluje uart_comm_poll() w pętli
         │   → Otrzymane linie wysyła przez k_msgq do Log Thread
         │   → Odbiera dane do wysłania przez k_msgq
         │
         ├── Log / Wi-Fi Thread (prio 9, najwyższy — musi nadążać)
         │   → Odbiera wiadomości do zalogowania z wszystkich innych wątków
         │     przez k_msgq
         │   → Loguje przez LOG_*() (UART)
         │   → Wysyła przez Wi-Fi (UDP)
         │
         └── GPIO Event — zostaje na workqueue (już jest ok)
```

### Wymagania szczegółowe

1.  **Message Queue jako centralny bus:**
    -   Jedna `k_msgq` dla logów (struktura: `level`, `tag`, `message[128]`).
    -   Jedna `k_msgq` dla odczytów sensora (struktura: `voltage_mv`, `timestamp`).
    -   Jedna `k_msgq` dla alertów (struktura: `alert_type`, `value`).

2.  **Stack size:** każdy wątek dostaje własny stos.
    -   Sensor Thread: 2048 B
    -   BLE Thread: 4096 B (BLE stack potrafi zejść głęboko)
    -   UART Comm Thread: 1536 B
    -   Log Thread: 2048 B
    -   Wszystkie rozmiary definiowane przez Kconfig.

3.  **Synchronizacja startu:**
    -   `k_sem` — main thread daje semafor po zakończeniu inicjalizacji,
        pozostałe wątki czekają na niego przed rozpoczęciem pracy.

4.  **LED heartbeat** — przeniesiony do osobnego, prostego wątku który tylko
    mruga diodą co 500 ms. Priorytet najniższy (5), stack 512 B.

### Komendy shella do dodania

| Komenda | Działanie |
|---|---|
| `threads list` | Wypisuje nazwę, priorytet, stack usage (%) dla każdego wątku |
| `threads stats` | Wypisuje `k_cycle_get_32()` i czas CPU per wątek (jeśli `CONFIG_THREAD_RUNTIME_STATS`) |

### Czego się nauczysz

-   `k_thread_create()`, `K_THREAD_DEFINE`
-   `k_msgq_init()`, `k_msgq_put()`, `k_msgq_get()`
-   `k_sem_init()`, `k_sem_give()`, `k_sem_take()`
-   `k_thread_stack_space_get()` — monitorowanie zużycia stosu
-   `CONFIG_THREAD_MONITOR`, `CONFIG_THREAD_RUNTIME_STATS`
-   `CONFIG_THREAD_NAME`

### Pliki do utworzenia / zmiany

| Plik | Operacja |
|---|---|
| `src/threads/msgq_defs.h` | Nowy — definicje struktur dla message queues |
| `src/threads/sensor_thread.c` | Nowy |
| `src/threads/ble_thread.c` | Nowy |
| `src/threads/uart_thread.c` | Nowy |
| `src/threads/log_thread.c` | Nowy |
| `src/threads/led_thread.c` | Nowy |
| `src/threads/threads_init.c` | Nowy — tworzy wszystkie wątki i kolejki |
| `src/main.c` | Duża modyfikacja — uproszczenie do samej inicjalizacji |
| `src/shell/shell_commands.c` | Modyfikacja — komendy `threads` |
| `CMakeLists.txt` | Modyfikacja |

### Kryteria akceptacji

-   [ ] System startuje, wszystkie wątki działają
-   [ ] `threads list` pokazuje poprawne nazwy i priorytety
-   [ ] Stack usage żadnego wątku nie przekracza 80%
-   [ ] LED mruga niezależnie od reszty systemu
-   [ ] Odczyt napięcia działa i jest logowany przez Log Thread
-   [ ] UART echo działa przez UART Thread
-   [ ] BLE notyfikacje działają przez BLE Thread

---

## 5. MQTT Client — Integracja IoT

### Cel

Publikacja danych z sensora i zdarzeń GPIO do brokera MQTT (np. Mosquitto
na laptopie albo na ESP8266). Subskrybcja topiców do zdalnego sterowania.

### Wymagania funkcjonalne

1.  **Broker:** konfigurowalny przez Kconfig (IP, port, client ID).
    Domyślnie `192.168.50.162:1883`, client ID = `my_app_<board>`.

2.  **Topici publikowane (PUB):**

    | Topic | QoS | Retain | Payload | Interwał |
    |---|---|---|---|---|
    | `my_app/status` | 1 | Tak | `online` (przy connect), `offline` (LWT) | — |
    | `my_app/voltage` | 0 | Nie | `{ "mv": 3300, "timestamp": 123456 }` (JSON) | Co odczyt sensora (~1s) |
    | `my_app/gpio/count` | 0 | Nie | `{ "count": 42 }` | Przy każdej zmianie licznika |
    | `my_app/uptime` | 0 | Nie | `{ "seconds": 3600 }` | Co 60 s |

3.  **LWT (Last Will Testament):**
    -   Topic: `my_app/status`, payload: `offline`, QoS 1, retain.
    -   Ustawiany przy connect.

4.  **Topici subskrybowane (SUB):**

    | Topic | QoS | Działanie |
    |---|---|---|
    | `my_app/reboot` | 1 | Odbiera `"cold"` lub `"warm"` — wykonuje reboot. |
    | `my_app/led` | 1 | Odbiera `"on"` / `"off"` / `"toggle"` — steruje LED. |
    | `my_app/log/level` | 1 | Odbiera `"err"` / `"wrn"` / `"inf"` / `"dbg"` — zmienia globalny log level. |

5.  **Reconnect:**
    -   Jeśli połączenie MQTT zostanie zerwane, wątek MQTT próbuje reconnect
        co 5 sekund (exponential backoff: 5, 10, 20, ..., max 120 s).

6.  **MQTT wątek** — osobny, priorytet 7, stack 4096 B. Komunikuje się
    z Sensor Thread i GPIO Event przez istniejące message queues.

### Konfiguracja Kconfig

```kconfig
config APP_MQTT_BROKER_IP
    string "MQTT broker IP"
    default "192.168.50.162"

config APP_MQTT_BROKER_PORT
    int "MQTT broker port"
    default 1883

config APP_MQTT_CLIENT_ID
    string "MQTT client ID"
    default "my_app_esp32"
```

### Czego się nauczysz

-   `CONFIG_MQTT_LIB` (Zephyr built-in MQTT, nie zewnętrzna biblioteka)
-   `mqtt_connect()`, `mqtt_subscribe()`, `mqtt_publish()`
-   `mqtt_live()`, `mqtt_input()`
-   LWT (Last Will Testament)
-   Integracja MQTT z własnym systemem kolejek/wątków
-   JSON payloady dla IoT (standardowy format)

### Pliki do utworzenia / zmiany

| Plik | Operacja |
|---|---|
| `src/mqtt/mqtt_client.c` | Nowy — wątek MQTT, connect, publish, subscribe |
| `src/mqtt/mqtt_client.h` | Nowy |
| `src/threads/mqtt_thread.c` | Nowy (lub zintegrowane z mqtt_client.c) |
| `prj.conf` | Modyfikacja — `CONFIG_MQTT_LIB` |
| `Kconfig` | Modyfikacja — opcje MQTT |

### Kryteria akceptacji

-   [ ] Po połączeniu z Wi-Fi, MQTT łączy się z brokerem
-   [ ] `my_app/status` pokazuje `online` (z retain)
-   [ ] `my_app/voltage` pojawia się regularnie na brokerze
-   [ ] Wysłanie `"toggle"` na `my_app/led` przełącza diodę
-   [ ] Wysłanie `"cold"` na `my_app/reboot` resetuje urządzenie
-   [ ] Po odcięciu zasilania ESP32, `my_app/status` pokazuje `offline` (LWT)
-   [ ] Reconnect działa po zaniku i powrocie Wi-Fi

---

## 6. Hardware Watchdog

### Cel

Wykrywanie zawieszenia systemu i automatyczny reset sprzętowy.

### Wymagania

1.  **IWDT (ESP32 Internal Watchdog Timer):**
    -   Konfiguracja przez devicetree (już jest `&iwdt0` w DTS ESP32).
    -   Timeout: 5 sekund (konfigurowalny przez Kconfig).
    -   Nie używa standardowego Zephyr watchdog API jeśli ESP32 IWDT ma
        własny sterownik. Sprawdź co jest dostępne w Zephyrze dla ESP32.

2.  **Strategia karmienia watchdoga:**
    -   **NIE** w main loop.
    -   Osobny wątek (priorytet 3 — najwyższy możliwy w Zephyrze, bo to
        bezpieczeństwo). Stack: 512 B.
    -   Wątek co 2 sekundy sprawdza, czy każdy zarejestrowany wątek roboczy
        "odżył" w ciągu ostatnich 4 sekund.
    -   Każdy monitorowany wątek co sekundę ustawia swój `alive_timestamp`
        (atomowy `uint32_t` z `k_uptime_get_32()`).
    -   Wątek watchdoga sprawdza timestampy — jeśli którykolwiek wątek
        nie odżył → NIE karwi watchdoga → hardware reset.

3.  **Rejestracja wątków do monitorowania:**
    -   `watchdog_thread_register(const char *name)` — dodaje wątek do listy
        monitorowanych.
    -   `watchdog_thread_alive(const char *name)` — wątek zgłasza, że żyje.
    -   API w `include/app/watchdog.h`.

4.  **Komenda shella:** `watchdog status` — pokazuje listę monitorowanych
    wątków i czas od ostatniego "alive".

### Czego się nauczysz

-   ESP32 IWDT przez Zephyr WDT API (lub direct register access)
-   `CONFIG_WATCHDOG`
-   Priorytety wątków w Zephyrze (najwyższy priorytet = 0)
-   Atomowe timestampy do komunikacji między wątkami
-   Wzorzec "software watchdog" na bazie hardware watchdoga

### Kryteria akceptacji

-   [ ] Normalna praca: system nie resetuje się (watchdog karmiony)
-   [ ] Sztuczne zawieszenie (dodaj `while(1);` w Sensor Thread) →
        po 5 s hardware reset
-   [ ] `watchdog status` pokazuje poprawne timestampy

---

## 7. PWM — Serwo i LED Fading

### Cel

Użycie kontrolera PWM (LEDC w ESP32) do płynnego sterowania jasnością LED
i pozycją serwa.

### 7a. LED Fading

1.  **Nowa dioda PWM** — zdefiniowana w devicetree jako `pwm-leds`.
    Możesz użyć istniejącego GPIO2 (obecny LED) albo osobnego pinu.
    Proponuję osobny pin, żeby zachować heartbeat na GPIO2.

2.  **Efekt "oddychania" (breathing):**
    -   Osobny wątek (priorytet 5, stack 512 B).
    -   Używa `pwm_set_dt()` z wartością zmieniającą się sinusoidalnie
        (pre-computed lookup table, 256 wartości dla uproszczenia).
    -   Pełny cykl: 3 sekundy (Kconfig).

3.  **Komenda shella:**
    -   `pwm led <0-100>` — ustawia jasność ręcznie (przerywa breathing).
    -   `pwm led breathe` — wznawia breathing.
    -   `pwm status` — pokazuje stan wszystkich kanałów PWM.

### 7b. Servo Control (opcjonalnie, ale fajne)

1.  **Drugi kanał PWM** — podłącz serwo (SG90 lub podobne) do GPIO.
    Zasilanie serwa OSOBNO (nie z pinu 3.3V ESP32 — za duży prąd).

2.  **Pozycja kąta:** `pwm_set_dt()` z pulse width 500–2500 µs (0°–180°).
    Okres: 20 ms (50 Hz).

3.  **Komenda shella:**
    -   `servo angle <0-180>` — ustawia kąt.
    -   `servo sweep` — płynne przejście 0°→180°→0° w 5 sekund.

### Devicetree (dodanie do app.overlay)

```dts
/ {
    pwm_leds {
        compatible = "pwm-leds";
        pwm_led0: pwm_led_0 {
            pwms = <&ledc0 0 0 PWM_POLARITY_NORMAL>;
            label = "Breathing LED";
        };
    };

    servo: servo_motor {
        compatible = "pwm-servo";
        pwms = <&ledc0 1 0 PWM_POLARITY_NORMAL>;
        min-pulse = <500>;
        max-pulse = <2500>;
    };
};
```

> **Uwaga:** `ledc0` to kontroler PWM ESP32. Sprawdź dokładną etykietę
> w bindings ESP32 (`zephyr/dts/bindings/pwm/espressif,esp32-ledc.yaml`).

### Czego się nauczysz

-   `CONFIG_PWM`, `CONFIG_PWM_ESP32_LEDC`
-   `pwm_dt_spec`, `pwm_set_dt()`, `pwm_set_pulse_dt()`
-   PWM polarity, period, pulse width
-   Servo timing (500–2500 µs)

### Kryteria akceptacji

-   [ ] LED płynnie oddycha po starcie
-   [ ] `pwm led 50` zatrzymuje breathing, LED świeci na 50%
-   [ ] `pwm led breathe` wznawia breathing
-   [ ] (Serwo) `servo angle 90` ustawia serwo na środek
-   [ ] (Serwo) `servo sweep` płynnie przejeżdża cały zakres

---

## 8. SPI — Komunikacja z Arduino jako SPI Slave

### Cel

ESP32 jako SPI Master, Arduino UNO jako SPI Slave. Dwukierunkowa komunikacja
z przerwaniem po stronie Arduino (ESP32 steruje zegarem).

### Wymagania funkcjonalne

1.  **Warstwa fizyczna (3 piny + GND):**
    -   ESP32 GPIO18 = SCK (clock)
    -   ESP32 GPIO23 = MOSI (master out, slave in)
    -   ESP32 GPIO19 = MISO (master in, slave out)
    -   ESP32 GPIO5  = CS   (chip select)
    -   Wspólna masa.

2.  **Protokół ramki:**
    -   Stały rozmiar ramki: 16 bajtów.
    -   Bajt 0: komenda z ESP32 do Arduino (1 B).
    -   Bajty 1–7: payload z ESP32 do Arduino (7 B).
    -   Bajt 8: status z Arduino do ESP32 (1 B).
    -   Bajty 9–15: payload z Arduino do ESP32 (7 B).
    -   CS = LOW przed transmisją, HIGH po.

3.  **Komendy:**

    | Kod | Nazwa | Payload ESP32→Arduino | Odpowiedź Arduino→ESP32 |
    |---|---|---|---|
    | 0x01 | `PING` | — | `PONG` (jako 4 bajty ASCII) |
    | 0x02 | `ADC_READ` | numer kanału (0–5) | wartość ADC (2 bajty, uint16 LE) |
    | 0x03 | `GPIO_WRITE` | pin + stan (2 bajty) | poprzedni stan pinu (1 bajt) |
    | 0x04 | `GPIO_READ` | pin (1 bajt) | stan pinu (1 bajt) |

4.  **Interwał pollingu:** co 500 ms ESP32 wysyła ramkę do Arduino.
    Jeśli nie ma aktywnej komendy, wysyła `PING`.

5.  **Arduino slave:**
    -   Szkic Arduino (dostarczony jako `support/arduino_spi_slave/`).
    -   Używa `SPI.transfer()` w trybie slave.
    -   Reaguje na CS jako trigger do przygotowania odpowiedzi.
    -   Czyta ADC, steruje GPIO na żądanie.

6.  **Komenda shella:**
    -   `spi ping` — wysyła PING, wypisuje odpowiedź.
    -   `spi adc <channel>` — odczytuje kanał ADC Arduino.
    -   `spi gpio write <pin> <0|1>` — ustawia pin Arduino.
    -   `spi gpio read <pin>` — odczytuje pin Arduino.

### Czego się nauczysz

-   `CONFIG_SPI`, `CONFIG_SPI_ESP32`
-   `spi_dt_spec`, `spi_transceive_dt()`
-   SPI mode (CPOL, CPHA), bit order, frequency
-   Devicetree dla SPI device
-   Protokół komunikacyjny master-slave z ramkami

### Kryteria akceptacji

-   [ ] `spi ping` zwraca `PONG`
-   [ ] `spi adc 0` zwraca wartość z przetwornika Arduino
-   [ ] `spi gpio write 13 1` zapala wbudowaną diodę Arduino (pin 13)
-   [ ] `spi gpio read 13` zwraca aktualny stan pinu
-   [ ] Komunikacja jest stabilna (brak timeoutów)

---

## 9. Testy Jednostkowe z Zephyr Test Framework (ztest)

### Cel

Nauczenie się pisania testów dla kodu Zephyra. Testujemy logikę niezależną
od hardware'u.

### Testy do zaimplementowania

1.  **`uart_comm` — testy jednostkowe:**
    -   Test: `test_circular_index_add` — sprawdza poprawność operacji
        na indeksach circular buffera (wrap-around).
    -   Test: `test_line_extract_empty` — brak danych → brak linii.
    -   Test: `test_line_extract_single` — jedna linia zakończona `\n`.
    -   Test: `test_line_extract_multiple` — wiele linii w buforze.
    -   Test: `test_line_extract_no_newline` — dane bez `\n` nie są zwracane.
    -   **Uwaga:** testy muszą być napisane tak, żeby nie wymagały prawdziwego
        UART-a. Być może trzeba zrefaktorować `uart_comm` i wyciągnąć czystą
        logikę circular buffera do osobnego modułu.

2.  **`slot_selector` — testy jednostkowe:**
    -   Test: `test_magic_read_write` — zapis i odczyt wartości przez RTC pointer.
    -   Test: `test_boot_source_read_write` — j.w. dla boot source.
    -   **Uwaga:** na ESP32 nie ma QEMU, więc te testy NIE będą działać
        na hoście. Trzeba je uruchamiać na prawdziwym sprzęcie (twister
        z `--device-testing`). Alternatywnie: mockowanie przez `#ifdef`.

3.  **`voltage_monitor` — testy integracyjne:**
    -   Test: `test_i2c_byte_assembly` — symulacja odczytu 2 bajtów z I2C,
        sprawdzenie poprawności złożenia wartości.
    -   To wymaga mocka I2C API — zaawansowane, opcjonalne.

### Struktura katalogów

```
tests/
  uart_comm/
    CMakeLists.txt
    test_uart_comm.c
    testcase.yaml
  slot_selector/
    CMakeLists.txt
    test_slot_selector.c
    testcase.yaml
```

### Czego się nauczysz

-   `CONFIG_ZTEST`, `CONFIG_ZTEST_NEW_API`
-   `ZTEST`, `ZTEST_SUITE`, `ztest_run_all`
-   Twister (`scripts/twister`)
-   `testcase.yaml` — konfiguracja testów dla twistera
-   Izolacja logiki od hardware'u (dependency injection / refactoring dla
    testowalności)

### Kryteria akceptacji

-   [ ] `twister -p esp32_devkitc_esp32_procpu -s tests/uart_comm` przechodzi
-   [ ] `twister -p esp32_devkitc_esp32_procpu -s tests/slot_selector` przechodzi
-   [ ] Testy nie wymagają ręcznej interwencji (automatyczne)

---

## 📐 Podsumowanie — Ścieżka Implementacji

```
Tydzień 1–2:  Zephyr Shell          ← Natychmiastowa korzyść
Tydzień 3–4:  BLE Peripheral       ← Nowy duży subsystem
Tydzień 5–6:  LittleFS + Settings   ← Trwałość danych
Tydzień 7–8:  Multi-threading       ← Refaktor architektury
Tydzień 9–10: MQTT Client           ← IoT stack
Tydzień 11:   Hardware Watchdog     ← Niezawodność
Tydzień 12:   PWM (LED + Servo)     ← Zabawa sprzętem
Tydzień 13:   SPI z Arduino         ← Komunikacja międzyprocesorowa
Równolegle:   Testy (ztest)         ← Przy okazji refaktorów
```

---

*Dokument żyje — po zaimplementowaniu każdego modułu dopisz sekcję
"Odstępstwa od specyfikacji" z tym co wyszło inaczej i dlaczego.*
