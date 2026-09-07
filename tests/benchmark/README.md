# Smart Grid System - Benchmark testovi

Ovaj direktorij sadrži benchmark testove razvijene za mjerenje
performansi implementiranog Smart Grid komunikacijskog sistema.

Benchmark testovi su odvojeni od glavnih komponenti sistema i ne
mijenjaju postojeću implementaciju Smart Metera, regionalnih servera
ili centralnog servera.

Implementirana su četiri benchmark testa:

1. SQLite INSERT/SELECT benchmark
2. PQC TLS i registracija
3. CONSUMPTION_REPORT -> CONSUMPTION_ACK RTT
4. DATA_STREAM_SAMPLE propusnost


## 1. SQLite INSERT/SELECT benchmark

Datoteka:

    database_benchmark.cpp

Benchmark mjeri performanse SQLite sloja sistema.

Koristi se izolovana SQLite baza u radnoj memoriji (`:memory:`), tako
da benchmark ne mijenja stvarne baze:

- central.db
- region1.db
- region2.db

Test izvršava:

- 10.000 INSERT operacija
- 10.000 SELECT operacija

Za obje vrste operacija mjere se ukupno vrijeme, prosječno vrijeme po
operaciji i propusnost.

Kompajliranje:

    g++ tests/benchmark/database_benchmark.cpp \
    -o tests/benchmark/database_benchmark \
    -lsqlite3

Pokretanje:

    ./tests/benchmark/database_benchmark

Dobijeni rezultat:

    INSERT:
    Broj operacija: 10000
    Ukupno vrijeme: 27.568 ms
    Prosjecno vrijeme: 0.003 ms
    Propusnost: 362733.171 operacija/s

    SELECT:
    Broj operacija: 10000
    Uspjesno procitano: 10000
    Ukupno vrijeme: 13.503 ms
    Prosjecno vrijeme: 0.001 ms
    Propusnost: 740602.714 operacija/s

Rezultati predstavljaju performanse SQLite operacija nad bazom u
radnoj memoriji i ne predstavljaju ukupnu end-to-end propusnost
Smart Grid sistema.


## 2. PQC TLS i registracija

Datoteka:

    registration_benchmark.cpp

Ovaj benchmark mjeri vrijeme potrebno za uspostavljanje sigurne
komunikacije i registraciju Smart Metera na regionalni server.

Mjere se:

- TCP connect
- PQC TLS handshake
- REGISTER_REQ -> REGISTER_ACK
- ukupno vrijeme TCP + TLS + registracija

Za test se koristi Smart Meter:

    smartgrid://sarajevo/meter/001

Regionalni server:

    127.0.0.1:5001

Komunikacija koristi TLS 1.3 i post-kvantne mehanizme konfigurirane
u Smart Grid sistemu.

Kompajliranje:

    g++ tests/benchmark/registration_benchmark.cpp \
    -o tests/benchmark/registration_benchmark \
    -I/opt/openssl-3.5/include \
    -L/opt/openssl-3.5/lib64 \
    -Wl,-rpath,/opt/openssl-3.5/lib64 \
    -lboost_system -lssl -lcrypto -lpthread

Prije pokretanja benchmarka potrebno je pokrenuti centralni server:

    ./central/sync_server

i regionalni server Region 1:

    ./regional/server_async

Zatim:

    ./tests/benchmark/registration_benchmark

Dobijeni rezultat:

    TLS verzija: TLSv1.3
    Pregovorena grupa: X25519MLKEM768
    Potpis TLS handshake-a: mldsa44
    Cipher suite: TLS_AES_256_GCM_SHA384

    TCP connect: 0.108 ms
    PQC TLS handshake: 8.364 ms
    REGISTER_REQ -> REGISTER_ACK: 4.509 ms
    Ukupno TCP + TLS + registracija: 13.000 ms

    PASS

Prikazane vrijednosti predstavljaju jedno izvršavanje benchmarka i
ne predstavljaju statistički prosjek većeg broja ponavljanja.


## 3. CONSUMPTION_REPORT -> CONSUMPTION_ACK RTT

Datoteka:

    consumption_rtt_benchmark.cpp

Benchmark mjeri aplikacijski RTT (Round-Trip Time) od trenutka slanja
CONSUMPTION_REPORT poruke do prijema odgovarajuće
CONSUMPTION_ACK poruke.

Komunikacija se odvija preko postojeće PQC TLS veze.

Koriste se četiri mjerenja kako dodatna logika koja se u regionalnom
serveru izvršava nakon svakih pet regularnih CONSUMPTION_REPORT
poruka ne bi uticala na osnovno RTT mjerenje.

Kompajliranje:

    g++ tests/benchmark/consumption_rtt_benchmark.cpp \
    -o tests/benchmark/consumption_rtt_benchmark \
    -I/opt/openssl-3.5/include \
    -L/opt/openssl-3.5/lib64 \
    -Wl,-rpath,/opt/openssl-3.5/lib64 \
    -lboost_system -lssl -lcrypto -lpthread

Potrebno je pokrenuti:

    ./central/sync_server

zatim:

    ./regional/server_async

i nakon toga:

    ./tests/benchmark/consumption_rtt_benchmark

Dobijeni rezultat:

    Mjerenje #1 RTT: 3.416 ms
    Mjerenje #2 RTT: 2.829 ms
    Mjerenje #3 RTT: 2.725 ms
    Mjerenje #4 RTT: 4.783 ms

    Minimalni RTT: 2.725 ms
    Prosjecni RTT: 3.438 ms
    Maksimalni RTT: 4.783 ms

    PASS

Ovaj rezultat predstavlja aplikacijski odziv protokola, a ne ICMP
ping ili samo vrijeme prenosa kroz mrežu.


## 4. DATA_STREAM_SAMPLE benchmark

Datoteka:

    data_stream_benchmark.cpp

Ovaj benchmark mjeri propusnost slanja kontinuiranog toka
DATA_STREAM_SAMPLE poruka preko PQC TLS veze.

Test šalje:

    1000 DATA_STREAM_SAMPLE poruka

DATA_STREAM_SAMPLE poruke nemaju pojedinačni ACK. Zbog toga se nakon
završetka toka šalje regularni CONSUMPTION_REPORT.

Uspješan prijem CONSUMPTION_ACK poruke potvrđuje da je regionalni
server nakon obrade data-stream toka nastavio ispravno obrađivati
protokol.

Kompajliranje:

    g++ tests/benchmark/data_stream_benchmark.cpp \
    -o tests/benchmark/data_stream_benchmark \
    -I/opt/openssl-3.5/include \
    -L/opt/openssl-3.5/lib64 \
    -Wl,-rpath,/opt/openssl-3.5/lib64 \
    -lboost_system -lssl -lcrypto -lpthread

Potrebno je pokrenuti:

    ./central/sync_server

zatim:

    ./regional/server_async

i nakon toga:

    ./tests/benchmark/data_stream_benchmark

Dobijeni rezultat:

    Broj poslanih poruka: 1000
    Velicina jedne poruke: 96 B
    Ukupno poslano podataka: 96000 B
    Ukupno vrijeme slanja: 8.785 ms
    Prosjecno po poruci: 0.009 ms
    Propusnost: 113830.717 poruka/s
    Propusnost podataka: 87.422 Mbit/s

    CONSUMPTION_ACK nakon toka: USPJESAN
    PASS

Dobijena propusnost predstavlja sender-side throughput, odnosno
brzinu slanja DATA_STREAM_SAMPLE poruka sa strane Smart Meter
klijenta. Rezultat se ne interpretira kao maksimalna procesna
propusnost regionalnog servera.


## Zakljucak

Benchmark testovi pokrivaju performanse ključnih dijelova Smart Grid
sistema:

- pristup SQLite bazi podataka,
- uspostavljanje PQC TLS komunikacije i registraciju uređaja,
- odziv pri razmjeni mjerenja potrošnje,
- propusnost kontinuirane data-stream komunikacije.

Benchmark testovi su implementirani odvojeno od glavnih komponenti
sistema kako njihovo izvršavanje ne bi zahtijevalo izmjene postojeće
implementacije protokola.
