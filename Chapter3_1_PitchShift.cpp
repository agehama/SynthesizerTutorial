# include <Siv3D.hpp> // OpenSiv3D v0.6.6

#include "SoundTools.hpp"

double WaveSaw(double t, int n)
{
	double sum = 0;
	for (int k = 1; k <= n; ++k)
	{
		const double a = (k % 2 == 0 ? 1.0 : -1.0) / k;
		sum += a * sin(k * t);
	}

	return -2.0 * sum / Math::Pi;
}

double WaveSquare(double t, int n)
{
	double sum = 0;
	for (int k = 1; k <= n; ++k)
	{
		const double a = 2.0 * k - 1.0;
		sum += sin(a * t) / a;
	}

	return 4.0 * sum / Math::Pi;
}

double WavePulse(double t, int n, double d)
{
	double sum = 0;
	for (int k = 1; k <= n; ++k)
	{
		const double a = sin(k * d * Math::Pi) / k;
		sum += a * cos(k * (t - d * Math::Pi));
	}

	return 2.0 * d - 1.0 + 4.0 * sum / Math::Pi;
}

double WaveNoise()
{
	return Random(-1.0, 1.0);
}

enum class WaveForm
{
	Saw, Sin, Square, Noise,
};

static constexpr uint32 SamplingFreq = Wave::DefaultSampleRate;
static constexpr uint32 MinFreq = 20;
static constexpr uint32 MaxFreq = SamplingFreq / 2;

class OscillatorWavetable
{
public:

	OscillatorWavetable() = default;

	OscillatorWavetable(size_t resolution, double frequency, WaveForm waveType) :
		m_wave(resolution),
		m_xToIndex(resolution / 2_pi)
	{
		const int mSaw = static_cast<int>(MaxFreq / frequency);
		const int mSquare = static_cast<int>((MaxFreq + frequency) / (frequency * 2.0));

		for (size_t i = 0; i < resolution; ++i)
		{
			const double angle = 2_pi * i / resolution;

			switch (waveType)
			{
			case WaveForm::Saw:
				m_wave[i] = static_cast<float>(WaveSaw(angle, mSaw));
				break;
			case WaveForm::Sin:
				m_wave[i] = static_cast<float>(sin(angle));
				break;
			case WaveForm::Square:
				m_wave[i] = static_cast<float>(WaveSquare(angle, mSquare));
				break;
			case WaveForm::Noise:
				m_wave[i] = static_cast<float>(WaveNoise());
				break;
			default: break;
			}
		}
	}

	double get(double x) const
	{
		auto indexFloat = x * m_xToIndex;
		auto prevIndex = static_cast<size_t>(indexFloat);
		if (m_wave.size() == prevIndex)
		{
			prevIndex -= m_wave.size();
			indexFloat -= m_wave.size();
		}
		auto nextIndex = prevIndex + 1;
		if (m_wave.size() == nextIndex)
		{
			nextIndex = 0;
		}
		const auto x01 = indexFloat - prevIndex;
		return Math::Lerp(m_wave[prevIndex], m_wave[nextIndex], x01);
	}

private:

	Array<float> m_wave;
	double m_xToIndex = 0;
};

class BandLimitedWaveTables
{
public:

	BandLimitedWaveTables() = default;

	BandLimitedWaveTables(size_t tableCount, size_t waveResolution, WaveForm waveType)
	{
		m_waveTables.reserve(tableCount);
		m_tableFreqs.reserve(tableCount);

		for (size_t i = 0; i < tableCount; ++i)
		{
			const double rate = 1.0 * i / tableCount;
			const double freq = pow(2, Math::Lerp(m_minFreqLog, m_maxFreqLog, rate));

			m_waveTables.emplace_back(waveResolution, freq, waveType);
			m_tableFreqs.push_back(static_cast<float>(freq));
		}

		{
			m_indices.resize(2048);
			m_freqToIndex = m_indices.size() / (1.0 * MaxFreq);
			for (int i = 0; i < m_indices.size(); ++i)
			{
				const float freq = static_cast<float>(i / m_freqToIndex);
				const auto nextIt = std::upper_bound(m_tableFreqs.begin(), m_tableFreqs.end(), freq);
				m_indices[i] = static_cast<uint32>(nextIt - m_tableFreqs.begin());
			}
		}
	}

	double get(double x, double freq) const
	{
		const auto nextIndex = m_indices[static_cast<int>(freq * m_freqToIndex)];
		if (nextIndex == 0)
		{
			return m_waveTables.front().get(x);
		}
		if (static_cast<size_t>(nextIndex) == m_tableFreqs.size())
		{
			return m_waveTables.back().get(x);
		}

		const auto prevIndex = nextIndex - 1;
		const auto rate = Math::InvLerp(m_tableFreqs[prevIndex], m_tableFreqs[nextIndex], freq);
		return Math::Lerp(m_waveTables[prevIndex].get(x), m_waveTables[nextIndex].get(x), rate);
	}

private:

	double m_minFreqLog = log2(MinFreq);
	double m_maxFreqLog = log2(MaxFreq);
	Array<OscillatorWavetable> m_waveTables;
	Array<float> m_tableFreqs;

	Array<uint32> m_indices;
	double m_freqToIndex = 0;
};

static Array<BandLimitedWaveTables> OscWaveTables =
{
	BandLimitedWaveTables(80, 2048, WaveForm::Saw),
	BandLimitedWaveTables(1, 2048, WaveForm::Sin),
	BandLimitedWaveTables(80, 2048, WaveForm::Square),
	BandLimitedWaveTables(1, SamplingFreq, WaveForm::Noise),
};

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

bool SliderInt(const String& label, int& value, double min, double max, const Vec2& pos, double labelWidth = 80.0, double sliderWidth = 120.0, bool enabled = true)
{
	static std::unordered_map<int*, double> val;
	val[&value] = value;
	const bool result = SimpleGUI::Slider(label, val[&value], min, max, pos, labelWidth, sliderWidth, enabled);
	value = static_cast<int>(Math::Round(val[&value]));
	return result;
}

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
	double m_phase = 0;
	float m_velocity = 1.f;
	EnvGenerator m_envelope;
};

class Synthesizer
{
public:

	// 1サンプル波形を生成して返す
	WaveSample renderSample()
	{
		const auto deltaT = 1.0 / SamplingFreq;

		// エンベロープの更新
		for (auto& [noteNumber, noteState] : m_noteState)
		{
			noteState.m_envelope.update(m_adsr, deltaT);
		}

		// リリースが終了したノートを削除する
		std::erase_if(m_noteState, [&](const auto& noteState) { return noteState.second.m_envelope.isReleased(m_adsr); });

		const auto pitch = pow(2.0, m_pitchShift / 12.0);

		// 入力中の波形を加算して書き込む
		WaveSample sample(0, 0);
		for (auto& [noteNumber, noteState] : m_noteState)
		{
			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
			const auto frequency = NoteNumberToFrequency(noteNumber) * pitch;

			const auto osc = OscWaveTables[m_oscIndex].get(noteState.m_phase, frequency);
			noteState.m_phase += deltaT * frequency * 2_pi;
			if (Math::TwoPi < noteState.m_phase)
			{
				noteState.m_phase -= Math::TwoPi;
			}

			const auto w = static_cast<float>(osc * envLevel);
			sample.left += w;
			sample.right += w;
		}

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
		SliderInt(U"oscillator : {}"_fmt(m_oscIndex), m_oscIndex, 0, 3, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

		if (SimpleGUI::Slider(U"pitchShift : {:.2f}"_fmt(m_pitchShift), m_pitchShift, -24.0, 24.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth)
			 && KeyControl.pressed())
		{
			m_pitchShift = Math::Round(m_pitchShift);
		}

		m_adsr.updateGUI(pos);
	}

	void clear()
	{
		m_noteState.clear();
	}

	ADSRConfig& adsr()
	{
		return m_adsr;
	}

	int oscIndex() const
	{
		return m_oscIndex;
	}
	void setOscIndex(int oscIndex)
	{
		m_oscIndex = oscIndex;
	}

	double pitchShift() const
	{
		return m_pitchShift;
	}
	void setPitchShift(double pitchShift)
	{
		m_pitchShift = pitchShift;
	}

private:

	std::multimap<int8_t, NoteState> m_noteState;

	ADSRConfig m_adsr;

	double m_amplitude = 0.1;
	double m_pitchShift = 0.0;
	int m_oscIndex = 0;
};

class AudioRenderer : public IAudioStream
{
public:

	AudioRenderer()
	{
		// 100ms分のバッファを確保する
		const size_t bufferSize = SamplingFreq / 10;
		m_buffer.resize(bufferSize);
	}

	void setMidiData(const MidiData& midiData)
	{
		m_midiData = midiData;
	}

	void bufferSample()
	{
		const double currentTime = 1.0 * m_readMIDIPos / SamplingFreq;
		const double nextTime = 1.0 * (m_readMIDIPos + 1) / SamplingFreq;

		const auto currentTick = m_midiData.secondsToTicks(currentTime);
		const auto nextTick = m_midiData.secondsToTicks(nextTime);

		// tick が進んだら MIDI イベントの処理を更新する
		if (currentTick != nextTick)
		{
			for (const auto& track : m_midiData.tracks())
			{
				if (track.isPercussionTrack())
				{
					continue;
				}

				// 発生したノートオフイベントをシンセに登録
				const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
				for (auto& [tick, noteOff] : noteOffEvents)
				{
					m_synth.noteOff(noteOff.note_number);
				}

				// 発生したノートオンイベントをシンセに登録
				const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
				for (auto& [tick, noteOn] : noteOnEvents)
				{
					m_synth.noteOn(noteOn.note_number, noteOn.velocity);
				}
			}
		}

		const size_t writeIndex = m_bufferWritePos % m_buffer.size();

		m_buffer[writeIndex] = m_synth.renderSample();

		++m_bufferWritePos;
		++m_readMIDIPos;
	}

	bool bufferCompleted() const
	{
		return m_bufferReadPos + m_buffer.size() - 1 < m_bufferWritePos;
	}

	void updateGUI(Vec2& pos)
	{
		m_synth.updateGUI(pos);
	}

	const Array<WaveSample>& buffer() const
	{
		return m_buffer;
	}

	size_t bufferReadPos() const
	{
		return m_bufferReadPos;
	}

	size_t playingMIDIPos() const
	{
		return m_readMIDIPos - (m_bufferWritePos - m_bufferReadPos);
	}

	Synthesizer& synth()
	{
		return m_synth;
	}

private:

	void getAudio(float* left, float* right, const size_t samplesToWrite) override
	{
		for (size_t i = 0; i < samplesToWrite; ++i)
		{
			const auto& readSample = m_buffer[(m_bufferReadPos + i) % m_buffer.size()];

			*left++ = readSample.left;
			*right++ = readSample.right;
		}

		m_bufferReadPos += samplesToWrite;
	}

	bool hasEnded() override { return false; }
	void rewind() override {}

	Synthesizer m_synth;
	MidiData m_midiData;
	Array<WaveSample> m_buffer;
	size_t m_readMIDIPos = 0;
	size_t m_bufferReadPos = 0;
	size_t m_bufferWritePos = 0;
};

void Main()
{
	Window::Resize(1600, 900);

	auto midiDataOpt = LoadMidi(U"example/midi/test.mid");
	if (!midiDataOpt)
	{
		// ファイルが見つからない or 読み込みエラー
		return;
	}

	AudioVisualizer visualizer;
	visualizer.setSplRange(-60, -30);
	visualizer.setWindowType(AudioVisualizer::Hamming);
	visualizer.setDrawScore(NoteNumber::C_2, NoteNumber::B_7);
	visualizer.setDrawArea(Scene::Rect());

	std::shared_ptr<AudioRenderer> audioStream = std::make_shared<AudioRenderer>();
	audioStream->setMidiData(midiDataOpt.value());

	auto& synth = audioStream->synth();
	synth.setOscIndex(static_cast<int>(WaveForm::Sin));

	auto& adsr = synth.adsr();
	adsr.attackTime = 0.01;
	adsr.decayTime = 0.0;
	adsr.sustainLevel = 1.0;
	adsr.releaseTime = 0.01;

	bool isRunning = true;

	auto renderUpdate = [&]()
	{
		while (isRunning)
		{
			while (!audioStream->bufferCompleted())
			{
				audioStream->bufferSample();
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	};

	std::thread audioRenderThread(renderUpdate);

	Audio audio(audioStream);
	audio.play();

	bool showGUI = true;
	while (System::Update())
	{
		Vec2 pos(20, 20 - SliderHeight);

		// visualizerの更新
		{
			auto& visualizeBuffer = visualizer.inputWave();
			visualizeBuffer.fill(0);

			const auto& streamBuffer = audioStream->buffer();
			const auto readStartPos = audioStream->bufferReadPos();

			const auto fftInputSize = Min(visualizeBuffer.size(), streamBuffer.size());

			for (size_t i = 0; i < fftInputSize; ++i)
			{
				const auto inputIndex = (readStartPos + i) % streamBuffer.size();
				const auto& sample = streamBuffer[inputIndex];
				visualizeBuffer[i] = (sample.left + sample.right) * 0.5f;
			}

			visualizer.updateFFT(fftInputSize);

			const auto currentTime = 1.0 * audioStream->playingMIDIPos() / SamplingFreq;
			visualizer.drawScore(midiDataOpt.value(), currentTime);
		}

		if (KeyG.down())
		{
			showGUI = !showGUI;
		}

		if (showGUI)
		{
			audioStream->updateGUI(pos);
		}
	}

	isRunning = false;
	audioRenderThread.join();
}
