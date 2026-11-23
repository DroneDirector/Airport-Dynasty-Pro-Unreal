# Apron bus sounds: sources and how the game plays them

The bus is a battery-electric, low-floor airside bus. It has no engine, so every sound below belongs to an electric
bus, its air system or its doors.

The real recordings are all Creative Commons 0, from Freesound. They were checked on each sound's own page when each
was first used (the dates are in that folder's SOURCES.md), and they are already in the repo in those folders.
Our own synthesis is used only where the real thing is itself an electronic or plain-noise sound. Everything is cut
and mixed by `Tools/make_apron_bus_sounds.py` into mono 48 kHz WAVs.

## #region ── The files ──

**Driving**
- **ABDriveLoop.wav** (loop): the drive's rumble, tyres and body.
  - Source: "Troleybus (empty) in Valparaiso (Chile)" by felix.blume, https://freesound.org/people/felix.blume/sounds/502649/
    (CC0), a real electric trolleybus driving. These are the same 12 s as `Coach/CoachDrive.wav`, with the lowest end
    rolled off.
- **ABMotorWhineLoop.wav** (loop, at about 30 km/h): the traction motor's whine. It has three layers:
  - "ElectricMotor.wav" by derjuli, https://freesound.org/people/derjuli/sounds/824119/ (CC0), an electric motor's
    hum (its steadiest 8 s).
  - "Electric Scooter Loop.WAV" by pkri, https://freesound.org/people/pkri/sounds/587511/ (CC0), the motor's whine
    played at 0.55 speed, which brings it down to a big traction motor's range.
  - Our own motor "orders": the set of related tones (1, 1.5, 2, 3 and 4.5 times a 310 Hz base) that makes an electric
    bus sing as it speeds up.
- **ABRegenLoop.wav** (loop): braking on the motor (regeneration). It is the same three layers, softer and darker.
- **ABAVASLoop.wav / ABAVASReverseLoop.wav** (loops): the pedestrian warning sound (AVAS).
  - Every electric bus must have one under UN Regulation 138. It must sound at standstill-to-moving and up to
    20 km/h, include at least two one-third-octave bands with one at 1.6 kHz or below, and rise in pitch with speed.
    Real AVAS sounds are designed electronic tones, so this one is our own: a soft 220 Hz harmonic tone with a slight
    detune and a breath of band-passed noise.
  - Reverse is the same tone, pulsing.

**Powering up and down**
- **ABReady.wav**: the precharge and main contactors pulling in (two thunks), then the ready chime.
  - Thunks: "Contactor/motor overload relay noises" by tt_runscript, https://freesound.org/people/tt_runscript/sounds/807384/
    (CC0), at 21.8 s.
  - Chime: "Electric car start-up and drive away" by evsecrets, https://freesound.org/people/evsecrets/sounds/324119/
    (CC0), the ready beep at 13.8-14.7 s.
- **ABShutdown.wav**: the contactor dropping out (the same tt_runscript recording, at 34.3 s), with the whine winding
  down to nothing.

**Air system**

Electric buses keep compressed air for the brakes, the suspension, kneeling and the doors. An electric compressor
fills it.
- **ABCompressorStart.wav** and **ABCompressorLoop.wav**: the compressor's contactor, then its motor running.
  - Source: "hydraulics.wav" by nuckan, https://freesound.org/people/nuckan/sounds/212941/ (CC0), an electric pump
    motor's steady run, with a deeper copy under it for the body.
- **ABAirDryerPurge.wav**: the short "pssht" when the compressor stops and the air dryer blows out.
  - Source: "Hydraulic Truck Brakes" by Filmscore, https://freesound.org/people/Filmscore/sounds/823069/ (CC0), the
    peak of its air hiss.
- **ABBrakeRelease.wav**: the park brake letting go. **ABStopBrakeApply.wav**: the bus-stop brake that holds the bus
  while its doors are open. Both come from the same Filmscore recording.
- **ABKneelDown.wav**: air let out of the bellows; the body sinks 80 mm. **ABKneelUp.wav**: the bellows refilling,
  softer. Both come from the same Filmscore recording.

**Doors**
- **ABDoorOpen.wav**: "busdoor.mp3" by BabelFishTank, https://freesound.org/people/BabelFishTank/sounds/534977/ (CC0),
  a real bus door opening (the same cut as `Coach/CoachDoor.wav`).
- **ABDoorClose.wav**: the same recording slightly faster, as the leaves swing back in. The thud of the rubber edges
  meeting is our own: a 70 Hz knock with a little low noise.
- **ABDoorChime.wav**: the warning before the doors close, three 1.6 kHz beeps. It is electronic on real buses, so it
  is our own.

**The rest**
- **ABACLoop.wav** (loop): the roof A/C fans. This is our own band-limited noise with the fan blades' hum.
- **ABIndicatorLoop.wav** (loop, 0.8 s): the flasher relay's tick and tock, 75 flashes a minute. It is our own.
- **ABReverseBeep.wav** (loop): "Truck Reverse Beeper.wav" by Disasteradio, https://freesound.org/people/Disasteradio/sounds/197166/
  (CC0), one beep and its gap.
- **ABHorn.wav**: "Maquina de vapor .wav" by Camilo_Vargas13, https://freesound.org/people/Camilo_Vargas13/sounds/429834/
  (CC0), a heavy vehicle's horn, one blast.

## #endregion

## #region ── How the game plays them (MetaSounds; to wire up in Unreal) ──

**The reference.** The owner chose the 40-second demo (2026-11-23: "perfect, use that one"). It is
`ApronBus-SoundDemo.wav`, made by `Tools/make_apron_bus_sound_demo.py`, and its mix code holds the exact gains and
curves listed below. The game must sound like it.


**Placement.** Every sound sits at its source on the bus:
- Drive and whine: at the rear axle's motor.
- Compressor and dryer: under the rear right floor (the motor compartment).
- AVAS: at the front bumper.
- Doors and chime: at each door.
- A/C: on the roof.
- Indicator: in the cab.
- Beeper: at the rear.

Each one is heard from where you stand, with the same distance and occlusion treatment as the jetway sounds. Inside
the bus, the drive is quieter and duller, and the A/C, chime and indicator are louder.

**Speed, v in km/h.** Volumes below are relative; the master is in the sound settings.
- **Drive loop:** volume 0 at standstill, full at 20; pitch 0.85 at 0 rising to 1.15 at 40.
- **Motor whine:** pitch = v / 30 (from 0.1 up to 1.4).
  - Volume follows the accelerator: about 0.3 when coasting, full under hard pull, and a little more when climbing
    or loaded.
  - It eases off smoothly on lifting the accelerator; there are no gear changes.
- **Regeneration:** while braking on the motor above 4 km/h, pitch = v / 30 and volume follows how hard it brakes.
  Below 4 km/h it fades as the friction brakes take over.
- **AVAS:**
  - Forward: on from moving off up to 20 km/h, fading out by 25 km/h.
  - Pitch = 1 + 0.008 v (the 0.8 % per km/h shift UN R138 asks for).
  - Reverse uses the pulsing loop together with the reverse beeper.
  - It is off while the bus stands with its doors open.

**Air pressure.** It is an air tank number from 0 to 10.5 bar.
- **Using air:**
  - Each door cycle uses about 0.08 bar.
  - Each kneel uses about 0.25 bar.
  - Each brake application uses 0.03-0.1 bar.
- **Compressor:** cuts in at 8.5 bar and out at 10.0 bar. These are typical bus values, and they are tunable.
  - When it cuts in: ABCompressorStart, then the loop.
  - When it cuts out: the loop stops, then ABAirDryerPurge plays.
- **Park brake:** releasing it plays ABBrakeRelease.

**At the stands.** This is the order of events:
1. The bus stops, applies the bus-stop brake (ABStopBrakeApply) and kneels (ABKneelDown).
2. The doors open (ABDoorOpen).
3. When boarding ends, the chime plays (ABDoorChime), then the doors close (ABDoorClose).
4. The bus rises (ABKneelUp) and the brake releases (ABBrakeRelease).

**Power.** ABReady plays when the driver switches on, and ABShutdown when the bus parks for good. Between them, at
standstill with the doors shut, all you hear is the A/C, the AVAS and the compressor now and then. There is no
engine idle.

## #endregion
