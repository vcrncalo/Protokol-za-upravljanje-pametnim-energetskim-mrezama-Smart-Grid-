# Protokol za upravljanje pametnim energetskim mrežama - **Smart Grid Management Protocol**
**Faculty of Electrical Engineering (*Elektrotehnički fakultet Univerziteta u Sarajevu*)** 

**University of Sarajevo Department of Telecommunications (*Odsjek za telekomunikacije*)**

Opis projekta: dizajn i implementacija protokola za upravljanje pametnim energetskim mrežama, sa centralnim serverom i smart meter uređajima raspoređenim po regijama.

## Funkcionalnosti

- Registracija svakog smart metera pomoću URI-a i digitalnog certifikata.
- Periodično slanje podataka o potrošnji električne energije sa uređaja ka serveru.
- Dinamički tarifni sistem:
  - cijene zavise od doba dana i ukupnog opterećenja mreže,
  - posebne cijene za industrijske i domaće korisnike,
  - popusti za smanjenu potrošnju u periodima vršnog opterećenja.
- Agregacija podataka i analiza potrošnje po regionima.
- Dvosmjerna komunikacija: regionalni server može poslati naredbu za smanjenje potrošnje (`REDUCE_CONSUMPTION_CMD`) pri povećanom opterećenju mreže, a Smart Meter potvrđuje izvršenje porukom `COMMAND_ACK`.
- Praćenje trenutne potrošnje i troška kroz web ili mobilnu aplikaciju.
- Vođenje registra:
  - aktivnih mjernih uređaja,
  - tarifnih planova,
  - historije potrošnje i fakturisanja.
- Regionalni sistem sa najmanje dva odvojena servera (npr. Sarajevo i Mostar) uz sinhronizaciju podataka u realnom vremenu.
- Alarmni sistem za neovlašten pristup ili neuobičajene vrijednosti potrošnje.
- Sva signalizacija ide preko servera

## O projektu

Ovaj repozitorij predstavlja edukativni prototip sistema za upravljanje pametnom energetskom mrežom. Ideja je da se pokaže kako smart meter uređaji mogu slati podatke serveru, kako server može potvrditi primljene poruke i kako se podaci mogu čuvati i objedinjavati po regionima.

Projekat je napisan u jeziku C++ i koristi:

- **Boost.Asio** za TCP i UDP mrežnu komunikaciju,
- **SQLite** za lokalno čuvanje uređaja, mjerenja, alarma i sinhronizacija,
- vlastiti binarni protokol definisan u `protocol/smart_grid_protocol.hpp`,
- **TLS 1.3 + PQC** za kvantno-sigurnu komunikaciju između uređaja i servera.

### Kvantno-sigurna komunikacija (*Quantum-safe communication*)

Za zaštićenu komunikaciju koristi se TLS 1.3 u kombinaciji sa post-kvantnom kriptografijom (PQC). TLS obezbjeđuje šifrovan i autentifikovan kanal, dok PQC mehanizmi pružaju zaštitu od napada.

Ovaj dio zahtijeva OpenSSL verziju koja podržava navedene algoritme. Osnovni TCP, UDP i SQLite primjeri mogu se koristiti i bez PQC podrške.

Trenutna realizacija je podijeljena na manje primjere. Najjednostavniji primjer je komunikacija između `server_basic` i `client_basic` programa. On omogućava da se bez pokretanja cijelog sistema vidi osnovni tok rada: registracija smart metera, slanje mjerenja i potvrda servera.

## Kako sistem radi

Pojednostavljeni tok komunikacije izgleda ovako:

1. Smart meter otvara TCP vezu prema regionalnom serveru.
2. Smart meter šalje poruku `REGISTER_REQ` sa URI adresom uređaja, regionom i tipom korisnika.
3. Server vraća `REGISTER_ACK` i potvrđuje registraciju.
4. Smart meter periodično šalje `CONSUMPTION_REPORT` poruke.
5. Server za svako mjerenje vraća `CONSUMPTION_ACK`.
6. Regionalni server može podatke upisati u lokalnu bazu i proslijediti ih centralnom serveru kroz `REGION_SYNC` poruku.

Svaka poruka počinje zaglavljem od 4 bajta:

| Polje | Veličina | Opis |
| --- | ---: | --- |
| Verzija | 1 bajt | Verzija protokola, trenutno `1` |
| Tip | 1 bajt | Vrsta poruke, na primjer `REGISTER_REQ` |
| Dužina payload-a | 2 bajta | Dužina podataka koji slijede iza zaglavlja |

Podaci su serijalizovani u mrežni redoslijed bajtova, a funkcije za serijalizaciju i deserijalizaciju nalaze se u protokolskom headeru.

### Byte stream i data stream

**Byte stream** je kontinuirani niz bajta koji se prenosi preko TCP/TLS veze. U ovom projektu poruke se šalju kao serijalizovani bajti, a 4byte zaglavlje omogućava serveru da prepozna tip poruke i dužinu njenog payload-a.

Ovaj način koriste poruke kao što su `REGISTER_REQ`,
`REGISTER_ACK`, `CONSUMPTION_REPORT`, `CONSUMPTION_ACK`,
`TARIFF_UPDATE`, `REDUCE_CONSUMPTION_CMD`, `COMMAND_ACK`,
`REGION_SYNC` i `REGION_SYNC_ACK`.

**Data stream** je aplikacijski tok uzoraka potrošnje predstavljen porukama `DATA_STREAM_SAMPLE`. Smart meter ih šalje kontinuirano, sa URI adresom, vremenom, rednim brojem, potrošnjom i trenutnom snagom; server ih obrađuje bez zasebnog ACK-a za svaki uzorak.

## Tabela protokolskih poruka

Svaka poruka ima zaglavlje od 4 bajta: verziju protokola, tip poruke i dužinu payload-a. Vrijednosti `kWh` predstavljaju energiju, a `kW` trenutnu snagu.

| Tip poruke | Smjer | Svrha | Glavni podaci |
| --- | --- | --- | --- |
| `REGISTER_REQ` | smart meter -> regionalni server | Zahtjev za registraciju uređaja | URI uređaja, region, tip korisnika |
| `REGISTER_ACK` | regionalni server -> smart meter | Potvrda ili odbijanje registracije | status |
| `CONSUMPTION_REPORT` | smart meter -> regionalni server | Slanje izmjerene potrošnje | URI, vrijeme, kWh, kW |
| `CONSUMPTION_ACK` | regionalni server -> smart meter | Potvrda prijema mjerenja | status |
| `TARIFF_UPDATE` | regionalni server -> smart meter | Slanje nove cijene električne energije | cijena po kWh |
| `REDUCE_CONSUMPTION_CMD` | regionalni server -> smart meter | Zahtjev za smanjenje opterećenja | URI, ciljna snaga u kW |
| `COMMAND_ACK` | smart meter -> regionalni server | Potvrda izvršavanja komande | status |
| `HEARTBEAT` | smart meter -> regionalni server | Provjera da je uređaj aktivan | URI, vrijeme slanja |
| `DEVICE_OFFLINE` | uređaj/server -> server | Signalizacija neuobičajenog stanja ili pristupa | Alarm kada Smart Meter prestane slati UDP heartbeat poruke |
| `DATA_STREAM_SAMPLE` | smart meter -> regionalni server | Kontinuirani tok uzoraka potrošnje | URI, vrijeme, redni broj, kWh, kW |
| `REGION_SYNC` | regionalni server -> centralni server | Sinhronizacija mjerenja između regija | izvorni region, URI, vrijeme, kWh, kW |
| `REGION_SYNC_ACK` | centralni server -> regionalni server | Potvrda regionalne sinhronizacije | status |

Status je bajt koji server ili uređaj koristi da označi uspjeh ili neuspjeh operacije. Poruke `DATA_STREAM_SAMPLE` nemaju zaseban ACK za svaki uzorak; nastavak rada protokola potvrđuje se narednim odgovorom, najčešće `CONSUMPTION_ACK`.

## Struktura repozitorija

```text
.
├── protocol/       Definicije poruka i serijalizacija protokola
├── smart_meter/    TCP i UDP klijenti koji predstavljaju smart meter uređaje
├── regional/       Regionalni serveri, registar uređaja i heartbeat
├── central/        Centralni server i sinhronizacija regionalnih podataka
├── database/       SQLite klasa i primjer upisa/čitanja podataka
├── security/       TLS 1.3 i PQC konfiguracija
├── certs/          Konfiguracije certifikata za servere i uređaje
│   └── pqc/        PQC konfiguracije certifikata
├── tests/          Funkcionalni testovi sistema
│   └── benchmark/  Benchmark za mjerenje performansi
└── web_monitoring/ Jednostavni web server za pregled agregiranih podataka
```

## Podešavanje okruženja za Boost.Asio

Boost.Asio je biblioteka za mrežno programiranje u jeziku C++. U ovom projektu koristi se **Boost verzija Asio biblioteke**, što se vidi po include direktivi `#include <boost/asio.hpp>` i namespace-u `boost::asio`. Zbog toga je potrebno instalirati Boost biblioteke, a ne samo standalone Asio repozitorij.

### Korak 1: Instalacija osnovnih alata

Potrebni su C++ kompajler koji podržava najmanje C++17 standard, CMake i osnovni alati za izgradnju projekta.

**Ubuntu / Debian / WSL**

```bash
sudo apt update
sudo apt install build-essential cmake git
```

**Fedora / RHEL**

```bash
sudo dnf install gcc-c++ make cmake git
```

**macOS**

Instalirajte Xcode command line tools:

```bash
xcode-select --install
```

Zatim instalirajte CMake i Boost pomoću Homebrew-a:

```bash
brew install cmake boost
```

Na Windows računaru preporučuje se korištenje WSL-a sa Ubuntu distribucijom. Dio izvornog koda koristi zaglavlja `arpa/inet.h` i `endian.h`, zbog čega je Linux/WSL okruženje najjednostavnije za pokretanje postojećih primjera.

### Korak 2: Instalacija Boost.Asio i ostalih biblioteka

U Ubuntu/WSL okruženju instalirajte Boost.System, SQLite3 i OpenSSL:

```bash
sudo apt install libboost-system-dev libsqlite3-dev libssl-dev
```

Paket `libboost-system-dev` omogućava povezivanje programa sa Boost.Asio komponentom koja se koristi u TCP i UDP primjerima. SQLite3 je potreban za rad baze podataka, a OpenSSL za TLS i PQC dio projekta.

Na Fedori/RHEL-u koristite:

```bash
sudo dnf install boost-system-devel sqlite-devel openssl-devel
```

Na macOS-u, ako Boost nije instaliran ranije, koristite:

```bash
brew install boost sqlite openssl
```

### Korak 3: Provjera instalacije

Provjerite da kompajler, CMake i Boost postoje u okruženju:

```bash
g++ --version
cmake --version
```

### Korak 4: Napomena o Asio biblioteci

Ovaj projekat koristi Boost.Asio, a ne standalone Asio. To se vidi po include putanji `boost/asio.hpp` i namespace-u `boost::asio`. Zbog toga je potrebna Boost instalacija iz prethodnog koraka; preuzimanje standalone Asio biblioteke nije potrebno.

## Najjednostavniji primjer: server i smart meter

Komande se izvršavaju iz glavnog direktorija repozitorija. Prvo se kompajliraju server i klijent:

```bash
g++ regional/server_basic.cpp -o regional/server_basic -lboost_system -pthread
g++ smart_meter/client_basic.cpp -o smart_meter/client_basic -lboost_system -pthread
```

Otvorite dva terminala. U prvom pokrenete server:

```bash
./regional/server_basic
```

Server će čekati na TCP portu `5000` i prikazati:

```text
Server slusa na portu 5000...
```

U drugom terminalu pokrenete smart meter:

```bash
./smart_meter/client_basic
```

Očekivani tok je približno:

```text
Povezan sa serverom!
Serijalizovani REGISTER_REQ poslan.
Smart meter je uspjesno registrovan!
CONSUMPTION_REPORT poslan: 2.35 kWh
CONSUMPTION_ACK primljen.
```

Klijent zatim šalje ukupno pet mjerenja, sa pauzom od pet sekundi između mjerenja. Nakon završetka klijenta server završava obradu te veze. Ovo je osnovni demonstracioni primjer i ne predstavlja još kompletan produkcijski sistem.

## SQLite baza podataka

Klasa `Database` inicijalizuje SQLite bazu i kreira tabele za uređaje, potrošnju, tarife, komande, alarme i regionalnu sinhronizaciju. Osnovni test baze može se kompajlirati i pokrenuti ovako:

```bash
g++ -std=c++17 database/database_test.cpp -o database/database_test -lsqlite3
./database/database_test
```

Test koristi bazu `database/smartgrid.db`, registruje uređaj `smartgrid://sarajevo/meter/001` i ispisuje dostupne podatke. Baza se kreira automatski ako ne postoji.

## Funkcionalni testovi

Skripte u direktoriju `tests/` provjeravaju funkcionalne tokove sistema. Pokreću se iz glavnog direktorija repozitorija i očekuju već izgrađene izvršne datoteke. Za rad u Linuxu ili WSL-u prvo omogućite izvršavanje skripti:

```bash
chmod +x tests/*.sh
```

Testovi koji koriste TLS i regionalnu sinhronizaciju zahtijevaju pokrenute odgovarajuće servere, na primjer:

```bash
./central/sync_server
./regional/server_async
```

U drugom terminalu pojedinačni test pokreće se ovako:

```bash
./tests/test_01_region1_registration.sh
```

| Test | Šta provjerava | Očekivani rezultat |
| --- | --- | --- |
| `test_01_region1_registration.sh` | TLS 1.3/PQC handshake i registraciju uređaja `sarajevo/meter/001` | uređaj je registrovan u Regionu 1 |
| `test_02_region2_registration.sh` | Registraciju uređaja `mostar/meter/002` | uređaj je registrovan u Regionu 2 |
| `test_03_invalid_uri.sh` | Odbijanje namjerno neispravnog uređaja `mostar/meter/999` | registracija je odbijena |
| `test_04_consumption_sync.sh` | `CONSUMPTION_REPORT`, `CONSUMPTION_ACK` i `REGION_SYNC` | novi zapis postoji u `database/central.db` |
| `test_05_dynamic_tariff.sh` | Više mjerenja i izračun dinamičke tarife | primljena je `TARIFF_UPDATE` poruka |
| `test_06_reduce_command.sh` | Reakciju industrijskog uređaja na veliko opterećenje | primljene su `REDUCE_CONSUMPTION_CMD` i `COMMAND_ACK` poruke |
| `test_07_data_stream.sh` | Kontinuirano slanje uzoraka | primljena su najmanje tri `DATA_STREAM_SAMPLE` uzorka |

Testovi upisuju detaljan izlaz u `tests/test_0X_output.log`. Vrijeme trajanja zavisi od timeouta u skripti i od toga koliko brzo server odgovara.

## Benchmark testovi

Benchmark programi nalaze se u `tests/benchmark/` i služe za mjerenje performansi, a ne za provjeru osnovne funkcionalnosti. Trenutno postoje:

| Benchmark | Mjerenje |
| --- | --- |
| `database_benchmark.cpp` | SQLite INSERT/SELECT operacije u memorijskoj bazi |
| `registration_benchmark.cpp` | TCP povezivanje, PQC TLS handshake i registracija |
| `consumption_rtt_benchmark.cpp` | RTT od `CONSUMPTION_REPORT` do `CONSUMPTION_ACK` |
| `data_stream_benchmark.cpp` | Propusnost toka `DATA_STREAM_SAMPLE` poruka |

Detaljne komande za kompajliranje i pokretanje benchmarka nalaze se u [tests/benchmark/README.md](tests/benchmark/README.md).

## UDP heartbeat

Heartbeat služi za provjeru da li je smart meter aktivan. Server prati vrijeme posljednjeg primljenog heartbeat-a i može označiti uređaj kao offline ako nema poruke duže od predviđenog intervala.

Za kompilaciju heartbeat primjera:

```bash
g++ regional/udp_heartbeat_server.cpp -o regional/udp_heartbeat_server -lboost_system -lsqlite3 -pthread
g++ smart_meter/udp_heartbeat_client.cpp -o smart_meter/udp_heartbeat_client -lboost_system -pthread
```

Heartbeat server koristi UDP port `5002`. Server i klijent se pokreću u odvojenim terminalima:

```bash
./regional/udp_heartbeat_server
./smart_meter/udp_heartbeat_client
```

## Centralni i regionalni dio

Napredniji primjer sastoji se od regionalnih servera i centralnog servera. Regionalni server obrađuje uređaje svoje regije, računa trenutno opterećenje, može izračunati dinamičku tarifu i sinhronizuje mjerenja prema centralnom serveru. Centralni server podatke upisuje u `database/central.db`, gdje ih web monitoring može prikazati.

Ovaj dio koristi certifikate iz direktorija `certs/` i TLS konfiguraciju iz `security/pqc_tls.hpp`. Zbog toga prvo treba pripremiti certifikate i provjeriti da lokalna verzija OpenSSL-a podržava grupe `X25519MLKEM768` i potpisni algoritam `ML-DSA-44`. Ako ti algoritmi nisu dostupni, osnovni TCP, UDP i SQLite primjeri i dalje se mogu koristiti nezavisno.

## Web monitoring

Web server čita agregirane podatke iz baze `database/central.db` i prikazuje ih u jednostavnom web interfejsu. Može se pokrenuti zasebno:

```bash
g++ web_monitoring/server.cpp -o web_monitoring/server -lboost_system -lsqlite3 -pthread
./web_monitoring/server
```
