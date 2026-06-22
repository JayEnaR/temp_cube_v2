# SIMCom A76XX Audio & Voice Services Expert

## Description

Expert guidance for implementing audio features on SIMCom A76XX-series LTE modems.

Supported capabilities:

* Text-to-Speech (TTS)
* Audio playback
* WAV playback
* AMR playback
* Local audio output
* Remote audio injection during calls
* Voice announcements
* Voice call prompts
* Call recording
* Local recording
* Remote call recording

Applies to:

* A7670
* A7600 Series
* A76XX family modules

Use this skill whenever generating firmware that interacts with modem audio services.

---

# Audio Architecture

The A76XX modem contains an internal audio engine.

Audio operations are controlled using AT commands over UART or USB.

The modem can:

* Generate speech from text
* Play audio files
* Inject audio into active calls
* Record local audio
* Record remote audio
* Save generated speech as WAV files

Supported playback formats:

* WAV
* AMR

Recorded files are stored as:

* WAV

---

# Core Audio Commands

## Text To Speech

### Configure TTS

```text
AT+CTTSPARAM=<volume>,<sysvolume>,<digitmode>,<pitch>,<speed>
```

Example:

```text
AT+CTTSPARAM=1,3,0,1,1
```

Parameters:

| Parameter | Purpose             |
| --------- | ------------------- |
| volume    | TTS volume          |
| sysvolume | System volume       |
| digitmode | Digit pronunciation |
| pitch     | Voice pitch         |
| speed     | Speech rate         |

---

## Speak ASCII Text

```text
AT+CTTS=2,"Hello World"
```

Expected:

```text
OK
+CTTS: 0
```

---

## Speak UCS2 Text

```text
AT+CTTS=1,"<UCS2_STRING>"
```

Used for multilingual speech generation.

---

## Stop TTS Playback

```text
AT+CTTS=0
```

Use whenever playback must be interrupted.

---

# Save TTS To WAV

Generate speech and save to storage.

Example:

```text
AT+CTTS=3,"Alarm activated","C:/alarm"
```

Output:

```text
C:/alarm.wav
```

Useful for:

* Pre-generated prompts
* IVR systems
* Cached announcements
* Voice alerts

---

# Remote TTS Playback

Play generated speech to the remote party during a voice call.

## Configure Remote Output

```text
AT+CDTAM=1
```

## Establish Call

```text
ATD<number>;
```

## Hang up Call

AT+CHUP

## Play Speech

```text
AT+CTTS=2,"Your alarm has been triggered"
```

Speech is transmitted to the remote caller.

---

# Audio File Playback

## Local Playback

Play AMR file:

```text
AT+CCMXPLAY="C:/sound.amr",0,1
```

Play WAV file:

```text
AT+CCMXPLAY="C:/recording.wav",0,1
```

Parameters:

```text
AT+CCMXPLAY="<file>",<destination>,<repeat>
```

Destination:

| Value | Output      |
| ----- | ----------- |
| 0     | Local       |
| 1     | Remote call |

Repeat:

| Value | Meaning        |
| ----- | -------------- |
| 0     | Infinite       |
| N     | Repeat N times |

---

# Remote Audio Injection

Inject an audio file into an active voice call.

## Place Call

```text
ATD<number>;
```

## Play Audio

```text
AT+CCMXPLAY="C:/announcement.amr",1,0
```

Common uses:

* IVR systems
* Automated notifications
* Alarm calls
* Outbound alerting

---

# Stop Playback

```text
AT+CCMXSTOP
```

Expected:

```text
+AUDIOSTATE: audio play stop
```

Always provide a method to stop playback.

---

# Recording

## Local Recording

Record audio locally.

```text
AT+CREC=1,"C:/recording.wav"
```

Expected:

```text
+CREC: 1
```

Recording ends automatically when storage is full.

---

## Stop Recording

```text
AT+CREC=0
```

Use before ending a session.

---

# Remote Call Recording

Record audio from an active call.

## Place Call

```text
ATD<number>;
```

## Start Recording

```text
AT+CREC=2,"C:/call.wav"
```

Expected:

```text
+CREC: 2
```

Output:

```text
C:/call.wav
```

Use for:

* Call logging
* Evidence capture
* Support systems
* Telemetry voice archives

---

# ESP32 Integration Patterns

## Audio Manager State Machine

Recommended states:

```cpp
IDLE
TTS_PLAYING
FILE_PLAYING
RECORDING
CALL_RECORDING
STOPPING
ERROR
```

Never block while waiting for audio completion.

Instead monitor URCs:

```text
+CTTS: 0
+AUDIOSTATE: audio play
+AUDIOSTATE: audio play stop
+CREC
```

---

# Common Applications

## Voice Alarm System

Workflow:

1. Event occurs
2. Dial contact
3. Wait for answer
4. Play TTS alert
5. Hang up

Example:

```text
ATD+27123456789;
AT+CTTS=2,"Motion detected at Site A"
ATH
```

---

## IVR System

Workflow:

1. Receive call
2. Answer
3. Play menu audio
4. Process DTMF
5. Route call

Use:

```text
AT+CCMXPLAY
```

for menu prompts.

---

## Voice Notification Device

Workflow:

1. Sensor event
2. Generate speech
3. Play locally
4. Optionally call operator
5. Play remotely

---

## Call Recorder

Workflow:

1. Detect call connected
2. Start recording

```text
AT+CREC=2,"C:/call.wav"
```

3. Stop recording on hangup

```text
AT+CREC=0
```

4. Upload recording

---

# Error Handling

## Generic Audio Failure

If modem returns:

```text
ERROR
```

Check:

* Firmware supports command
* File exists
* Storage available
* Call state valid
* Parameters valid

---

## Error Codes

| Code | Meaning              |
| ---- | -------------------- |
| 0    | Success              |
| 2    | Unknown error        |
| 3    | Busy                 |
| 7    | File or memory error |
| 8    | Invalid parameter    |
| 9    | Operation rejected   |
| 11   | State error          |
| 17   | File error           |

---

# Best Practices

Always:

* Wait for OK responses.
* Monitor audio URCs.
* Verify files exist before playback.
* Stop playback before starting another.
* Stop recording before shutdown.
* Use WAV for compatibility.
* Use AMR when storage size matters.
* Use state machines instead of delays.
* Log all AT commands and responses.

For ESP32 projects, implement audio operations asynchronously and treat the modem as an independent audio subsystem rather than attempting to manage timing with delays.
