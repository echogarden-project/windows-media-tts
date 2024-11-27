import { writeFile } from 'node:fs/promises'
import { getVoiceList, isAddonAvailable, synthesize } from './Exports.js'

const available = isAddonAvailable()
//console.log(available)

if (!available) {
	process.exit(1)
}

const voices = getVoiceList()

//console.log(voices)

const text = `Hello World`

const ssmlText = `
Hello World! <mark name="a" />How are you <mark name="b" />doing today?
`

const { audioData, markers, timedMetadataTracks } = synthesize(ssmlText, {
	voiceName: 'Microsoft David',
	speakingRate: 1.0,
	audioPitch: 1.0,
	enableSsml: true
})

console.log(JSON.stringify(markers, undefined, 4))

console.log(JSON.stringify(timedMetadataTracks, undefined, 4))

await writeFile(`out/out.wav`, audioData)

const x = 0

