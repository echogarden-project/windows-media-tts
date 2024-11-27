import { createRequire } from 'node:module'

export function synthesize(text: string, options: WindowsMediaTTSOptions) {
	const addon = getAddonForCurrentPlatform()

	options = { ...defaultWindowsMediaTTSOptions, ...options }

	if (options.enableSsml) {
		let voiceLanguage: string

		if (options.voiceName) {
			const selectedVoiceInfo = getVoiceInfo(options.voiceName)

			if (selectedVoiceInfo) {
				voiceLanguage = selectedVoiceInfo.language
			} else {
				voiceLanguage = getDefaultVoiceInfo().language
			}
		} else {
			voiceLanguage = getDefaultVoiceInfo().language
		}

		text = `
<speak version="1.0" xmlns="http://www.w3.org/2001/10/synthesis" xml:lang="${voiceLanguage}">
${text}
</speak>
`
	}

	const result: WindowsMediaTTSSynthesizeResult = addon.synthesize(text, options)

	return result
}

export function getVoiceList(): WindowsMediaTTSVoiceInfo[] {
	const addon = getAddonForCurrentPlatform()

	const voiceList: WindowsMediaTTSVoiceInfo[] = addon.getVoiceList()

	return voiceList
}

export function getVoiceInfo(name: string): WindowsMediaTTSVoiceInfo {
	const addon = getAddonForCurrentPlatform()

	return addon.getVoiceInfo(name)
}

export function getDefaultVoiceInfo(): WindowsMediaTTSVoiceInfo {
	const addon = getAddonForCurrentPlatform()

	return addon.getDefaultVoiceInfo()
}

export function isAddonAvailable() {
	try {
		const addon = getAddonForCurrentPlatform()

		const result = addon.isAddonLoaded()

		return result === true
	} catch (e) {
		return false
	}
}

function getAddonForCurrentPlatform() {
	const platform = process.platform
	const arch = process.arch

	const require = createRequire(import.meta.url)

	let addonModule: any

	if (platform === 'win32' && arch === 'x64') {
		addonModule = require('../addons/bin/windows-x64-tts.node')
	} else if (platform === 'win32' && arch === 'arm64') {
		addonModule = require('../addons/bin/windows-arm64-tts.node')
	} else {
		throw new Error(`windows-media-tts initialization error: platform ${platform}, ${arch} is not supported`)
	}

	return addonModule
}

export interface WindowsMediaTTSSynthesizeResult {
	audioContentType: string
	audioData: Uint8Array
	markers: { text: string; time: number }
	
	timedMetadataTracks: {
		id: string
		cues: {
			id: string
			startTime: number
			duration: number
		}[]
	}
}

export interface WindowsMediaTTSVoiceInfo {
	id: string
	displayName: string
	description: string
	language: string
	gender: 'male' | 'female'
}

export interface WindowsMediaTTSOptions {
	voiceName?: string
	speakingRate?: number
	audioPitch?: number
	enableSsml?: boolean
	enableTrace?: boolean
}

export const defaultWindowsMediaTTSOptions: WindowsMediaTTSOptions = {
	voiceName: '',
	speakingRate: 1.0,
	audioPitch: 1.0,
	enableSsml: false,
	enableTrace: false,
}
