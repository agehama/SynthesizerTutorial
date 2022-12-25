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
			return filterNoteEvent(m_noteOnEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<NoteOffEvent, T>)
		{
			return filterNoteEvent(m_noteOffEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<PolyphonicKeyPressureEvent, T>)
		{
			return filterNoteEvent(m_polyphonicKeyPressureEvents, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<ControlChangeEvent, T>)
		{
			return filterNoteEvent(m_controlChangeEvent, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<ProgramChangeEvent, T>)
		{
			return filterNoteEvent(m_programChangeEvent, tickBegin, tickEnd);
		}
		else if constexpr (std::is_same_v<PitchBendEvent, T>)
		{
			return filterNoteEvent(m_pitchBendEvent, tickBegin, tickEnd);
		}
		else
		{
			static_assert(!sizeof(T*), "T is not MidiEventData");
		}
	}

private:

	friend class MidiData;

	template<class T>
	std::multimap<int64, T> filterNoteEvent(const std::multimap<int64, T>& eventList, int64 tickBegin, int64 tickEnd) const
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

struct NoteNumber
{
	enum Name : uint8
	{
		C_Minus1,
		Cs_Minus1,
		D_Minus1,
		Ds_Minus1,
		E_Minus1,
		F_Minus1,
		Fs_Minus1,
		G_Minus1,
		Gs_Minus1,
		A_Minus1,
		As_Minus1,
		B_Minus1,
		C_0,
		Cs_0,
		D_0,
		Ds_0,
		E_0,
		F_0,
		Fs_0,
		G_0,
		Gs_0,
		A_0,
		As_0,
		B_0,
		C_1,
		Cs_1,
		D_1,
		Ds_1,
		E_1,
		F_1,
		Fs_1,
		G_1,
		Gs_1,
		A_1,
		As_1,
		B_1,
		C_2,
		Cs_2,
		D_2,
		Ds_2,
		E_2,
		F_2,
		Fs_2,
		G_2,
		Gs_2,
		A_2,
		As_2,
		B_2,
		C_3,
		Cs_3,
		D_3,
		Ds_3,
		E_3,
		F_3,
		Fs_3,
		G_3,
		Gs_3,
		A_3,
		As_3,
		B_3,
		C_4,
		Cs_4,
		D_4,
		Ds_4,
		E_4,
		F_4,
		Fs_4,
		G_4,
		Gs_4,
		A_4,
		As_4,
		B_4,
		C_5,
		Cs_5,
		D_5,
		Ds_5,
		E_5,
		F_5,
		Fs_5,
		G_5,
		Gs_5,
		A_5,
		As_5,
		B_5,
		C_6,
		Cs_6,
		D_6,
		Ds_6,
		E_6,
		F_6,
		Fs_6,
		G_6,
		Gs_6,
		A_6,
		As_6,
		B_6,
		C_7,
		Cs_7,
		D_7,
		Ds_7,
		E_7,
		F_7,
		Fs_7,
		G_7,
		Gs_7,
		A_7,
		As_7,
		B_7,
		C_8,
		Cs_8,
		D_8,
		Ds_8,
		E_8,
		F_8,
		Fs_8,
		G_8,
		Gs_8,
		A_8,
		As_8,
		B_8,
		C_9,
		Cs_9,
		D_9,
		Ds_9,
		E_9,
		F_9,
		Fs_9,
		G_9,
	};
};

class ScoreVisualizer
{
public:

	ScoreVisualizer(const Rect& drawArea)
		: m_drawArea(drawArea)
	{
		m_noteTimes.resize(128);
		m_noteRangeTimes.resize(128);
	}

	void drawBack() const
	{
		const double unitHeight = 1.0 * m_drawArea.h / (m_maxNoteNumber - m_minNoteNumber + 1);

		for (uint8 noteNumber = m_minNoteNumber; noteNumber <= m_maxNoteNumber; ++noteNumber)
		{
			const int zeroIndexedOctave = static_cast<int>(floor(noteNumber / 12.0));
			const int noteIndex = noteNumber - zeroIndexedOctave * 12;
			const int keyIndex = noteNumber - m_minNoteNumber;
			const double currentY = bottomY() - unitHeight * (keyIndex + 1);

			const RectF rect(m_drawArea.x, currentY, m_drawArea.w, unitHeight);

			if (whiteIndices.contains(noteIndex))
			{
				rect.draw(Color(28, 28, 32));
			}
			else
			{
				rect.draw(Color(18, 18, 21));
			}

			if (noteIndex == 0)
			{
				rect.bottom().draw(ColorF(0.4));
			}
			else if (noteIndex == 5)
			{
				rect.bottom().draw(ColorF(0.15));
			}
		}
	}

	void drawFront(const MidiData& midiData, double currentTime) const
	{
		const double beginTime = currentTime - m_pastSeconds;
		const double endTime = currentTime + m_laterSeconds;

		const auto beginTick = midiData.secondsToTicks(beginTime);
		const auto endTick = midiData.secondsToTicks(endTime);

		for (uint8 i = 0; i < 128; ++i)
		{
			m_noteTimes[i].clear();
			m_noteRangeTimes[i].clear();
		}
		
		for (const auto& track : midiData.tracks())
		{
			if (track.isPercussionTrack())
			{
				continue;
			}

			const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(beginTick, endTick);
			for (auto& [tick, noteOn] : noteOnEvents)
			{
				if (m_minNoteNumber <= noteOn.note_number && noteOn.note_number <= m_maxNoteNumber)
				{
					const double sec = midiData.ticksToSeconds(tick);
					m_noteTimes[noteOn.note_number].emplace_back(NoteType::NoteOn, sec);
				}
			}

			const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(beginTick, endTick);
			for (auto& [tick, noteOff] : noteOffEvents)
			{
				if (m_minNoteNumber <= noteOff.note_number && noteOff.note_number <= m_maxNoteNumber)
				{
					const double sec = midiData.ticksToSeconds(tick);
					m_noteTimes[noteOff.note_number].emplace_back(NoteType::NoteOff, sec);
				}
			}
		}

		// m_noteTimes => m_noteRangeTimes に変換
		for (uint8 i = 0; i < 128; ++i)
		{
			// 同じ時間だったら Off -> On の順
			m_noteTimes[i].sort_by([](const std::pair<NoteType, double>& a, const std::pair<NoteType, double>& b)
				{ return a.second == b.second ? a.first < b.first : a.second < b.second; });

			auto& keyNotes = m_noteRangeTimes[i];

			double noteBeginTime = beginTime;
			bool lastNoteIsOn = false;
			for (const auto& [type, time] : m_noteTimes[i])
			{
				if (type == NoteType::NoteOff)
				{
					keyNotes.emplace_back(noteBeginTime, time);
					lastNoteIsOn = false;
				}
				if (type == NoteType::NoteOn)
				{
					noteBeginTime = time;
					lastNoteIsOn = true;
				}
			}

			if (lastNoteIsOn)
			{
				keyNotes.emplace_back(noteBeginTime, endTime);
			}
		}

		const double unitHeight = 1.0 * m_drawArea.h / (m_maxNoteNumber - m_minNoteNumber + 1);

		const double currentX = Math::Map(currentTime, beginTime, endTime, leftX(), rightX());
		Line(currentX, bottomY(), currentX, topY()).draw(ColorF(0.4));

		// currentTime==0.0 は非再生時
		if (0.0 < currentTime)
		{
			for (uint8 noteNumber = m_minNoteNumber; noteNumber <= m_maxNoteNumber; ++noteNumber)
			{
				auto& keyNotes = m_noteRangeTimes[noteNumber];

				for (const auto& note : keyNotes)
				{
					// currentTimeを過ぎた表示は消す
					const double t0 = Max(note.x, currentTime);
					const double t1 = Max(note.y, currentTime);

					const double x0 = Math::Map(t0, beginTime, endTime, leftX(), rightX());
					const double x1 = Math::Map(t1, beginTime, endTime, leftX(), rightX());
					
					const int keyIndex = noteNumber - m_minNoteNumber;
					const double currentY = bottomY() - unitHeight * (keyIndex + 1);

					const RectF rect(x0, currentY, x1 - x0, unitHeight);

					if (note.x <= currentTime && currentTime < note.y)
					{
						rect.draw(Color(161, 58, 152));
					}
					else
					{
						rect.draw(Color(96, 28, 90));
					}
				}
			}
		}

		for (uint8 noteNumber = m_minNoteNumber; noteNumber <= m_maxNoteNumber; ++noteNumber)
		{
			const int zeroIndexedOctave = static_cast<int>(floor(noteNumber / 12.0));
			const int noteIndex = noteNumber - zeroIndexedOctave * 12;
			const int keyIndex = noteNumber - m_minNoteNumber;
			const double currentY = bottomY() - unitHeight * (keyIndex + 1);

			const RectF rect(m_drawArea.x, currentY, m_drawArea.w, unitHeight);

			if (noteIndex == 0)
			{
				m_font(U" C", zeroIndexedOctave - 1).draw(Arg::bottomLeft = rect.bottomCenter(), ColorF(0.7));
			}
		}
	}

	const Rect& drawArea() const
	{
		return m_drawArea;
	}
	void setDrawArea(const Rect& drawArea)
	{
		m_drawArea = drawArea;
	}

	double pastSeconds() const
	{
		return m_pastSeconds;
	}
	void setPastSeconds(double pastSeconds)
	{
		m_pastSeconds = pastSeconds;
	}

	double laterSeconds() const
	{
		return m_laterSeconds;
	}
	void setLaterSeconds(double laterSeconds)
	{
		m_laterSeconds = laterSeconds;
	}

	uint8 minNoteNumber() const
	{
		return m_minNoteNumber;
	}
	void setMinNoteNumber(uint8 minNoteNumber)
	{
		m_minNoteNumber = minNoteNumber;
	}

	uint8 maxNoteNumber() const
	{
		return m_maxNoteNumber;
	}
	void setMaxNoteNumber(uint8 maxNoteNumber)
	{
		m_maxNoteNumber = maxNoteNumber;
	}

private:

	const std::set<int> whiteIndices = { 0,2,4,5,7,9,11 };
	const std::set<int> blackIndices = { 1,3,6,8,10 };

	double leftX() const { return m_drawArea.x; }
	double rightX() const { return m_drawArea.x + m_drawArea.w; }
	double topY() const { return m_drawArea.y; }
	double bottomY() const { return m_drawArea.y + m_drawArea.h; }
	
	enum NoteType { NoteOff, NoteOn };

	Font m_font = Font(12);

	Rect m_drawArea;
	double m_pastSeconds = 2;
	double m_laterSeconds = 2;

	uint8 m_minNoteNumber = NoteNumber::C_3;
	uint8 m_maxNoteNumber = NoteNumber::B_6;

	mutable Array<Array<std::pair<NoteType, double>>> m_noteTimes;
	mutable Array<Array<Vec2>> m_noteRangeTimes;
};

class AudioVisualizer
{
public:

	enum VisualizeType
	{
		Spectrum,
		Spectrogram,
		Score,
	};

	enum FrequencyAxis
	{
		LinearScale,
		LogScale
	};

	AudioVisualizer(const Rect& drawArea = Scene::Rect(), VisualizeType visualizeType = VisualizeType::Spectrum, FrequencyAxis axisType = FrequencyAxis::LogScale)
		: m_inputWave(8192)
		, m_drawArea(drawArea)
		, m_scoreVisualizer(drawArea)
		, m_visualize(visualizeType)
		, m_freqAxis(axisType)
	{
		resetCurve();
	}

	void setInputWave(const Audio& audio)
	{
		const auto left = audio.getSamples(0);
		const auto right = audio.getSamples(1);

		for (size_t i = 0; i < m_inputWave.size(); ++i)
		{
			const int64 sampleIndex = audio.posSample() - m_inputWave.size() + i;
			const auto index = Clamp<int64>(sampleIndex, 0, audio.samples() - 1);
			m_inputWave[i] = (left[index] + right[index]) * 0.5f;
		}
	}

	void updateFFT()
	{
		FFT::Analyze(m_fft, &m_inputWave[0], m_inputWave.size(), Wave::DefaultSampleRate, FFTSampleLength::SL8K);

		if (m_visualize == VisualizeType::Score)
		{
			const auto minFreq = noteNumberToFrequency(m_scoreVisualizer.minNoteNumber() - 0.5);
			const auto maxFreq = noteNumberToFrequency(m_scoreVisualizer.maxNoteNumber() + 0.5);
			setMinFreq(minFreq);
			setMaxFreq(maxFreq);
		}

		++m_scrollY;
		m_scrollY %= m_drawArea.h;

		const double unitFreq = 1.0 * Wave::DefaultSampleRate / 8192;
		const size_t length = m_fft.buffer.size();
		int j = 0;
		for (int i = 1; i < length; ++i)
		{
			const double f = unitFreq * i;
			if (f < m_freqMin)
			{
				continue;
			}

			const double t = freqToAxis(f);
			if (1.0 <= t)
			{
				break;
			}

			const double x = m_drawArea.w * t;

			// 追加するポイントは1ピクセル以上離れるまでスキップ
			if (1 <= j && x - m_xs[j - 1] < 1.0)
			{
				continue;
			}

			// https://en.wikipedia.org/wiki/A-weighting
			const double f2 = f * f;
			const double ra1 = 12194.0 * 12194.0 * f2 * f2;
			const double ra2 = (f2 + 20.6 * 20.6) * sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0);
			const double aWeighting = toSpl(ra1 / ra2) + 2.0;

			const double spl = toSpl(m_fft.buffer[i]) + aWeighting;
			const double y = Clamp(Math::InvLerp(m_minSpl, m_maxSpl, spl), 0., 1.);
			m_ys[j] = Math::Lerp(m_ys[j], y, m_lerpStrength);
			m_xs[j] = x;

			if (m_visualize == VisualizeType::Spectrum)
			{
				m_drawCurve[j] = Vec2(Math::Lerp(leftX(), rightX(), t), Math::Lerp(bottomY(), topY(), m_ys[j]));
				m_points[j] = m_drawCurve[j];
			}
			else if (m_visualize == VisualizeType::Spectrogram || m_visualize == VisualizeType::Score)
			{
				m_drawCurve[j] = Vec2(x, m_scrollY);
			}

			m_colors[j] = Colormap01(m_ys[j], ColormapType::Inferno);
			++j;
		}

		m_points[j] = m_drawArea.br() + Vec2(0, 100);
		m_points[j + 1] = m_drawArea.bl() + Vec2(0, 100);
		m_points.resize(j + 2);
	}

	void drawScore(const MidiData& midiData, double currentTime) const
	{
		updateSpectrogramTexture();

		m_scoreVisualizer.drawBack();
		m_scoreVisualizer.drawFront(midiData, currentTime);

		const double w = m_renderTexture.width();
		const double h = m_renderTexture.height();

		const double w_ = h;
		const double h_ = 0.5 * w;

		{
			const auto blendState = Graphics2D::GetBlendState();
			Graphics2D::Internal::SetBlendState(BlendState::Additive);

			Graphics2D::SetScissorRect(Rect(m_drawArea.pos, static_cast<int>(h_), static_cast<int>(w_)));

			RasterizerState rs = RasterizerState::Default2D;
			rs.scissorEnable = true;
			ScopedRenderStates2D ss{ rs };

			const double centerX = (leftX() + rightX()) * 0.5;

			// 元はh/60秒で一周するのでh/60倍速で描画幅1秒になる
			const double drawScale = (h / 60.0) / m_scoreVisualizer.pastSeconds();

			m_renderTexture
				.scaled(w_ / w, drawScale * h_ / h)
				.rotatedAt(Vec2::Zero(), -90_deg)
				.draw(centerX - (h_ + m_scrollY * (h_ / h)) * drawScale, bottomY());

			m_renderTexture
				.scaled(w_ / w, drawScale * h_ / h)
				.rotatedAt(Vec2::Zero(), -90_deg)
				.draw(centerX - m_scrollY * (h_ / h) * drawScale, bottomY());

			Graphics2D::Internal::SetBlendState(blendState);
		}
	}

	void draw() const
	{
		if (m_visualize == VisualizeType::Spectrum)
		{
			for (int8 i = 0; i <= 10; ++i)
			{
				const double freq = noteNumberToFrequency(12 * i);
				const double t = freqToAxis(freq);
				if (t < 0.0 || 1.0 <= t)
				{
					continue;
				}
				const double x = Math::Lerp(leftX(), rightX(), t);
				m_font(U"C", i - 1).drawAt(x, topY() - 20, m_color);
				Line(x, topY(), x, topY() + 10).draw(m_color);
			}

			for (int spl = static_cast<int>(m_minSpl); spl <= m_maxSpl; spl += 10)
			{
				const double y = Math::Map(spl, m_minSpl, m_maxSpl, bottomY(), topY());
				m_font(spl).draw(rightX() + 10, y - m_font.height() * 0.5, m_color);
			}

			Graphics2D::SetScissorRect(m_drawArea);

			RasterizerState rs = RasterizerState::Default2D;
			rs.scissorEnable = true;
			ScopedRenderStates2D ss{ rs };

			const Polygon poly(LineString(m_points).asSpline().asLineString(10));
			poly.draw(m_color);
		}
		else if (m_visualize == VisualizeType::Spectrogram)
		{
			updateSpectrogramTexture();

			Graphics2D::SetScissorRect(m_drawArea.stretched(-1));

			RasterizerState rs = RasterizerState::Default2D;
			rs.scissorEnable = true;
			ScopedRenderStates2D ss{ rs };

			const double drawScale = 1;

			m_renderTexture.scaled(1, drawScale).draw(leftX(), m_drawArea.bl().y - m_scrollY * drawScale);
			m_renderTexture.scaled(1, drawScale).draw(leftX(), m_drawArea.bl().y - (m_drawArea.h + m_scrollY) * drawScale);
		}

		for (auto f : { 30,60,100,200,300,600,1000,2000,3000,6000,10000,15000,20000 })
		{
			const double t = freqToAxis(f);
			if (t < 0.0 || 1.0 < t)
			{
				continue;
			}
			const double x = Math::Lerp(leftX(), rightX(), t);
			m_font(f < 1000 ? Format(f) : Format(f / 1000, U"k")).drawAt(x, bottomY() + 20, Color(m_color, 128));
			Line({ x, topY() }, { x, bottomY() }).draw(Color(m_color, 128));
		}

		m_drawArea.drawFrame(1.0, m_color);
	}

	void setFreqRange(double minFreq, double maxFreq)
	{
		setMinFreq(minFreq);
		setMaxFreq(maxFreq);
	}

	void setSplRange(double minSpl, double maxSpl)
	{
		setMinSpl(minSpl);
		setMaxSpl(maxSpl);
	}

	void setDrawScore(uint8 minNoteNumber, uint8 maxNoteNumber)
	{
		m_scoreVisualizer.setMinNoteNumber(minNoteNumber);
		m_scoreVisualizer.setMaxNoteNumber(maxNoteNumber);
		setVisualizeType(VisualizeType::Score);
		setFreqAxis(FrequencyAxis::LogScale);
	}

	double minFreq() const
	{
		return m_freqMin;
	}
	void setMinFreq(double minFreq)
	{
		m_freqMin = minFreq;
		m_minFreqLog = log2(m_freqMin);
	}

	double maxFreq() const
	{
		return m_freqMax;
	}
	void setMaxFreq(double maxFreq)
	{
		m_freqMax = maxFreq;
		m_maxFreqLog = log2(m_freqMax);
	}

	double minSpl() const
	{
		return m_minSpl;
	}
	void setMinSpl(double minSpl)
	{
		m_minSpl = minSpl;
	}

	double maxSpl() const
	{
		return m_maxSpl;
	}
	void setMaxSpl(double maxSpl)
	{
		m_maxSpl = maxSpl;
	}

	VisualizeType visualizeType() const
	{
		return m_visualize;
	}
	void setVisualizeType(VisualizeType type)
	{
		m_visualize = type;
	}

	FrequencyAxis freqAxis() const
	{
		return m_freqAxis;
	}
	void setFreqAxis(FrequencyAxis type)
	{
		m_freqAxis = type;
	}

	double lerpStrength() const
	{
		return m_lerpStrength;
	}
	void setLerpStrength(double lerpStrength)
	{
		m_lerpStrength = lerpStrength;
	}

private:

	void updateSpectrogramTexture() const
	{
		ScopedRenderStates2D blend{ BlendState::Default3D };
		ScopedRenderTarget2D target(m_renderTexture);
		m_drawCurve.draw(1.5, m_colors);
	}

	double noteNumberToFrequency(double d) const
	{
		return 440.0 * pow(2.0, (d - 69) / 12.0);
	}

	void resetCurve()
	{
		m_renderTexture = RenderTexture(m_drawArea.w, m_drawArea.h, ColorF(0, 1), TextureFormat::R8G8B8A8_Unorm);

		m_drawCurve.resize(m_drawArea.w);
		m_points.resize(m_drawArea.w + 2);
		m_colors.resize(m_drawArea.w);
		m_ys.resize(m_drawArea.w);
		m_xs.resize(m_drawArea.w);
		for (size_t x = 0; x < m_drawCurve.size(); ++x)
		{
			m_drawCurve[x].x = rightX();
			m_drawCurve[x].y = bottomY();
			m_colors[x] = Palette::Black;
			m_ys[x] = 0;
			m_xs[x] = leftX() + x;
			m_points[x] = Vec2(leftX() + x, bottomY());
		}
	}

	double freqToAxis(double f) const
	{
		return m_freqAxis == FrequencyAxis::LinearScale
			? freqToLinearAxis(f)
			: freqToLogAxis(f);
	}

	double freqToLogAxis(double freq) const
	{
		return Math::InvLerp(m_minFreqLog, m_maxFreqLog, log2(freq));
	}

	double logAxisTofreq(double t) const
	{
		return pow(2.0, Math::Lerp(m_minFreqLog, m_maxFreqLog, t));
	}

	double freqToLinearAxis(double freq) const
	{
		return Math::InvLerp(m_freqMin, m_freqMax, freq);
	}

	double linearAxisTofreq(double t) const
	{
		return Math::Lerp(m_freqMin, m_freqMax, t);
	}

	double toSpl(double fftBuffer) const
	{
		return 20.0 * log10(fftBuffer);
	}

	double leftX() const { return m_drawArea.x; }
	double rightX() const { return m_drawArea.x + m_drawArea.w; }
	double topY() const { return m_drawArea.y; }
	double bottomY() const { return m_drawArea.y + m_drawArea.h; }

	Rect m_drawArea;
	Font m_font = Font(16);
	Color m_color = Palette::White;

	// 横軸の表示範囲: [30, 20000] Hz
	double m_freqMin = 30;
	double m_freqMax = 20000;
	double m_minFreqLog = log2(m_freqMin);
	double m_maxFreqLog = log2(m_freqMax);

	// 縦軸の表示範囲: [-100, 0] dB
	double m_minSpl = -100;
	double m_maxSpl = -0;

	VisualizeType m_visualize = VisualizeType::Spectrum;
	FrequencyAxis m_freqAxis = FrequencyAxis::LogScale;

	double m_lerpStrength = 0.2;

	ScoreVisualizer m_scoreVisualizer;

	Array<float> m_inputWave;
	FFTResult m_fft;
	RenderTexture m_renderTexture;
	int m_scrollY = 0;

	LineString m_drawCurve;
	Array<double> m_xs;
	Array<double> m_ys;
	Array<ColorF> m_colors;
	Array<Vec2> m_points;
};
