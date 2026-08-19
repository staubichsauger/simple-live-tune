# Simple Live Tune

Simple Live Tune is a free pitch-correction plug-in for live vocals. It moves
sung notes toward the selected key and scale without delaying the audio path.

It is available for macOS as an Audio Unit (AU), VST3 plug-in, and standalone
app.

## Download

Download the latest version from the
[GitHub releases page](https://github.com/staubichsauger/simple-live-tune/releases/latest).

Choose the macOS universal ZIP file. It works on Apple Silicon and Intel Macs
running macOS 11 or later.

## Install

1. Quit your DAW or live audio host.
2. Unzip the downloaded file.
3. Install the format your host supports:
   - Copy the AU file to `~/Library/Audio/Plug-Ins/Components/`.
   - Copy the VST3 file to `~/Library/Audio/Plug-Ins/VST3/`.
4. Fully reopen your host and rescan its plug-ins if necessary.

You can install both formats. To test a microphone without a DAW, copy the
included Simple Live Tune standalone app to your Applications folder.

The download includes a more detailed installation guide.

### If macOS blocks the app or plug-in

The release is not yet notarized by Apple. Only continue if you trust the
download from this repository.

Open System Settings > Privacy & Security, scroll to the Security section, and
choose Open Anyway. You can also remove the quarantine flag from the
unzipped release using Terminal:

```sh
xattr -dr com.apple.quarantine "/path/to/Simple-Live-Tune"
```

Replace the example path with the folder you unzipped.

## Get started

1. Add Simple Live Tune as an effect on a vocal track.
2. Select the song's Key and choose Major, Minor, or Chromatic.
3. Start with the default settings and sing into the microphone.
4. Lower Correction Amount or increase Retune Speed if the correction sounds
   too strong.

Keep monitoring volume low during your first live test to avoid feedback.

## Controls

- Key and Scale choose the allowed notes.
- Retune Speed sets how quickly the plug-in corrects a note. A low value reacts
  quickly and sounds more obvious. A high value allows slower pitch movement.
- Note Transition sets how quickly correction moves to a new note.
- Correction Amount mixes the corrected vocal with the original vocal.
- Tune Tolerance leaves small pitch differences untouched.
- Vibrato preserves more of the singer's pitch movement.
- Confidence Threshold and Gate prevent breaths, background noise,
  and unclear notes from being tuned.
- Bypass turns correction off without removing the plug-in.

The display shows the detected note, how far it is from the target, and the
original and corrected pitch over time.

## Troubleshooting

### The plug-in does not appear

Make sure you copied it to the correct folder, fully quit and reopened the
host, and rescanned the plug-in list. Check whether your host uses AU or VST3.

### The standalone app receives no microphone signal

Allow microphone access in System Settings > Privacy & Security > Microphone,
then check the audio input selected in the app.

### The correction sounds too strong

Check the key and scale first. Then increase Retune Speed or Note Transition,
or lower Correction Amount.

### The plug-in tunes breaths or background noise

Raise Gate or Confidence Threshold until the unwanted correction stops.

## Technical information

Read the [architecture and DSP notes](ARCHITECTURE.md) for implementation
details. Building from source requires CMake 3.20+ and a C++20 compiler. CMake
downloads JUCE during setup.

## Acknowledgements

Simple Live Tune's approach is inspired by
[Autotalent](http://web.mit.edu/tbaran/www/autotalent.html) by Tom Baran and
[zita-at1](https://kokkinizita.linuxaudio.org/linuxaudio/) by Fons Adriaensen.
It is an independent clean-room implementation and includes no code from those
projects.

It uses [JUCE](https://juce.com), the
[VST3 SDK](https://github.com/steinbergmedia/vst3sdk), and Apple's Audio Unit
SDK. VST is a registered trademark of Steinberg Media Technologies GmbH.

## License

Simple Live Tune is free software licensed under the
[GNU General Public License v3.0 or later](LICENSE).
