# Protokol za upravljanje pametnim energetskim mrežama  Smart Grid

Opis projekta: dizajn i implementacija protokola za upravljanje pametnim energetskim mrežama, sa centralnim serverom i smart meter uređajima raspoređenim po regijama.

## Funkcionalnosti

- Registracija svakog smart metera pomoću URI-a i digitalnog certifikata.
- Periodično slanje podataka o potrošnji električne energije sa uređaja ka serveru.
- Dinamički tarifni sistem:
  - cijene zavise od doba dana i ukupnog opterećenja mreže,
  - posebne cijene za industrijske i domaće korisnike,
  - popusti za smanjenu potrošnju u periodima vršnog opterećenja.
- Agregacija podataka i analiza potrošnje po regionima.
- Dvosmjerna komunikacija: server može poslati komandu za privremeno isključenje potrošača pri preopterećenju.
- Praćenje trenutne potrošnje i troška kroz web ili mobilnu aplikaciju.
- Vođenje registra:
  - aktivnih mjernih uređaja,
  - tarifnih planova,
  - historije potrošnje i fakturisanja.
- Regionalni sistem sa najmanje dva odvojena servera (npr. Sarajevo i Mostar) uz sinhronizaciju podataka u realnom vremenu.
- Alarmni sistem za neovlašten pristup ili neuobičajene vrijednosti potrošnje.
- Sva signalizacija ide preko servera; direktna komunikacija između mjernih uređaja nije dozvoljena.

## O projektu

Ovaj repozitorij predstavlja edukativni prototip sistema za upravljanje pametnom energetskom mrežom. Ideja je da se pokaže kako smart meter uređaji mogu slati podatke serveru, kako server može potvrditi primljene poruke i kako se podaci mogu čuvati i objedinjavati po regionima.

Projekat je napisan u jeziku C++ i koristi:

- **Boost.Asio** za TCP i UDP mrežnu komunikaciju,
- **SQLite** za lokalno čuvanje uređaja, mjerenja, alarma i sinhronizacija,
- vlastiti binarni protokol definisan u `protocol/smart_grid_protocol.hpp`,
- TLS 1.3 i post-kvantne algoritme u naprednom sigurnosnom dijelu projekta.

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

## Struktura repozitorija

```text
.
├── protocol/       Definicije poruka i serijalizacija protokola
├── smart_meter/    TCP i UDP klijenti koji predstavljaju smart meter uređaje
├── regional/       Regionalni serveri, registar uređaja i heartbeat
├── central/        Centralni server i sinhronizacija regionalnih podataka
├── database/       SQLite omotač i primjer upisa/čitanja podataka
├── security/       TLS 1.3 i PQC konfiguracija
├── certs/          Konfiguracije certifikata za servere i uređaje
└── web_monitoring/ Jednostavni web server za pregled agregiranih podataka
```

## Preduslovi

Za kompilaciju su potrebni:

- C++ kompajler sa podrškom za C++17,
- Boost biblioteke, posebno Boost.Asio,
- SQLite3 razvojni paket,
- pthread podrška,
- OpenSSL za TLS/PQC komponente.

Na Windows računaru najjednostavnije je koristiti WSL sa Ubuntu okruženjem ili MinGW/MSYS2 okruženje koje ima navedene biblioteke. Dio izvornog koda koristi zaglavlja kao što su `arpa/inet.h` i `endian.h`, pa je Linux/WSL okruženje praktičnije za prve testove.

Primjer instalacije zavisnosti u Ubuntu/WSL okruženju:

```bash
sudo apt update
sudo apt install build-essential libboost-system-dev libsqlite3-dev libssl-dev
```

## Najjednostavniji primjer: server i smart meter

Komande se izvršavaju iz korijenskog direktorija repozitorija. Prvo se kompajliraju server i klijent:

```bash
g++ -std=c++17 regional/server_basic.cpp -o regional/server_basic -lboost_system -pthread
g++ -std=c++17 smart_meter/client_basic.cpp -o smart_meter/client_basic -lboost_system -pthread
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

## UDP heartbeat

Heartbeat služi za provjeru da li je smart meter aktivan. Server prati vrijeme posljednjeg primljenog heartbeat-a i može označiti uređaj kao offline ako nema poruke duže od predviđenog intervala.

Za kompilaciju heartbeat primjera:

```bash
g++ -std=c++17 regional/udp_heartbeat_server.cpp -o regional/udp_heartbeat_server -lboost_system -lsqlite3 -pthread
g++ -std=c++17 smart_meter/udp_heartbeat_client.cpp -o smart_meter/udp_heartbeat_client -lboost_system -pthread
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
g++ -std=c++17 web_monitoring/server.cpp -o web_monitoring/server -lboost_system -lsqlite3 -pthread
./web_monitoring/server
```
