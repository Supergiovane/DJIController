# Protocollo DJI R SDK: riassunto

Riferimento: *DJI R SDK Protocol and User Interface v2.3* (gennaio 2021), integrato con le osservazioni di
[DJI-RS-Control](https://github.com/NewDayNaz/DJI-RS-Control).

## Trasporto CAN

| Parametro | Valore |
|---|---|
| Bitrate | 1 Mbit/s |
| Frame | standard (ID a 11 bit) |
| Controller → gimbal | `0x223` |
| Gimbal → controller | `0x222` |

Un pacchetto viene diviso in frame CAN consecutivi da 8 byte (l'ultimo può essere più corto) sullo stesso ID,
senza intercalare altri pacchetti. In ricezione si riassembla cercando `0xAA`, leggendo la lunghezza e
verificando i due CRC. Sul bus passa anche il traffico degli accessori (es. Focus Wheel su `0x530`/`0x531`/`0x426`),
quindi conviene filtrare in hardware solo `0x222`.

## Formato del pacchetto (little-endian)

| Offset | Dim. | Campo | Note |
|---|---|---|---|
| 0 | 1 | SOF | `0xAA` |
| 1 | 2 | Ver/Length | bit 9..0 = lunghezza totale, bit 15..10 = versione (0) |
| 3 | 1 | CmdType | bit 4..0: 0 = nessuna risposta, 1 = risposta facoltativa, 2–31 = risposta richiesta. Bit 5 = frame di risposta |
| 4 | 1 | ENC | 0 = non cifrato |
| 5 | 3 | RES | 0 |
| 8 | 2 | SEQ | numero di sequenza |
| 10 | 2 | CRC-16 | sui byte 0..9 |
| 12 | 1 | CmdSet | |
| 13 | 1 | CmdID | |
| 14 | n | Data | nelle risposte il primo byte è il return code |
| 14+n | 4 | CRC-32 | su tutto il pacchetto escluso il CRC stesso |

Lunghezza totale = 18 + n.

## CRC

| | Poly | Init | RefIn/RefOut | XorOut |
|---|---|---|---|---|
| CRC-16 | `0x8005` | `0xC55C` | sì | 0 |
| CRC-32 | `0x04C11DB7` | `0xC55C0000` | sì | 0 |

Con l'implementazione riflessa (LSB-first) i valori iniziali diventano entrambi `0x3AA3`.

Pacchetto di esempio ufficiale (posizione assoluta yaw 3,2°, roll 4,8°, pitch 6,4° in 2 s):

```
AA 1A 00 03 00 00 00 00 22 11 A2 42 0E 00 20 00 30 00 40 00 01 14 7B 40 97 BE
```

## Return code

| Valore | Significato |
|---|---|
| `0x00` | successo |
| `0x01` | errore di parsing |
| `0x02` | esecuzione fallita |
| `0xFF` | errore non definito |

## Comandi gimbal (CmdSet `0x0E`)

| CmdID | Funzione | Dati comando | Dati risposta |
|---|---|---|---|
| `0x00` | Posizione | yaw, roll, pitch `int16` (0,1°, ±1800); ctrl `u8` (bit 0: assoluto; bit 1/2/3: yaw/roll/pitch da ignorare); tempo `u8` (0,1 s) | ret |
| `0x01` | Velocità | yaw, roll, pitch `int16` (0,1°/s); ctrl `u8` (bit 7: presa di controllo; bit 3: ignora la focale) | ret |
| `0x02` | Leggi angoli | `0x01` assetto, `0x02` angoli dei giunti | ret, tipo (0 = non pronto), yaw, roll, pitch `int16` |
| `0x03` | Imposta limiti | ctrl `0x01`, poi pitch max/min, yaw max/min, roll max/min `u8` (0–179) | ret |
| `0x04` | Leggi limiti | `0x01` | ret + 6 × `u8` |
| `0x05` | Imposta rigidità | ctrl `0x01`, poi pitch, roll, yaw `u8` (0–100) | ret |
| `0x06` | Leggi rigidità | `0x01` | ret + 3 × `u8` |
| `0x07` | Push parametri | `0x01` attiva, `0x02` disattiva | ret |
| `0x08` | Push (dal gimbal) | flag `u8` (bit 0: angoli validi, bit 1: limiti, bit 2: rigidità), assetto e giunti 6 × `int16`, limiti 6 × `u8`, rigidità 3 × `u8` | nessuna |
| `0x09` | Versione modulo | device ID `u32` (`0x00000001` = R SDK) | ret, device ID, versione `0xAABBCCDD` = AA.BB.CC.DD |
| `0x0A` | Controllo esterno | joystick: `0x01`, pitch, roll, yaw `int16` (±15000). Dial: `0x02`, valore `int16` (±2048) | nessuna |
| `0x0B` / `0x0C` | Leggi / imposta parametri utente | formato TLV | |
| `0x0D` | Modalità operativa | `0xFE`, poi orizzontale/verticale `u8` | ret |
| `0x0E` | Recenter / selfie / follow | `FE 01` recenter, `FE 02` selfie; `00 00` lock, `02 00` yaw follow, `03 00` sport | ret |
| `0x0F` | Auto calibrazione | TLV | |
| `0x10` | Stato auto calibrazione (push) | | |
| `0x11` | ActiveTrack | `0x03` avvia/ferma | nessuna |
| `0x12` | Focus motor | posizione: `01 00 02` + `u16` (0–4095); calibrazione: `02 00` + sotto-comando | |

Il comando velocità (`0x01`) vale solo 0,5 s per sicurezza: per un movimento continuo va inviato di nuovo
periodicamente, e per fermarsi si mandano velocità nulle.

## Comandi camera (CmdSet `0x0D`)

| CmdID | Funzione | Dati |
|---|---|---|
| `0x00` | Controllo camera | `u16`: `1` scatto, `2` fine scatto, `3` avvia registrazione, `4` ferma registrazione, `5` messa a fuoco centrale, `0x0B` fine messa a fuoco |
| `0x01` | Stato camera | `0x01`; risposta: ret, stato (`0x00` non registra, `0x02` registra) |
