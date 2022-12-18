# include <Siv3D.hpp> // OpenSiv3D v0.6.6

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

void RenderWave(Wave& wave, double amplitude, const Array<float>& frequencies, const ADSRConfig& adsr)
{
	const auto lengthOfSamples = 3 * wave.sampleRate();

	// 1.5秒のところでキー入力が離された想定
	const auto noteOffSample = lengthOfSamples / 2;

	wave.resize(lengthOfSamples, WaveSample::Zero());

	EnvGenerator envelope;

	const float deltaT = 1.0f / Wave::DefaultSampleRate;
	float time = 0;

	for (uint32 i = 0; i < lengthOfSamples; ++i)
	{
		if (i == noteOffSample)
		{
			envelope.noteOff();
		}

		double w = 0;
		for (auto freq : frequencies)
		{
			w += sin(Math::TwoPiF * freq * time)
				* amplitude * envelope.currentLevel();
		}

		wave[i].left = wave[i].right = static_cast<float>(w);
		time += deltaT;
		envelope.update(adsr, deltaT);
	}
}

void Main()
{
	double amplitude = 0.2;

	ADSRConfig adsr;
	adsr.attackTime = 0.1;
	adsr.decayTime = 0.1;
	adsr.sustainLevel = 0.8;
	adsr.releaseTime = 0.5;

	const Array<float> frequencies =
	{
		NoteNumberToFrequency(60), // C_4
		NoteNumberToFrequency(64), // E_4
		NoteNumberToFrequency(67), // G_4
	};

	Wave wave;
	RenderWave(wave, amplitude, frequencies, adsr);

	Audio audio(wave);
	audio.play();

	while (System::Update())
	{
		Vec2 pos(20, 20 - SliderHeight);
		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

		adsr.updateGUI(pos);

		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
		{
			RenderWave(wave, amplitude, frequencies, adsr);
			audio = Audio(wave);
			audio.play();
		}
	}
}
