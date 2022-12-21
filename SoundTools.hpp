#pragma once
#include <Siv3D.hpp> // OpenSiv3D v0.6.4

struct ControlChangeData
{
	uint8 type;
	uint8 data;
};

enum class MetaEventType
{
	TrackName,		// トラック名
	Tempo,			// 一拍あたりの時間（マイクロ秒）
	SetMetre,		// 拍子
	EndOfTrack,
	Error,
};

struct MetreData
{
	uint32 numerator;
	uint32 denominator;
};

struct MetaEventData
{
	MetaEventType type;
	MetreData eventData;
	double tempo = 0;

	bool isError() const { return type == MetaEventType::Error; }
	bool isEndOfTrack() const { return type == MetaEventType::EndOfTrack; }

	static MetaEventData Error();
	static MetaEventData EndOfTrack();
	static MetaEventData SetMetre(uint32 numerator, uint32 denominator);
	static MetaEventData SetTempo(double bpm);
};

enum class MidiEventType
{
	NoteOff,
	NoteOn,
	PolyphonicKeyPressure,
	ControlChange,
	ProgramChange,
	ChannelPressure,
	PitchBend,
};

struct NoteOnEvent
{
	uint8 channel;
	uint8 note_number;
	uint8 velocity;

	NoteOnEvent() = default;
	NoteOnEvent(uint8 ch, uint8 note_number, uint8 velocity) :channel(ch), note_number(note_number), velocity(velocity) {}
};

struct NoteOffEvent
{
	uint8 channel;
	uint8 note_number;

	NoteOffEvent() = default;
	NoteOffEvent(uint8 ch, uint8 note_number) :channel(ch), note_number(note_number) {}
};

struct PolyphonicKeyPressureEvent
{
	uint8 channel;
	uint8 note_number;
	uint8 velocity;

	PolyphonicKeyPressureEvent() = default;
	PolyphonicKeyPressureEvent(uint8 ch, uint8 note_number, uint8 velocity) :channel(ch), note_number(note_number), velocity(velocity) {}
};

struct ControlChangeEvent
{
	uint8 channel;
	uint8 type;
	uint8 value;

	ControlChangeEvent() = default;
	ControlChangeEvent(uint8 ch, uint8 type, uint8 value) :channel(ch), type(type), value(value) {}
};

struct ProgramChangeEvent
{
	uint8 channel;
	uint8 type;

	ProgramChangeEvent() = default;
	ProgramChangeEvent(uint8 ch, uint8 type) :channel(ch), type(type) {}
};

struct ChannelPressureEvent
{
	uint8 channel;
	uint8 velocity;

	ChannelPressureEvent() = default;
	ChannelPressureEvent(uint8 ch, uint8 velocity) :channel(ch), velocity(velocity) {}
};

struct PitchBendEvent
{
	uint8 channel;
	uint16 value;

	PitchBendEvent() = default;
	PitchBendEvent(uint8 ch, uint16 value) :channel(ch), value(value) {}
};

using MidiEventData = std::variant<
	NoteOnEvent,
	NoteOffEvent,
	PolyphonicKeyPressureEvent,
	ControlChangeEvent,
	ProgramChangeEvent,
	ChannelPressureEvent,
	PitchBendEvent
>;

enum class EventType
{
	MidiEvent,
	SysExEvent,
	MetaEvent,
};

struct MidiCode
{
	int64 tick;
	EventType type;
	std::variant<MidiEventData, MetaEventData> data;
};

struct Note
{
	int64 tick;
	uint32 gate;
	uint8 key;
	uint8 velocity;
	uint8 ch;
};

class TrackData
{
public:

	TrackData(const Array<MidiCode>& operations) : m_operations(operations)
	{
		init();
	}

	void init();

	uint8 channel() const { return m_channel; }

	uint8 program() const { return m_program; }

	bool isPercussionTrack() const { return m_channel == 9; }

	template<class T>
	std::multimap<int64, T> getMIDIEvent(int64 tickBegin, int64 tickEnd) const
	{
		if constexpr (std::is_same_v<NoteOnEvent, T>)
		{
			return filterMIDIEvent(m_noteOnEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<NoteOffEvent, T>)
		{
			return filterMIDIEvent(m_noteOffEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<PolyphonicKeyPressureEvent, T>)
		{
			return filterMIDIEvent(m_polyphonicKeyPressureEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<ControlChangeEvent, T>)
		{
			return filterMIDIEvent(m_controlChangeEvent, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<ProgramChangeEvent, T>)
		{
			return filterMIDIEvent(m_programChangeEvent, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<PitchBendEvent, T>)
		{
			return filterMIDIEvent(m_pitchBendEvent, tickBegin, tickEnd);
		}
		else
		{
			static_assert(!sizeof(T*), "T is not MidiEventData");
		}
	}

private:

	friend class MidiData;

	template<class T>
	std::multimap<int64, T> filterMIDIEvent(const std::multimap<int64, T>& eventList, int64 tickBegin, int64 tickEnd) const
	{
		std::multimap<int64, T> result;
		auto itBegin = eventList.lower_bound(tickBegin);
		auto itEnd = eventList.lower_bound(tickEnd);
		for (auto it = itBegin; it != itEnd; ++it)
		{
			result.emplace(it->first, it->second);
		}
		return result;
	}

	std::multimap<int64, NoteOnEvent> m_noteOnEvents;
	std::multimap<int64, NoteOffEvent> m_noteOffEvents;
	std::multimap<int64, PolyphonicKeyPressureEvent> m_polyphonicKeyPressureEvents;
	std::multimap<int64, ControlChangeEvent> m_controlChangeEvent;
	std::multimap<int64, ProgramChangeEvent> m_programChangeEvent;
	std::multimap<int64, PitchBendEvent> m_pitchBendEvent;

	Array<MidiCode> m_operations;

	uint8 m_channel = 0;
	uint8 m_program = 0;
};

struct Beat
{
	uint32 localTick;
};

struct Measure
{
	int64 globalTick;
	uint32 measureIndex;
	uint32 beatStep;
	Array<Beat> beats;

	// 小節のtick数=1拍あたりのtick数×拍数
	uint32 widthOfTicks() const { return static_cast<uint32>(beats.size()) * beatStep; }

	void outputLog() const;
};

class MidiData
{
public:

	MidiData() = default;

	MidiData(const Array<TrackData>& tracks, uint16 resolution) :
		m_tracks(tracks),
		m_resolution(resolution)
	{
		init();
	}

	void init();

	const Array<TrackData>& tracks() const { return m_tracks; }

	Array<Measure> getMeasures() const;

	int64 endTick() const;

	uint16 resolution() const { return m_resolution; }

	double getBPM() const;

	double ticksToSeconds(int64 currentTick) const;

	int64 secondsToTicks(double seconds) const;

	double secondsToTicks2(double seconds) const;

	double lengthOfTime() const;

	int64 lengthSample(uint32 sampleRate) const;

private:

	// tick -> BPM
	std::map<int64, double> BPMSetEvents() const;

	bool intersects(uint32 range0begin, uint32 range0end, uint32 range1begin, uint32 range1end) const;

	struct MeasureInfo
	{
		MetreData metre;
		int64 globalTick;
	};

	uint16 m_resolution;

	uint64 m_endTick;

	Array<TrackData> m_tracks;

	Array<MeasureInfo> m_measures;

	std::map<int64, double> m_bpmSetEvents;
};

Optional<MidiData> LoadMidi(FilePathView path);

inline uint8 ReadByte(BinaryReader& reader)
{
	uint8 byte[1] = {};
	reader.read(byte, 1);
	return byte[0];
}

template<typename T>
inline T ReadBytes(BinaryReader& reader)
{
	uint8 bytes[sizeof(T)] = {};
	reader.read(bytes, sizeof(T));
	T value = 0;
	for (size_t i = 0; i < sizeof(T); i++)
	{
		value += (bytes[i] << ((sizeof(T) - i - 1) * 8));
	}
	return value;
}

inline std::string ReadText(BinaryReader& reader)
{
	const uint8 length = ReadBytes<uint8>(reader);
	Array<char> chars(length + 1ull, '\0');
	reader.read(chars.data(), length);
	return std::string(chars.data());
}

inline uint32 GetTick(const Array<uint8>& tickBytes)
{
	uint32 value = 0;
	for (size_t i = 0; i < tickBytes.size(); i++)
	{
		value += ((tickBytes[i] & 0x7F) << ((tickBytes.size() - i - 1) * 7));
	}
	return value;
}

MetaEventData MetaEventData::Error()
{
	MetaEventData data;
	data.type = MetaEventType::Error;
	return data;
}

MetaEventData MetaEventData::EndOfTrack()
{
	MetaEventData data;
	data.type = MetaEventType::EndOfTrack;
	return data;
}

MetaEventData MetaEventData::SetMetre(uint32 numerator, uint32 denominator)
{
	MetaEventData data;
	data.type = MetaEventType::SetMetre;
	data.eventData = MetreData{ numerator,denominator };
	return data;
}

MetaEventData MetaEventData::SetTempo(double bpm)
{
	MetaEventData data;
	data.type = MetaEventType::Tempo;
	data.tempo = bpm;
	return data;
}

void TrackData::init()
{
	HashTable<int, Note> onNotes;

	for (auto& code : m_operations)
	{
		if (code.type == EventType::MidiEvent)
		{
			auto& midiEvent = std::get<MidiEventData>(code.data);

			if (auto* pNoteOn = std::get_if<NoteOnEvent>(&midiEvent))
			{
				m_noteOnEvents.emplace(code.tick, *pNoteOn);
			}
			else if (auto* pNoteOff = std::get_if<NoteOffEvent>(&midiEvent))
			{
				m_noteOffEvents.emplace(code.tick, *pNoteOff);
			}
			else if (auto* pPolyphonicKeyPressure = std::get_if<PolyphonicKeyPressureEvent>(&midiEvent))
			{
				m_polyphonicKeyPressureEvents.emplace(code.tick, *pPolyphonicKeyPressure);
			}
			else if (auto* pControlChange = std::get_if<ControlChangeEvent>(&midiEvent))
			{
				m_controlChangeEvent.emplace(code.tick, *pControlChange);
			}
			else if (auto* pProgramChange = std::get_if<ProgramChangeEvent>(&midiEvent))
			{
				m_programChangeEvent.emplace(code.tick, *pProgramChange);
				// TODO: 途中でProgramChangeイベントがある場合に対応してない
				m_channel = pProgramChange->channel;
				m_program = pProgramChange->type;
			}
			else if (auto* pPitchBend = std::get_if<PitchBendEvent>(&midiEvent))
			{
				m_pitchBendEvent.emplace(code.tick, *pPitchBend);
			}
		}
	}
}

void Measure::outputLog() const
{
	Logger << U"measure: " << measureIndex;
	Logger << U"tick: " << globalTick;

	for (const auto& beat : beats)
	{
		Logger << U"  beat: " << beat.localTick;
	}
}

void MidiData::init()
{
	m_measures.clear();

	m_endTick = 0;

	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::SetMetre)
				{
					MeasureInfo info;
					info.metre = metaEvent.eventData;
					info.globalTick = code.tick;
					m_measures.push_back(info);
				}
			}

			m_endTick = Max<uint64>(m_endTick, code.tick);
		}
	}

	m_measures.sort_by([](const MeasureInfo& a, const MeasureInfo& b) { return a.globalTick < b.globalTick; });

	m_bpmSetEvents = BPMSetEvents();
}

Array<Measure> MidiData::getMeasures() const
{
	Array<Measure> result;

	int64 prevEventTick = 0;
	uint32 currentNumerator = 4;
	uint32 currentDenominator = 4;

	const auto addMeasures = [&](int64 nextTick)
	{
		const uint32 measureWidthOfTick = static_cast<uint32>(m_resolution * 4 * currentNumerator / currentDenominator);
		for (int64 tick = prevEventTick; tick < nextTick; tick += measureWidthOfTick)
		{
			Measure newMeasure;
			newMeasure.globalTick = tick;
			newMeasure.measureIndex = static_cast<uint32>(result.size());
			newMeasure.beatStep = measureWidthOfTick / currentNumerator;

			for (uint32 beatIndex = 0; beatIndex < currentNumerator; ++beatIndex)
			{
				Beat beat;
				beat.localTick = measureWidthOfTick * beatIndex / currentNumerator;
				newMeasure.beats.push_back(beat);
			}

			result.push_back(newMeasure);
		}
	};

	for (const auto& measureData : m_measures)
	{
		// 拍子イベントは必ず小節の先頭にある前提
		const int64 currentTick = measureData.globalTick;
		addMeasures(currentTick);

		currentNumerator = measureData.metre.numerator;
		currentDenominator = measureData.metre.denominator;
		prevEventTick = measureData.globalTick;
	}

	addMeasures(endTick());

	return result;
}

int64 MidiData::endTick() const
{
	int64 maxTick = 0;
	for (const auto& track : m_tracks)
	{
		maxTick = Max(maxTick, track.m_operations.back().tick);
	}
	return maxTick;
}

double MidiData::getBPM() const
{
	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::Tempo)
				{
					return metaEvent.tempo;
				}
			}
		}
	}

	return 120.0;
}

double MidiData::ticksToSeconds(int64 currentTick) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastTickToSec = 60.0 / (resolution * 120.0);
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		if (currentTick <= tick)
		{
			return sumOfTime + lastTickToSec * (currentTick - lastBPMSetTick);
		}

		sumOfTime += lastTickToSec * (tick - lastBPMSetTick);
		lastBPMSetTick = tick;
		lastTickToSec = 60.0 / (resolution * bpm);
	}

	return sumOfTime + lastTickToSec * (currentTick - lastBPMSetTick);
}

int64 MidiData::secondsToTicks(double seconds) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastBPM = 120;
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		const double nextSumOfTime = sumOfTime + (60.0 / (resolution * lastBPM)) * (tick - lastBPMSetTick);
		if (sumOfTime <= seconds && seconds < nextSumOfTime)
		{
			const double secToTicks = (resolution * lastBPM) / 60.0;
			return lastBPMSetTick + static_cast<int64>(Math::Round((seconds - sumOfTime) * secToTicks));
		}

		sumOfTime = nextSumOfTime;
		lastBPMSetTick = tick;
		lastBPM = bpm;
	}

	const double secToTicks = (resolution * lastBPM) / 60.0;
	return lastBPMSetTick + static_cast<int64>(Math::Round((seconds - sumOfTime) * secToTicks));
}

double MidiData::secondsToTicks2(double seconds) const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastBPM = 120;
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		const double nextSumOfTime = sumOfTime + (60.0 / (resolution * lastBPM)) * (tick - lastBPMSetTick);
		if (sumOfTime <= seconds && seconds < nextSumOfTime)
		{
			const double secToTicks = (resolution * lastBPM) / 60.0;
			return lastBPMSetTick + (seconds - sumOfTime) * secToTicks;
		}

		sumOfTime = nextSumOfTime;
		lastBPMSetTick = tick;
		lastBPM = bpm;
	}

	const double secToTicks = (resolution * lastBPM) / 60.0;
	return lastBPMSetTick + (seconds - sumOfTime) * secToTicks;
}

double MidiData::lengthOfTime() const
{
	const double resolution = m_resolution;
	double sumOfTime = 0;
	int64 lastBPMSetTick = 0;
	double lastBPM = 120;
	for (const auto [tick, bpm] : m_bpmSetEvents)
	{
		const double nextSumOfTime = sumOfTime + (60.0 / (resolution * lastBPM)) * (tick - lastBPMSetTick);

		sumOfTime = nextSumOfTime;
		lastBPMSetTick = tick;
		lastBPM = bpm;
	}

	return sumOfTime + (60.0 / (resolution * lastBPM)) * (m_endTick - lastBPMSetTick);
}

int64 MidiData::lengthSample(uint32 sampleRate) const
{
	return static_cast<int64>(lengthOfTime() * sampleRate);
}

// tick -> BPM
std::map<int64, double> MidiData::BPMSetEvents() const
{
	std::map<int64, double> result;
	for (const auto& track : m_tracks)
	{
		for (const auto& code : track.m_operations)
		{
			if (code.type == EventType::MetaEvent)
			{
				const auto& metaEvent = std::get<MetaEventData>(code.data);
				if (metaEvent.type == MetaEventType::Tempo)
				{
					result[code.tick] = metaEvent.tempo;
				}
			}
		}
	}

	return result;
}

bool MidiData::intersects(uint32 range0begin, uint32 range0end, uint32 range1begin, uint32 range1end) const
{
	const bool notIntersects = range0end < range1begin || range1end < range0begin;
	return !notIntersects;
}

namespace
{
	// https://sites.google.com/site/yyagisite/material/smfspec
	// http://quelque.sakura.ne.jp/midi_meta.html
	MetaEventData ReadMetaEvent(BinaryReader& reader)
	{
		const uint8 metaEventType = ReadBytes<uint8>(reader);
		switch (metaEventType)
		{
		case 0x0:
		{
			Logger << U"error: シーケンス番号（非対応フォーマット）";
			return MetaEventData::Error();
		}
		case 0x1:
		{
			Logger << U"テキストイベント";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x2:
		{
			Logger << U"著作権表示";
			const uint8 length = ReadBytes<uint8>(reader);
			Array<char> chars(length + 1ull, '\0');
			reader.read(chars.data(), length);
			Logger << Unicode::FromUTF8(std::string(chars.data()));
			return MetaEventData();
		}
		case 0x3:
		{
			Logger << U"シーケンス名/トラック名";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x4:
		{
			Logger << U"楽器名";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x5:
		{
			Logger << U"歌詞";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x6:
		{
			Logger << U"マーカー";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x7:
		{
			Logger << U"キューポイント";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x8:
		{
			Logger << U"プログラム名";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x9:
		{
			Logger << U"デバイス名";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
		case 0x20:
		{
			Logger << U"MIDIチャンネルプリフィクス";
			ReadBytes<uint8>(reader);
			/*const uint8 data =*/ ReadBytes<uint8>(reader);
			return MetaEventData();
		}
		case 0x21:
		{
			Logger << U"ポート指定";
			ReadBytes<uint8>(reader);
			/*const uint8 data =*/ ReadBytes<uint8>(reader);
			return MetaEventData();
		}
		case 0x2f:
		{
			Logger << U"end of track";
			ReadBytes<uint8>(reader);
			return MetaEventData::EndOfTrack();
		}
		case 0x51:
		{
			ReadBytes<uint8>(reader);//==3

			const auto a = ReadBytes<uint8>(reader);
			const auto b = ReadBytes<uint8>(reader);
			const auto c = ReadBytes<uint8>(reader);
			const auto microSecPerBeat = 1.0 * ((a << 16) + (b << 8) + c);

			const double bpm = 1.e6 * 60.0 / microSecPerBeat;
			Logger << U"テンポ: " << bpm;
			return MetaEventData::SetTempo(bpm);
		}
		case 0x54:
		{
			Logger << U"SMPTEオフセット";
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			return MetaEventData();
		}
		case 0x58:
		{
			//https://nekonenene.hatenablog.com/entry/2017/02/26/001351
			ReadBytes<uint8>(reader);
			const uint8 numerator = ReadBytes<uint8>(reader);
			const uint8 denominator = ReadBytes<uint8>(reader);
			Logger << U"拍子: " << numerator << U"/" << (1 << denominator);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			return MetaEventData::SetMetre(numerator, (1 << denominator));
		}
		case 0x59:
		{
			Logger << U"調号";
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			ReadBytes<uint8>(reader);
			return MetaEventData();
		}
		case 0x7f:
		{
			Logger << U"シーケンサ固有メタイベント";
			const uint8 length = ReadBytes<uint8>(reader);
			Array<uint8> data(length);
			reader.read(data.data(), length);
			return MetaEventData();
		}
		default:
			Logger << U" unknown metaEvent: " << metaEventType;
			Logger << U" テキストとして解釈します";
			const auto text = ReadText(reader);
			Logger << Unicode::FromUTF8(text);
			return MetaEventData();
		}
	}
}

Optional<MidiData> LoadMidi(FilePathView path)
{
	Logger << U"open \"" << path << U"\"";
	BinaryReader reader(path);

	if (!reader.isOpen())
	{
		Logger << U"couldn't open file";
		return none;
	}

	char mthd[5] = {};
	reader.read(mthd, 4);
	if (std::string(mthd) != "MThd")
	{
		Logger << U"error: std::string(mthd) != \"MThd\"";
		return none;
	}

	const uint32 headerLength = ReadBytes<uint32>(reader);
	if (headerLength != 6)
	{
		Logger << U"error: headerLength != 6";
		return none;
	}

	const uint16 format = ReadBytes<uint16>(reader);
	if ((format != 0) && (format != 1))
	{
		Logger << U"error: (format != 0) && (format != 1)";
		return none;
	}
	Logger << U"format: " << format;

	uint16 trackCount = ReadBytes<uint16>(reader);
	Logger << U"tracks: " << trackCount;

	const uint16 resolution = ReadBytes<uint16>(reader);
	Logger << U"resolution: " << resolution;

	Array<TrackData> tracks;

	for (uint32 trackIndex = 0; trackIndex < trackCount; ++trackIndex)
	{
		char mtrk[5] = {};
		reader.read(mtrk, 4);
		if (std::string(mtrk) != "MTrk")
		{
			Logger << U"error: std::string(str) != \"MTrk\"";
			return none;
		}

		uint32 trackBytesLength = ReadBytes<uint32>(reader);
		Logger << U"trackLength: " << trackBytesLength;

		Array<MidiCode> trackData;

		int64 currentTick = 0;
		uint8 prevOpCode = 0;

		const int64 trackEndPos = reader.getPos() + trackBytesLength;

		for (;;)
		{
			MidiCode codeData;

			Array<uint8> nextTick;
			for (;;)
			{
				uint8 byte = ReadBytes<uint8>(reader);
				if (byte < 0x80)
				{
					nextTick.push_back(byte);
					break;
				}
				else
				{
					nextTick.push_back(byte);
				}
			}

			if (5 <= nextTick.size())
			{
				Logger << U"error: 5 <= nextTick.size()";
				return none;
			}
			const uint32 step = GetTick(nextTick);
			currentTick += step;
			codeData.tick = currentTick;

			uint8 opcode = ReadBytes<uint8>(reader);

			// ランニングステータス
			if (opcode < 0x80)
			{
				opcode = prevOpCode;
				reader.setPos(reader.getPos() - 1);
			}

			prevOpCode = opcode;

			// https://sites.google.com/site/yyagisite/material/smfspec
			if (0x80 <= opcode && opcode <= 0x8F)
			{
				//Logger << U"ノートオフ";
				const uint8 channelIndex = opcode - 0x80;
				const uint8 key = ReadBytes<uint8>(reader);
				ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(NoteOffEvent(channelIndex, key));
			}
			else if (0x90 <= opcode && opcode <= 0x9F)
			{
				//Logger << U"ノートオン";
				const uint8 channelIndex = opcode - 0x90;
				const uint8 key = ReadBytes<uint8>(reader);
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				if (velocity == 0)
				{
					codeData.data = MidiEventData(NoteOffEvent(channelIndex, key));
				}
				else
				{
					codeData.data = MidiEventData(NoteOnEvent(channelIndex, key, velocity));
				}
			}
			else if (0xA0 <= opcode && opcode <= 0xAF)
			{
				//Logger << U"ポリフォニックキープレッシャー";
				const uint8 channelIndex = opcode - 0xA0;
				const uint8 key = ReadBytes<uint8>(reader);
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(PolyphonicKeyPressureEvent(channelIndex, key, velocity));
			}
			else if (0xB0 <= opcode && opcode <= 0xBF)
			{
				const uint8 channelIndex = opcode - 0xB0;
				const uint8 changeType = ReadBytes<uint8>(reader);
				const uint8 controlChangeData = ReadBytes<uint8>(reader);
				//Logger << U"コントロールチェンジ " << changeType;
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(ControlChangeEvent(channelIndex, changeType, controlChangeData));
			}
			else if (0xC0 <= opcode && opcode <= 0xCF)
			{
				//Logger << U"プログラムチェンジ";
				const uint8 channelIndex = opcode - 0xC0;
				const uint8 programNumber = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(ProgramChangeEvent(channelIndex, programNumber));
			}
			else if (0xD0 <= opcode && opcode <= 0xDF)
			{
				//Logger << U"チャンネルプレッシャー";
				const uint8 channelIndex = opcode - 0xD0;
				const uint8 velocity = ReadBytes<uint8>(reader);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(ChannelPressureEvent(channelIndex, velocity));
			}
			else if (0xE0 <= opcode && opcode <= 0xEF)
			{
				//Logger << U"ピッチベンド";
				const uint8 channelIndex = opcode - 0xE0;
				const uint8 m = ReadBytes<uint8>(reader);
				const uint8 l = ReadBytes<uint8>(reader);
				const uint16 value = ((l & 0x7F) << 7) + (m & 0x7F);
				codeData.type = EventType::MidiEvent;
				codeData.data = MidiEventData(PitchBendEvent(channelIndex, value));
			}
			else if (0xF0 == opcode)
			{
				//Logger << U"SysEx イベント";
				codeData.type = EventType::SysExEvent;
				while (ReadBytes<uint8>(reader) != 0xF7) {}
			}
			else if (0xF7 == opcode)
			{
				Logger << U"error: SysEx イベント（非対応フォーマット）";
				return none;
			}
			else if (0xFF == opcode)
			{
				const auto result = ReadMetaEvent(reader);
				codeData.type = EventType::MetaEvent;
				codeData.data = result;

				if (result.isEndOfTrack())
				{
					trackData.push_back(codeData);
					reader.setPos(trackEndPos);
					break;
				}
				else if (result.isError())
				{
					return none;
				}
			}
			else
			{
				Logger << U" unknown opcode: " << opcode;
				return none;
			}

			trackData.push_back(codeData);
		}

		tracks.emplace_back(trackData);
	}

	MidiData midiData(tracks, resolution);
	Logger << U"read succeeded";

	return midiData;
}
