# Node.js binding to the Windows Media Speech Synthesis API

Uses N-API to bind to the Universal Windows Platform speech-synthesis API (`Windows.Media.SpeechSynthesis`). The native speech synthesizer on Windows 10 and later.

* Speech is returned as a `Uint8Array`, in WAVE format
* Will recognize voice packages installed via the Windows Speech settings
* Supports SSML (when option `enableSsml` is set to `true`)
* Provides markers and metadata tracks
* Addon binaries are pre-bundled. Doesn't require any install-time postprocessing
* Supports Windows 10 and newer, on both x64 and arm64

## Installation

`npm install @echogarden/window-media-tts`

## Usage examples

### List voices:
```ts
import { getVoiceList, synthesize } from '@echogarden/windows-media-tts'

const voices = getVoiceList()

console.log(voices)
```
Prints:

```ts
[
  {
    id: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices\\Tokens\\MSTTS_V110_enUS_DavidM',
    displayName: 'Microsoft David',
    description: 'Microsoft David - English (United States)',
    language: 'en-US',
    gender: 'male'
  },
  {
    id: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices\\Tokens\\MSTTS_V110_enUS_ZiraM',
    displayName: 'Microsoft Zira',
    description: 'Microsoft Zira - English (United States)',
    language: 'en-US',
    gender: 'female'
  },

  //...
]
```

### Synthesize text

```ts
import { writeFile } from 'node:fs/promises'
import { synthesize } from '@echogarden/window-media-tts'

const text = `
Hello World! How are you doing today?
`

const { audioData } = synthesize(text, {
	voiceName: 'Microsoft Zira',
	speakingRate: 0.9,
	audioPitch: 1.0,
	enableSsml: false
})

await writeFile(`hello-world.wav`, audioData)
```

The `voiceName` property will match any of `id`, `displayName` or `description` properties of the target voice.

### Synthesize SSML

```ts
const ssml = `
Hello World! <mark name="a" />How are you <mark name="b" />doing today?
`

const { audioData, markers } = synthesize(ssml, {
	voiceName: 'Microsoft David',
	speakingRate: 0.8,
	audioPitch: 1.0,
	enableSsml: true
})

// ...

console.log(markers)
```

Markers show as:
```ts
[ { name: 'a', time: 1.6964375 }, { name: 'b', time: 2.2614375 } ]
```

**Notes**:
* The wrapping SSML `<speak>` tag is automatically added, and includes the language name for the selected voice (or for the default one, if not selected). That is crucial for the synthesis engine to successfully accept the input
* The Windows synthesis engine is very strict about the validity of the SSML markup. If the SSML has errors, like tags that aren't properly closed, it will fail without a helpful error message

### Timed metadata

The Windows synthesis API returns the start time of each sentence and word, in a [`TimedMetadataTracks`](https://learn.microsoft.com/en-us/uwp/api/windows.media.speechsynthesis.speechsynthesisstream.timedmetadatatracks?view=winrt-26100#windows-media-speechsynthesis-speechsynthesisstream-timedmetadatatracks) object.

This object is converted to a JavaScript object and returned in the `timedMetadataTracks` property of the returned object.

```ts
const { audioData, timedMetadataTracks } = synthesize(...)
```

**Note**: it seems to give correct timing. However, the cue identifiers seem to be empty, cue durations are 0, and it contains no text offset information. This makes it harder to reliably match the cues to the words and sentences in the text, in practice.

## Building the N-API addons

The library is bundled with pre-built addons, so recompilation shouldn't be needed.

If you still want to compile yourself, for a modification or a fork:

* Install Visual Studio 2022 build tools
* In the `addons` directory, run `npm install`, which would install the necessary build tools. Then run `npm run build-x64`
* To cross-compile for arm64 go to `Visual Studio Installer -> Individual components`, and ensure `MSVC v143 - VS 2022 C++ ARM64 build tools (latest)` is checked. Then run `npm run build-arm64`
* Resulting binaries should be written to the `addons/bin` directory

## License

MIT
