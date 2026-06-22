# A76XX Series AT Commands Skill

## Purpose
This skill helps an AI assistant work effectively with SIMCom A76XX-series cellular modules using the A76XX Series AT Command Manual v1.10.

## When to Use
Use this skill whenever working with:
- SIMCom A7600, A7670, A7672, A7678, and related A76XX modules
- ESP-IDF integrations
- LTE Cat-1 / Cat-4 modem development
- AT command troubleshooting
- Cellular networking
- SMS, voice, GNSS, MQTT, HTTP, FTP, SSL, WebSocket, BLE, Wi-Fi, and FOTA features

## Core Principles
1. Prefer official A76XX AT commands over assumptions.
2. Always verify command syntax, parameter ranges, and URCs.
3. Distinguish between:
   - Test commands (`=?`)
   - Read commands (`?`)
   - Write commands (`=`)
   - Execution commands
4. Consider unsolicited result codes (URCs) when designing firmware.
5. Provide complete command sequences rather than isolated commands.

## Major Command Groups

### V.25TER Basics
- AT
- ATE
- ATI
- AT&W
- ATZ
- AT+CGMI
- AT+CGMM
- AT+CGMR
- AT+CGSN

### Status Control
- AT+CFUN
- AT+CSQ
- AT+AUTOCSQ
- AT+CCLK
- AT+CMEE

### Network Registration
- AT+CREG
- AT+CGREG
- AT+CEREG
- AT+COPS
- AT+CPSI
- AT+CNMP

### PDP & Data Connectivity
- AT+CGATT
- AT+CGACT
- AT+CGDCONT
- AT+CGPADDR
- AT+CGAUTH

### SIM Management
- AT+CPIN
- AT+CLCK
- AT+CPWD
- AT+CRSM
- AT+SWITCHSIM
- AT+DUALSIM

### Voice Calls
- ATD
- ATA
- ATH
- AT+CLCC
- AT+CLIP
- AT+COLP
- AT+CHLD

### SMS
- AT+CMGF
- AT+CMGS
- AT+CMGR
- AT+CMGL
- AT+CMGD
- AT+CSMP
- AT+CNMI

### TCP/IP
- AT+NETOPEN
- AT+NETCLOSE
- AT+CIPOPEN
- AT+CIPSEND
- AT+CIPRXGET
- AT+CIPCLOSE
- AT+CDNSGIP

### HTTP/HTTPS
- AT+HTTPINIT
- AT+HTTPPARA
- AT+HTTPACTION
- AT+HTTPREAD
- AT+HTTPPOSTFILE

### FTP/FTPS
- AT+CFTPSLOGIN
- AT+CFTPSGET
- AT+CFTPSPUT
- AT+CFTPSGETFILE
- AT+CFTPSPUTFILE

### MQTT
- AT+CMQTTSTART
- AT+CMQTTACCQ
- AT+CMQTTCONNECT
- AT+CMQTTPUB
- AT+CMQTTSUB
- AT+CMQTTDISC

### SSL/TLS
- AT+CSSLCFG
- AT+CCERTDOWN
- AT+CCHOPEN
- AT+CCHSEND
- AT+CCHRECV

### File System
- AT+FSOPEN
- AT+FSREAD
- AT+FSWRITE
- AT+FSCLOSE
- AT+FSLS
- AT+FSMEM
- AT+FSCD

### Audio / TTS
- AT+CCMXPLAY
- AT+CCMXSTOP
- AT+CREC
- AT+CTTS

### GNSS
- AT+CGNSSPWR
- AT+CGNSSINFO
- AT+CGPSINFO
- AT+CGNSSNMEA

### FOTA
- AT+CFOTA
- AT+LFOTA

### WebSocket
- WebSocket AT command chapter

### BLE & Bluetooth
- BLE server/client commands
- Bluetooth SPP commands

## Expected Response Style
When answering questions:
1. Explain the purpose of the command.
2. Show syntax.
3. Provide a complete example session.
4. Mention important URCs.
5. Include common failure causes.
6. Highlight ESP-IDF integration considerations when relevant.

## Troubleshooting Checklist
- SIM detected?
- Registered on network?
- PDP context configured?
- PDP context activated?
- Socket open?
- SSL configured?
- APN correct?
- Signal quality acceptable?
- DNS resolution successful?
- URCs enabled and monitored?

## Common ESP-IDF Workflow
1. Power modem.
2. Wait for boot URCs.
3. Verify SIM with `AT+CPIN?`.
4. Verify registration with `AT+CEREG?`.
5. Configure APN using `AT+CGDCONT`.
6. Open data service.
7. Establish protocol connection (TCP/MQTT/HTTP/etc.).
8. Monitor URCs continuously.
9. Gracefully close connections before power down.
