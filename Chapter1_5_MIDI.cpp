# include <Siv3D.hpp> // OpenSiv3D v0.6.6

#include "SoundTools.hpp"

const auto SliderHeight = 36;
const auto SliderWidth = 400;
const auto LabelWidth = 200;

struct ADSRConfig
{
	double attackTime = 0.01;
	double decayTime = 0.01;
	double sustainLevel = 0.6;
	double releaseTime = 0.4;

	void updateGUI(Vec2& pos)
	{
		SimpleGUI::Slider(U"attack : {:.2f}"_fmt(attackTime), attackTime, 0.0, 0.5, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
		SimpleGUI::Slider(U"decay : {:.2f}"_fmt(decayTime), decayTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
		SimpleGUI::Slider(U"sustain : {:.2f}"_fmt(sustainLevel), sustainLevel, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
		SimpleGUI::Slider(U"release : {:.2f}"_fmt(releaseTime), releaseTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
	}
};

class EnvGenerator
{
public:

	enum class State
	{
		Attack, Decay, Sustain, Release
	};

	void noteOff()
	{
		if (m_state != State::Release)
		{
			m_elapsed = 0;
			m_state = State::Release;
		}
	}

	void reset(State state)
	{
		m_elapsed = 0;
		m_state = state;
	}

	void update(const ADSRConfig& adsr, double dt)
	{
		switch (m_state)
		{
		case State::Attack: // 0.0 から 1.0 まで attackTime かけて増幅する
			if (m_elapsed < adsr.attackTime)
			{
				m_currentLevel = m_elapsed / adsr.attackTime;
				break;
			}
			m_elapsed -= adsr.attackTime;
			m_state = State::Decay;
			[[fallthrough]]; // Decay処理にそのまま続く

		case State::Decay: // 1.0 から sustainLevel まで decayTime かけて減衰する
			if (m_elapsed < adsr.decayTime)
			{
				m_currentLevel = Math::Lerp(1.0, adsr.sustainLevel, m_elapsed / adsr.decayTime);
				break;
			}
			m_elapsed -= adsr.decayTime;
			m_state = State::Sustain;
			[[fallthrough]]; // Sustain処理にそのまま続く


		case State::Sustain: // ノートオンの間 sustainLevel を維持する
			m_currentLevel = adsr.sustainLevel;
			break;

		case State::Release: // sustainLevel から 0.0 まで releaseTime かけて減衰する
			m_currentLevel = m_elapsed < adsr.releaseTime
				? Math::Lerp(adsr.sustainLevel, 0.0, m_elapsed / adsr.releaseTime)
				: 0.0;
			break;

		default: break;
		}

		m_elapsed += dt;
	}

	bool isReleased(const ADSRConfig& adsr) const
	{
		return m_state == State::Release && adsr.releaseTime <= m_elapsed;
	}

	double currentLevel() const
	{
		return m_currentLevel;
	}

	State state() const
	{
		return m_state;
	}

private:

	State m_state = State::Attack;
	double m_elapsed = 0; // ステート変更からの経過秒数
	double m_currentLevel = 0; // 現在のレベル [0, 1]
};

float NoteNumberToFrequency(int8_t d)
{
	return 440.0f * pow(2.0f, (d - 69) / 12.0f);
}

struct NoteState
{
	float m_velocity;
	EnvGenerator m_envelope;
};

class Synthesizer
{
public:

	// 1サンプル波形を生成して返す
	WaveSample renderSample()
	{
		const auto deltaT = 1.0 / Wave::DefaultSampleRate;

		// エンベロープの更新
		for (auto& [noteNumber, noteState] : m_noteState)
		{
			noteState.m_envelope.update(m_adsr, deltaT);
		}

		// リリースが終了したノートを削除する
		std::erase_if(m_noteState, [&](const auto& noteState) { return noteState.second.m_envelope.isReleased(m_adsr); });

		// 入力中の波形を加算して書き込む
		WaveSample sample(0, 0);
		for (auto& [noteNumber, noteState] : m_noteState)
		{
			const auto amplitude = noteState.m_envelope.currentLevel() * noteState.m_velocity;
			const auto frequency = NoteNumberToFrequency(noteNumber);

			const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * amplitude);
			sample.left += w;
			sample.right += w;
		}

		m_time += deltaT;

		return sample * static_cast<float>(m_amplitude);
	}

	void noteOn(int8_t noteNumber, int8_t velocity)
	{
		NoteState noteState;
		noteState.m_velocity = velocity / 127.0f;
		m_noteState.emplace(noteNumber, noteState);
	}

	void noteOff(int8_t noteNumber)
	{
		auto [beginIt, endIt] = m_noteState.equal_range(noteNumber);

		for (auto it = beginIt; it != endIt; ++it)
		{
			auto& envelope = it->second.m_envelope;

			// noteOnになっている最初の要素をnoteOffにする
			if (envelope.state() != EnvGenerator::State::Release)
			{
				envelope.noteOff();
				break;
			}
		}
	}

	void updateGUI(Vec2& pos)
	{
		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(m_amplitude), m_amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

		m_adsr.updateGUI(pos);
	}

	void clear()
	{
		m_noteState.clear();
	}

private:

	std::multimap<int8_t, NoteState> m_noteState;

	ADSRConfig m_adsr;

	double m_amplitude = 0.1;

	double m_time = 0;
};

void RenderWave(Wave& wave, Synthesizer& synth, const MidiData& midiData)
{
	const auto lengthOfSamples = static_cast<int64>(ceil(midiData.lengthOfTime() * wave.sampleRate()));

	wave.resize(lengthOfSamples, WaveSample::Zero());

	for (int64 i = 0; i < lengthOfSamples; ++i)
	{
		const auto currentTime = 1.0 * i / wave.sampleRate();
		const auto nextTime = 1.0 * (i + 1) / wave.sampleRate();

		const auto currentTick = midiData.secondsToTicks(currentTime);
		const auto nextTick = midiData.secondsToTicks(nextTime);

		// tick が進んだら MIDI イベントの処理を更新する
		if (currentTick != nextTick)
		{
			for (const auto& track : midiData.tracks())
			{
				if (track.isPercussionTrack())
				{
					continue;
				}

				// 発生したノートオンイベントをシンセに登録
				const auto noteOnEvents = track.getRangeNoteEvent<NoteOnEvent>(currentTick, nextTick);
				for (auto& [tick, noteOn] : noteOnEvents)
				{
					synth.noteOn(noteOn.note_number, noteOn.velocity);
				}

				// 発生したノートオフイベントをシンセに登録
				const auto noteOffEvents = track.getRangeNoteEvent<NoteOffEvent>(currentTick, nextTick);
				for (auto& [tick, noteOff] : noteOffEvents)
				{
					synth.noteOff(noteOff.note_number);
				}
			}
		}

		// シンセを更新して現在の波形を書き込む
		wave[i] = synth.renderSample();
	}
}

void Main()
{
	auto midiDataOpt = LoadMidi(U"short_loop.mid");
	if (!midiDataOpt)
	{
		// ファイルが見つからない or 読み込みエラー
		return;
	}

	const MidiData& midiData = midiDataOpt.value();

	Synthesizer synth;

	Wave wave;
	RenderWave(wave, synth, midiData);

	Audio audio(wave);
	audio.play();

	while (System::Update())
	{
		Vec2 pos(20, 20 - SliderHeight);

		synth.updateGUI(pos);

		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
		{
			audio.stop();
			synth.clear();

			RenderWave(wave, synth, midiData);
			audio = Audio(wave);
			audio.play();
		}
	}
}
