#include <string>
#include <vector>
#include <codecvt>
#include <sstream>
#include <iostream>

#include <windows.h>

#include <winrt/Windows.Media.SpeechSynthesis.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Core.h>

#include <napi.h>

using namespace winrt;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;

///////////////////////////////////////////////////////////////////////////////////////////
// Utility methods
///////////////////////////////////////////////////////////////////////////////////////////
std::string wStringToUTF8(const std::wstring& wstring) {
	if (wstring.length() == 0) {
		return std::string();
	}

	int capacity = (wstring.length() + 1) * sizeof(char) * 4;
	char* charBuffer = new char[capacity];

	int charLength = WideCharToMultiByte(CP_UTF8, 0, wstring.c_str(), wstring.length(), charBuffer, capacity, nullptr, nullptr);
	charBuffer[charLength] = 0;

	std::string resultString;
	resultString.assign(charBuffer);

	return resultString;
}

std::wstring utf8ToWString(const std::string& string) {
	if (string.length() == 0) {
		return std::wstring();
	}

	int capacity = (string.length() + 1) * 2;
	wchar_t* wcharBuffer = new wchar_t[capacity];

	int wcharLength = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), strlen(string.c_str()), wcharBuffer, capacity);
	wcharBuffer[wcharLength] = 0;

	std::wstring resultWString;
	resultWString.assign(wcharBuffer);

	return resultWString;
}

std::wstring napiStringToWString(const Napi::String& napiString) {
	return utf8ToWString(napiString.Utf8Value());
}

winrt::hstring napiStringToHString(const Napi::String& napiString) {
	winrt::hstring winrtStr(napiStringToWString(napiString));

	return winrtStr;
}

Napi::String hStringToNapiString(Napi::Env env, const winrt::hstring& hstr) {
    return Napi::String::New(env, wStringToUTF8(std::wstring(hstr)));
}

///////////////////////////////////////////////////////////////////////////////////////////
// Windows Media Speech Synthesis utilities
///////////////////////////////////////////////////////////////////////////////////////////
VoiceInformation getVoiceInformation(const winrt::hstring& name) {
	auto voices = SpeechSynthesizer::AllVoices();
	auto size = voices.Size();

	for (uint32_t i = 0; i < size; i++) {
		auto voiceInformation = voices.GetAt(i);

		if (voiceInformation.Id() == name || voiceInformation.DisplayName() == name || voiceInformation.Description() == name) {
			return voiceInformation;
		}
	}

	return nullptr;
}

Napi::Object voiceInformationToNapiObject(const Napi::Env& env, VoiceInformation voice) {
	auto voiceNapiObject = Napi::Object::New(env);

	voiceNapiObject.Set("id", hStringToNapiString(env, voice.Id()));
	voiceNapiObject.Set("displayName", hStringToNapiString(env, voice.DisplayName()));
	voiceNapiObject.Set("description", hStringToNapiString(env, voice.Description()));
	voiceNapiObject.Set("language", hStringToNapiString(env, voice.Language()));
	voiceNapiObject.Set("gender", static_cast<bool>(voice.Gender()) == 0 ? "male" : "female");

	return voiceNapiObject;
}

///////////////////////////////////////////////////////////////////////////////////////////
// N-API wrapper methods
///////////////////////////////////////////////////////////////////////////////////////////
Napi::Object synthesize(const Napi::CallbackInfo& info) {
	auto env = info.Env();

	auto text = napiStringToHString(info[0].As<Napi::String>());

	// Options are not checked here. They are assumed to be pre-checked in JavaScript.
	// They must be all correctly given.
	auto options = info[1].As<Napi::Object>();

	auto voiceName = napiStringToHString(options.Get("voiceName").As<Napi::String>());
	auto speakingRate = options.Get("speakingRate").As<Napi::Number>().DoubleValue();
	auto audioPitch = options.Get("audioPitch").As<Napi::Number>().DoubleValue();
	auto enableSsml = options.Get("enableSsml").As<Napi::Boolean>().Value();
	auto enableTrace = options.Get("enableTrace").As<Napi::Boolean>().Value();

	SpeechSynthesizer synth;

	synth.Options().SpeakingRate(speakingRate);
	synth.Options().AudioPitch(audioPitch);
	synth.Options().IncludeSentenceBoundaryMetadata(true);
	synth.Options().IncludeWordBoundaryMetadata(true);

	auto voiceInfo = getVoiceInformation(voiceName);

	if (voiceInfo) {
		synth.Voice(voiceInfo);
	}

	auto synthesisOperation = enableSsml? synth.SynthesizeSsmlToStreamAsync(text) : synth.SynthesizeTextToStreamAsync(text);

	if (enableTrace) {
		std::cout << "Getting speech stream.." << std::endl;
	}

	auto speechStream = synthesisOperation.get();

	if (enableTrace) {
		std::cout << "Succeeded getting speech stream." << std::endl;
	}

	// Get the audio data from the stream
	auto inputStream = speechStream.GetInputStreamAt(0);

	Buffer buffer(speechStream.Size());

	auto readOperation = inputStream.ReadAsync(buffer, speechStream.Size(), InputStreamOptions::None);
	readOperation.get();

	// Copy the audio data to the vector
	std::vector<uint8_t> audioData;

	auto dataReader = DataReader::FromBuffer(buffer);
	audioData.resize(buffer.Length());
	dataReader.ReadBytes(audioData);

	auto audioDataUint8Array = Napi::Uint8Array::New(env, audioData.size());
	std::memcpy(audioDataUint8Array.Data(), audioData.data(), audioData.size());

	// Get markers
	auto markers = speechStream.Markers();

	auto napiMarkerList = Napi::Array::New(env, markers.Size());

	for (uint32_t markerIndex = 0; markerIndex < markers.Size(); markerIndex++) {
		auto marker = markers.GetAt(markerIndex);

		auto markerNapiObj = Napi::Object::New(env);

		auto timeSeconds = static_cast<double>(marker.Time().count()) / 10000000.0;

		markerNapiObj.Set("type", hStringToNapiString(env, marker.MediaMarkerType()));
		markerNapiObj.Set("name", hStringToNapiString(env, marker.Text()));
		markerNapiObj.Set("time", Napi::Number().From(env, timeSeconds));

		napiMarkerList.Set(markerIndex, markerNapiObj);
	}

	auto tracks = speechStream.TimedMetadataTracks();
	auto napiTracksList = Napi::Array::New(env, tracks.Size());

	for (uint32_t trackIndex = 0; trackIndex < tracks.Size(); trackIndex++) {
		auto track = tracks.GetAt(trackIndex);

		auto cues = track.Cues();
		auto napiCueList = Napi::Array::New(env, cues.Size());

		for (uint32_t cueIndex = 0; cueIndex < cues.Size(); cueIndex++) {
			auto cue = cues.GetAt(cueIndex);

			auto napiCueObject = Napi::Object::New(env);

			auto startTimeSeconds = static_cast<double>(cue.StartTime().count()) / 10000000.0;
			auto durationSeconds = static_cast<double>(cue.Duration().count()) / 10000000.0;

			napiCueObject.Set("id", hStringToNapiString(env, cue.Id()));
			napiCueObject.Set("startTime", Napi::Number::From(env, startTimeSeconds));
			napiCueObject.Set("duration", Napi::Number::From(env, durationSeconds));

			napiCueList.Set(cueIndex, napiCueObject);
		}

		auto napiTrackObject = Napi::Object::New(env);
		napiTrackObject.Set("id", hStringToNapiString(env, track.Id()));
		napiTrackObject.Set("cues", napiCueList);

		napiTracksList.Set(trackIndex, napiTrackObject);
	}

	//

	auto returnObj = Napi::Object::New(env);

	returnObj.Set("audioContentType", hStringToNapiString(env, speechStream.ContentType()));
	returnObj.Set("audioData", audioDataUint8Array);
	returnObj.Set("markers", napiMarkerList);
	returnObj.Set("timedMetadataTracks", napiTracksList);

	return returnObj;
}

Napi::Array getVoiceList(const Napi::CallbackInfo& info) {
	auto env = info.Env();

    auto voices = SpeechSynthesizer::AllVoices();

	auto resultNapiArray = Napi::Array::New(env, voices.Size());

    for (uint32_t i = 0; i < voices.Size(); ++i) {
		auto voice = voices.GetAt(i);

		resultNapiArray.Set(i, voiceInformationToNapiObject(env, voice));
    }

	return resultNapiArray;
}

Napi::Object getDefaultVoiceInfo(const Napi::CallbackInfo& info) {
	auto defaultVoice = SpeechSynthesizer::DefaultVoice();

	return voiceInformationToNapiObject(info.Env(), defaultVoice);
}

Napi::Value getVoiceInfo(const Napi::CallbackInfo& info) {
	auto env = info.Env();

	auto voiceName = napiStringToHString(info[0].As<Napi::String>());

	auto voiceInfo = getVoiceInformation(voiceName);

	if (voiceInfo) {
		return voiceInformationToNapiObject(info.Env(), voiceInfo);
	} else {
		return env.Undefined();
	}
}

Napi::Boolean isAddonLoaded(const Napi::CallbackInfo& info) {
	auto env = info.Env();

	return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	exports.Set(Napi::String::New(env, "isAddonLoaded"), Napi::Function::New(env, isAddonLoaded));
	exports.Set(Napi::String::New(env, "getVoiceList"), Napi::Function::New(env, getVoiceList));
	exports.Set(Napi::String::New(env, "getVoiceInfo"), Napi::Function::New(env, getVoiceInfo));
	exports.Set(Napi::String::New(env, "getDefaultVoiceInfo"), Napi::Function::New(env, getDefaultVoiceInfo));
	exports.Set(Napi::String::New(env, "synthesize"), Napi::Function::New(env, synthesize));

	return exports;
}

NODE_API_MODULE(addon, Init)
