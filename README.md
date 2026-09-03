# Protokol za upravljanje pametnim energetskim mrežama - Smart Grid

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
