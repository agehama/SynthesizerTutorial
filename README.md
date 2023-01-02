# SynthesizerTutorial
ソフトウェアシンセサイザーを作成するチュートリアルのソースコードです。

記事：  
１．https://qiita.com/agehama_/items/7da430491400e9a2b6a7  
２．https://qiita.com/agehama_/items/863933459ca44ca5dbe0  

## ビルド環境
- Visual Studio 2022
- OpenSiv3D v0.6.6

## 記事１の各ステップの差分
### １．サイン波を再生する
```diff
@@ -0,0 +1,45 @@
+﻿# include <Siv3D.hpp> // OpenSiv3D v0.6.6
+
+const auto SliderHeight = 36;
+const auto SliderWidth = 400;
+const auto LabelWidth = 200;
+
+Wave RenderWave(uint32 seconds, double amplitude, double frequency)
+{
+	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
+
+	Wave wave(lengthOfSamples);
+
+	for (uint32 i = 0; i < lengthOfSamples; ++i)
+	{
+		const double sec = 1.0f * i / Wave::DefaultSampleRate;
+		const double w = sin(Math::TwoPiF * frequency * sec) * amplitude;
+		wave[i].left = wave[i].right = static_cast<float>(w);
+	}
+
+	return wave;
+}
+
+void Main()
+{
+	double amplitude = 0.2;
+	double frequency = 440.0;
+
+	uint32 seconds = 3;
+
+	Audio audio(RenderWave(seconds, amplitude, frequency));
+	audio.play();
+
+	while (System::Update())
+	{
+		Vec2 pos(20, 20 - SliderHeight);
+		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+		SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+
+		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
+		{
+			audio = Audio(RenderWave(seconds, amplitude, frequency));
+			audio.play();
+		}
+	}
+}
```
### ２．ADSR エンベロープを実装する
```diff
@@ -4,17 +4,134 @@ const auto SliderHeight = 36;
 const auto SliderWidth = 400;
 const auto LabelWidth = 200;
 
-Wave RenderWave(uint32 seconds, double amplitude, double frequency)
+struct ADSRConfig
+{
+	double attackTime = 0.01;
+	double decayTime = 0.01;
+	double sustainLevel = 0.6;
+	double releaseTime = 0.4;
+
+	void updateGUI(Vec2& pos)
+	{
+		SimpleGUI::Slider(U"attack : {:.2f}"_fmt(attackTime), attackTime, 0.0, 0.5, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+		SimpleGUI::Slider(U"decay : {:.2f}"_fmt(decayTime), decayTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+		SimpleGUI::Slider(U"sustain : {:.2f}"_fmt(sustainLevel), sustainLevel, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+		SimpleGUI::Slider(U"release : {:.2f}"_fmt(releaseTime), releaseTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+	}
+};
+
+class EnvGenerator
+{
+public:
+
+	enum class State
+	{
+		Attack, Decay, Sustain, Release
+	};
+
+	void noteOff()
+	{
+		if (m_state != State::Release)
+		{
+			m_elapsed = 0;
+			m_state = State::Release;
+		}
+	}
+
+	void reset(State state)
+	{
+		m_elapsed = 0;
+		m_state = state;
+	}
+
+	void update(const ADSRConfig& adsr, double dt)
+	{
+		switch (m_state)
+		{
+		case State::Attack: // 0.0 から 1.0 まで attackTime かけて増幅する
+			if (m_elapsed < adsr.attackTime)
+			{
+				m_currentLevel = m_elapsed / adsr.attackTime;
+				break;
+			}
+			m_elapsed -= adsr.attackTime;
+			m_state = State::Decay;
+			[[fallthrough]]; // Decay処理にそのまま続く
+
+		case State::Decay: // 1.0 から sustainLevel まで decayTime かけて減衰する
+			if (m_elapsed < adsr.decayTime)
+			{
+				m_currentLevel = Math::Lerp(1.0, adsr.sustainLevel, m_elapsed / adsr.decayTime);
+				break;
+			}
+			m_elapsed -= adsr.decayTime;
+			m_state = State::Sustain;
+			[[fallthrough]]; // Sustain処理にそのまま続く
+
+
+		case State::Sustain: // ノートオンの間 sustainLevel を維持する
+			m_currentLevel = adsr.sustainLevel;
+			break;
+
+		case State::Release: // sustainLevel から 0.0 まで releaseTime かけて減衰する
+			m_currentLevel = m_elapsed < adsr.releaseTime
+				? Math::Lerp(adsr.sustainLevel, 0.0, m_elapsed / adsr.releaseTime)
+				: 0.0;
+			break;
+
+		default: break;
+		}
+
+		m_elapsed += dt;
+	}
+
+	bool isReleased(const ADSRConfig& adsr) const
+	{
+		return m_state == State::Release && adsr.releaseTime <= m_elapsed;
+	}
+
+	double currentLevel() const
+	{
+		return m_currentLevel;
+	}
+
+	State state() const
+	{
+		return m_state;
+	}
+
+private:
+
+	State m_state = State::Attack;
+	double m_elapsed = 0; // ステート変更からの経過秒数
+	double m_currentLevel = 0; // 現在のレベル [0, 1]
+};
+
+Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRConfig& adsr)
 {
 	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
 
 	Wave wave(lengthOfSamples);
 
+	// 0サンプル目でノートオン
+	EnvGenerator envelope;
+
+	// 半分経過したところでノートオフ
+	const auto noteOffSample = lengthOfSamples / 2;
+
+	const float deltaT = 1.0f / Wave::DefaultSampleRate;
+	float time = 0;
 	for (uint32 i = 0; i < lengthOfSamples; ++i)
 	{
-		const double sec = 1.0f * i / Wave::DefaultSampleRate;
-		const double w = sin(Math::TwoPiF * frequency * sec) * amplitude;
+		if (i == noteOffSample)
+		{
+			envelope.noteOff();
+		}
+		const auto w = sin(Math::TwoPiF * frequency * time)
+			* amplitude * envelope.currentLevel();
 		wave[i].left = wave[i].right = static_cast<float>(w);
+		time += deltaT;
+		envelope.update(adsr, deltaT);
 	}
 
 	return wave;
@@ -27,7 +144,13 @@ void Main()
 
 	uint32 seconds = 3;
 
-	Audio audio(RenderWave(seconds, amplitude, frequency));
+	ADSRConfig adsr;
+	adsr.attackTime = 0.1;
+	adsr.decayTime = 0.1;
+	adsr.sustainLevel = 0.8;
+	adsr.releaseTime = 0.5;
+
+	Audio audio(RenderWave(seconds, amplitude, frequency, adsr));
 	audio.play();
 
 	while (System::Update())
@@ -36,9 +159,11 @@ void Main()
 		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
 		SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
 
+		adsr.updateGUI(pos);
+
 		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
 		{
-			audio = Audio(RenderWave(seconds, amplitude, frequency));
+			audio = Audio(RenderWave(seconds, amplitude, frequency, adsr));
 			audio.play();
 		}
 	}
```
### ３．音階に対応させる
```diff
@@ -107,7 +107,12 @@ private:
 	double m_currentLevel = 0; // 現在のレベル [0, 1]
 };
 
-Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRConfig& adsr)
+float NoteNumberToFrequency(int8_t d)
+{
+	return 440.0f * pow(2.0f, (d - 69) / 12.0f);
+}
+
+Wave RenderWave(uint32 seconds, double amplitude, const Array<int8_t>& noteNumbers, const ADSRConfig& adsr)
 {
 	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
 
@@ -121,14 +126,23 @@ Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRCo
 
 	const float deltaT = 1.0f / Wave::DefaultSampleRate;
 	float time = 0;
+
 	for (uint32 i = 0; i < lengthOfSamples; ++i)
 	{
 		if (i == noteOffSample)
 		{
 			envelope.noteOff();
 		}
-		const auto w = sin(Math::TwoPiF * frequency * time)
-			* amplitude * envelope.currentLevel();
+
+		// 和音の各波形を加算合成する
+		double w = 0;
+		for (auto note : noteNumbers)
+		{
+			const auto freq = NoteNumberToFrequency(note);
+			w += sin(Math::TwoPiF * freq * time)
+				* amplitude * envelope.currentLevel();
+		}
+
 		wave[i].left = wave[i].right = static_cast<float>(w);
 		time += deltaT;
 		envelope.update(adsr, deltaT);
@@ -140,7 +154,6 @@ Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRCo
 void Main()
 {
 	double amplitude = 0.2;
-	double frequency = 440.0;
 
 	uint32 seconds = 3;
 
@@ -150,20 +163,26 @@ void Main()
 	adsr.sustainLevel = 0.8;
 	adsr.releaseTime = 0.5;
 
-	Audio audio(RenderWave(seconds, amplitude, frequency, adsr));
+	const Array<int8_t> noteNumbers =
+	{
+		60, // C_4
+		64, // E_4
+		67, // G_4
+	};
+
+	Audio audio(RenderWave(seconds, amplitude, noteNumbers, adsr));
 	audio.play();
 
 	while (System::Update())
 	{
 		Vec2 pos(20, 20 - SliderHeight);
 		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
-		SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
 
 		adsr.updateGUI(pos);
 
 		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
 		{
-			audio = Audio(RenderWave(seconds, amplitude, frequency, adsr));
+			audio = Audio(RenderWave(seconds, amplitude, noteNumbers, adsr));
 			audio.play();
 		}
 	}
```
### ４．シンセサイザーを定義する
```diff
@@ -112,40 +112,114 @@ float NoteNumberToFrequency(int8_t d)
 	return 440.0f * pow(2.0f, (d - 69) / 12.0f);
 }
 
-Wave RenderWave(uint32 seconds, double amplitude, const Array<int8_t>& noteNumbers, const ADSRConfig& adsr)
+struct NoteState
+{
+	EnvGenerator m_envelope;
+};
+
+class Synthesizer
+{
+public:
+
+	// 1サンプル波形を生成して返す
+	WaveSample renderSample()
+	{
+		const auto deltaT = 1.0 / Wave::DefaultSampleRate;
+
+		// エンベロープの更新
+		for (auto& [noteNumber, noteState] : m_noteState)
+		{
+			noteState.m_envelope.update(m_adsr, deltaT);
+		}
+
+		// リリースが終了したノートを削除する
+		std::erase_if(m_noteState, [&](const auto& noteState) { return noteState.second.m_envelope.isReleased(m_adsr); });
+
+		// 入力中の波形を加算して書き込む
+		WaveSample sample(0, 0);
+		for (auto& [noteNumber, noteState] : m_noteState)
+		{
+			const auto envLevel = noteState.m_envelope.currentLevel();
+			const auto frequency = NoteNumberToFrequency(noteNumber);
+
+			const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * envLevel);
+			sample.left += w;
+			sample.right += w;
+		}
+
+		m_time += deltaT;
+
+		return sample * static_cast<float>(m_amplitude);
+	}
+
+	void noteOn(int8_t noteNumber)
+	{
+		m_noteState.emplace(noteNumber, NoteState());
+	}
+
+	void noteOff(int8_t noteNumber)
+	{
+		auto [beginIt, endIt] = m_noteState.equal_range(noteNumber);
+
+		for (auto it = beginIt; it != endIt; ++it)
+		{
+			auto& envelope = it->second.m_envelope;
+
+			// noteOnになっている最初の要素をnoteOffにする
+			if (envelope.state() != EnvGenerator::State::Release)
+			{
+				envelope.noteOff();
+				break;
+			}
+		}
+	}
+
+	void updateGUI(Vec2& pos)
+	{
+		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(m_amplitude), m_amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+
+		m_adsr.updateGUI(pos);
+	}
+
+	void clear()
+	{
+		m_noteState.clear();
+	}
+
+private:
+
+	std::multimap<int8_t, NoteState> m_noteState;
+
+	ADSRConfig m_adsr;
+
+	double m_amplitude = 0.2;
+
+	double m_time = 0;
+};
+
+Wave RenderWave(uint32 seconds, Synthesizer& synth)
 {
 	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
 
 	Wave wave(lengthOfSamples);
 
-	// 0サンプル目でノートオン
-	EnvGenerator envelope;
-
 	// 半分経過したところでノートオフ
 	const auto noteOffSample = lengthOfSamples / 2;
 
-	const float deltaT = 1.0f / Wave::DefaultSampleRate;
-	float time = 0;
+	synth.noteOn(60); // C_4
+	synth.noteOn(64); // E_4
+	synth.noteOn(67); // G_4
 
 	for (uint32 i = 0; i < lengthOfSamples; ++i)
 	{
 		if (i == noteOffSample)
 		{
-			envelope.noteOff();
-		}
-
-		// 和音の各波形を加算合成する
-		double w = 0;
-		for (auto note : noteNumbers)
-		{
-			const auto freq = NoteNumberToFrequency(note);
-			w += sin(Math::TwoPiF * freq * time)
-				* amplitude * envelope.currentLevel();
+			synth.noteOff(60);
+			synth.noteOff(64);
+			synth.noteOff(67);
 		}
 
-		wave[i].left = wave[i].right = static_cast<float>(w);
-		time += deltaT;
-		envelope.update(adsr, deltaT);
+		wave[i] = synth.renderSample();
 	}
 
 	return wave;
@@ -153,36 +227,24 @@ Wave RenderWave(uint32 seconds, double amplitude, const Array<int8_t>& noteNumbe
 
 void Main()
 {
-	double amplitude = 0.2;
-
 	uint32 seconds = 3;
 
-	ADSRConfig adsr;
-	adsr.attackTime = 0.1;
-	adsr.decayTime = 0.1;
-	adsr.sustainLevel = 0.8;
-	adsr.releaseTime = 0.5;
+	Synthesizer synth;
 
-	const Array<int8_t> noteNumbers =
-	{
-		60, // C_4
-		64, // E_4
-		67, // G_4
-	};
-
-	Audio audio(RenderWave(seconds, amplitude, noteNumbers, adsr));
+	Audio audio(RenderWave(seconds, synth));
 	audio.play();
 
 	while (System::Update())
 	{
 		Vec2 pos(20, 20 - SliderHeight);
-		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
 
-		adsr.updateGUI(pos);
+		synth.updateGUI(pos);
 
 		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
 		{
-			audio = Audio(RenderWave(seconds, amplitude, noteNumbers, adsr));
+			synth.clear();
+
+			audio = Audio(RenderWave(seconds, synth));
 			audio.play();
 		}
 	}
```
### ５．譜面を再生する
```diff
@@ -1,5 +1,7 @@
 ﻿# include <Siv3D.hpp> // OpenSiv3D v0.6.6
 
+#include "SoundTools.hpp"
+
 const auto SliderHeight = 36;
 const auto SliderWidth = 400;
 const auto LabelWidth = 200;
@@ -114,6 +116,7 @@ float NoteNumberToFrequency(int8_t d)
 
 struct NoteState
 {
+	float m_velocity = 1.f;
 	EnvGenerator m_envelope;
 };
 
@@ -139,7 +142,7 @@ public:
 		WaveSample sample(0, 0);
 		for (auto& [noteNumber, noteState] : m_noteState)
 		{
-			const auto envLevel = noteState.m_envelope.currentLevel();
+			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
 			const auto frequency = NoteNumberToFrequency(noteNumber);
 
 			const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * envLevel);
@@ -152,9 +155,11 @@ public:
 		return sample * static_cast<float>(m_amplitude);
 	}
 
-	void noteOn(int8_t noteNumber)
+	void noteOn(int8_t noteNumber, int8_t velocity)
 	{
-		m_noteState.emplace(noteNumber, NoteState());
+		NoteState noteState;
+		noteState.m_velocity = velocity / 127.0f;
+		m_noteState.emplace(noteNumber, noteState);
 	}
 
 	void noteOff(int8_t noteNumber)
@@ -192,33 +197,52 @@ private:
 
 	ADSRConfig m_adsr;
 
-	double m_amplitude = 0.2;
+	double m_amplitude = 0.1;
 
 	double m_time = 0;
 };
 
-Wave RenderWave(uint32 seconds, Synthesizer& synth)
+Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
 {
-	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
+	const auto lengthOfSamples = static_cast<int64>(ceil(midiData.lengthOfTime() * Wave::DefaultSampleRate));
 
 	Wave wave(lengthOfSamples);
 
-	// 半分経過したところでノートオフ
-	const auto noteOffSample = lengthOfSamples / 2;
+	for (int64 i = 0; i < lengthOfSamples; ++i)
+	{
+		const auto currentTime = 1.0 * i / wave.sampleRate();
+		const auto nextTime = 1.0 * (i + 1) / wave.sampleRate();
 
-	synth.noteOn(60); // C_4
-	synth.noteOn(64); // E_4
-	synth.noteOn(67); // G_4
+		const auto currentTick = midiData.secondsToTicks(currentTime);
+		const auto nextTick = midiData.secondsToTicks(nextTime);
 
-	for (uint32 i = 0; i < lengthOfSamples; ++i)
-	{
-		if (i == noteOffSample)
+		// tick が進んだら MIDI イベントの処理を更新する
+		if (currentTick != nextTick)
 		{
-			synth.noteOff(60);
-			synth.noteOff(64);
-			synth.noteOff(67);
+			for (const auto& track : midiData.tracks())
+			{
+				if (track.isPercussionTrack())
+				{
+					continue;
+				}
+
+				// 発生したノートオンイベントをシンセに登録
+				const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
+				for (auto& [tick, noteOn] : noteOnEvents)
+				{
+					synth.noteOn(noteOn.note_number, noteOn.velocity);
+				}
+
+				// 発生したノートオフイベントをシンセに登録
+				const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
+				for (auto& [tick, noteOff] : noteOffEvents)
+				{
+					synth.noteOff(noteOff.note_number);
+				}
+			}
 		}
 
+		// シンセを1サンプル更新して波形を書き込む
 		wave[i] = synth.renderSample();
 	}
 
@@ -227,11 +251,18 @@ Wave RenderWave(uint32 seconds, Synthesizer& synth)
 
 void Main()
 {
-	uint32 seconds = 3;
+	auto midiDataOpt = LoadMidi(U"short_loop.mid");
+	if (!midiDataOpt)
+	{
+		// ファイルが見つからない or 読み込みエラー
+		return;
+	}
+
+	const MidiData& midiData = midiDataOpt.value();
 
 	Synthesizer synth;
 
-	Audio audio(RenderWave(seconds, synth));
+	Audio audio(RenderWave(synth, midiData));
 	audio.play();
 
 	while (System::Update())
@@ -242,9 +273,10 @@ void Main()
 
 		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
 		{
+			audio.stop();
 			synth.clear();
 
-			audio = Audio(RenderWave(seconds, synth));
+			audio = Audio(RenderWave(synth, midiData));
 			audio.play();
 		}
 	}
```
## 記事２の各ステップの差分
### １．基本波形を追加する
```diff
@@ -2,6 +2,76 @@
 
 #include "SoundTools.hpp"
 
+double WaveSaw(double t, int n)
+{
+	double sum = 0;
+	for (int k = 1; k <= n; ++k)
+	{
+		const double a = (k % 2 == 0 ? 1.0 : -1.0) / k;
+		sum += a * sin(k * t);
+	}
+
+	return -2.0 * sum / Math::Pi;
+}
+
+double WaveSquare(double t, int n)
+{
+	double sum = 0;
+	for (int k = 1; k <= n; ++k)
+	{
+		const double a = 2.0 * k - 1.0;
+		sum += sin(a * t) / a;
+	}
+
+	return 4.0 * sum / Math::Pi;
+}
+
+double WavePulse(double t, int n, double d)
+{
+	double sum = 0;
+	for (int k = 1; k <= n; ++k)
+	{
+		const double a = sin(k * d * Math::Pi) / k;
+		sum += a * cos(k * (t - d * Math::Pi));
+	}
+
+	return 2.0 * d - 1.0 + 4.0 * sum / Math::Pi;
+}
+
+double WaveNoise()
+{
+	return Random(-1.0, 1.0);
+}
+
+enum class WaveForm
+{
+	Saw, Sin, Square, Noise,
+};
+
+static constexpr uint32 SamplingFreq = Wave::DefaultSampleRate;
+static constexpr uint32 MaxFreq = SamplingFreq / 2;
+
+class Oscillator
+{
+public:
+
+	double get(double t, double freq, WaveForm waveForm) const
+	{
+		switch (waveForm)
+		{
+		case WaveForm::Saw:
+			return WaveSaw(freq * t * 2_pi, static_cast<int>(MaxFreq / freq));
+		case WaveForm::Sin:
+			return sin(freq * t * 2_pi);
+		case WaveForm::Square:
+			return WaveSquare(freq * t * 2_pi, static_cast<int>((MaxFreq + freq) / (freq * 2.0)));
+		case WaveForm::Noise:
+			return WaveNoise();
+		default: return 0;
+		}
+	}
+};
+
 const auto SliderHeight = 36;
 const auto SliderWidth = 400;
 const auto LabelWidth = 200;
@@ -22,6 +92,15 @@ struct ADSRConfig
 	}
 };
 
+bool SliderInt(const String& label, int& value, double min, double max, const Vec2& pos, double labelWidth = 80.0, double sliderWidth = 120.0, bool enabled = true)
+{
+	static std::unordered_map<int*, double> val;
+	val[&value] = value;
+	const bool result = SimpleGUI::Slider(label, val[&value], min, max, pos, labelWidth, sliderWidth, enabled);
+	value = static_cast<int>(Math::Round(val[&value]));
+	return result;
+}
+
 class EnvGenerator
 {
 public:
@@ -145,7 +224,8 @@ public:
 			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
 			const auto frequency = NoteNumberToFrequency(noteNumber);
 
-			const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * envLevel);
+			const auto osc = m_oscillator.get(m_time, frequency, static_cast<WaveForm>(m_oscIndex));
+			const auto w = static_cast<float>(osc * envLevel);
 			sample.left += w;
 			sample.right += w;
 		}
@@ -182,6 +262,7 @@ public:
 	void updateGUI(Vec2& pos)
 	{
 		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(m_amplitude), m_amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+		SliderInt(U"oscillator : {}"_fmt(m_oscIndex), m_oscIndex, 0, 3, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
 
 		m_adsr.updateGUI(pos);
 	}
@@ -198,6 +279,8 @@ private:
 	ADSRConfig m_adsr;
 
 	double m_amplitude = 0.1;
+	Oscillator m_oscillator;
+	int m_oscIndex = 0;
 
 	double m_time = 0;
 };
@@ -251,6 +334,7 @@ Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
 
 void Main()
 {
+	// 注意：波形生成に数秒かかります。example/midi/test.mid を渡すと長すぎて返って来ないので注意
 	auto midiDataOpt = LoadMidi(U"short_loop.mid");
 	if (!midiDataOpt)
 	{
```
### ２．オシレータをウェーブテーブル化する
```diff
@@ -51,25 +51,62 @@ enum class WaveForm
 static constexpr uint32 SamplingFreq = Wave::DefaultSampleRate;
 static constexpr uint32 MaxFreq = SamplingFreq / 2;
 
-class Oscillator
+class OscillatorWavetable
 {
 public:
 
-	double get(double t, double freq, WaveForm waveForm) const
+	OscillatorWavetable() = default;
+
+	OscillatorWavetable(size_t resolution, double frequency, WaveForm waveType) :
+		m_wave(resolution)
 	{
-		switch (waveForm)
+		const int mSaw = static_cast<int>(MaxFreq / frequency);
+		const int mSquare = static_cast<int>((MaxFreq + frequency) / (frequency * 2.0));
+
+		for (size_t i = 0; i < resolution; ++i)
 		{
-		case WaveForm::Saw:
-			return WaveSaw(freq * t * 2_pi, static_cast<int>(MaxFreq / freq));
-		case WaveForm::Sin:
-			return sin(freq * t * 2_pi);
-		case WaveForm::Square:
-			return WaveSquare(freq * t * 2_pi, static_cast<int>((MaxFreq + freq) / (freq * 2.0)));
-		case WaveForm::Noise:
-			return WaveNoise();
-		default: return 0;
+			const double angle = 2_pi * i / resolution;
+
+			switch (waveType)
+			{
+			case WaveForm::Saw:
+				m_wave[i] = static_cast<float>(WaveSaw(angle, mSaw));
+				break;
+			case WaveForm::Sin:
+				m_wave[i] = static_cast<float>(sin(angle));
+				break;
+			case WaveForm::Square:
+				m_wave[i] = static_cast<float>(WaveSquare(angle, mSquare));
+				break;
+			case WaveForm::Noise:
+				m_wave[i] = static_cast<float>(WaveNoise());
+				break;
+			default: break;
+			}
 		}
 	}
+
+	double get(double x) const
+	{
+		const size_t resolution = m_wave.size();
+		const double indexFloat = fmod(x * resolution / 2_pi, resolution);
+		const int indexInt = static_cast<int>(indexFloat);
+		const double rate = indexFloat - indexInt;
+		return Math::Lerp(m_wave[indexInt], m_wave[(indexInt + 1) % resolution], rate);
+	}
+
+private:
+
+	Array<float> m_wave;
+};
+
+// とりあえず440Hzで生成
+static Array<OscillatorWavetable> OscWaveTables =
+{
+	OscillatorWavetable(2048, 440, WaveForm::Saw),
+	OscillatorWavetable(2048, 440, WaveForm::Sin),
+	OscillatorWavetable(2048, 440, WaveForm::Square),
+	OscillatorWavetable(SamplingFreq, 440, WaveForm::Noise),
 };
 
 const auto SliderHeight = 36;
@@ -206,7 +243,7 @@ public:
 	// 1サンプル波形を生成して返す
 	WaveSample renderSample()
 	{
-		const auto deltaT = 1.0 / Wave::DefaultSampleRate;
+		const auto deltaT = 1.0 / SamplingFreq;
 
 		// エンベロープの更新
 		for (auto& [noteNumber, noteState] : m_noteState)
@@ -224,7 +261,7 @@ public:
 			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
 			const auto frequency = NoteNumberToFrequency(noteNumber);
 
-			const auto osc = m_oscillator.get(m_time, frequency, static_cast<WaveForm>(m_oscIndex));
+			const auto osc = OscWaveTables[m_oscIndex].get(m_time * frequency * 2_pi);
 			const auto w = static_cast<float>(osc * envLevel);
 			sample.left += w;
 			sample.right += w;
@@ -279,7 +316,6 @@ private:
 	ADSRConfig m_adsr;
 
 	double m_amplitude = 0.1;
-	Oscillator m_oscillator;
 	int m_oscIndex = 0;
 
 	double m_time = 0;
@@ -334,7 +370,6 @@ Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
 
 void Main()
 {
-	// 注意：波形生成に数秒かかります。example/midi/test.mid を渡すと長すぎて返って来ないので注意
 	auto midiDataOpt = LoadMidi(U"short_loop.mid");
 	if (!midiDataOpt)
 	{
```
### ３．帯域制限付きウェーブテーブルを実装する
```diff
@@ -49,6 +49,7 @@ enum class WaveForm
 };
 
 static constexpr uint32 SamplingFreq = Wave::DefaultSampleRate;
+static constexpr uint32 MinFreq = 20;
 static constexpr uint32 MaxFreq = SamplingFreq / 2;
 
 class OscillatorWavetable
@@ -100,13 +101,59 @@ private:
 	Array<float> m_wave;
 };
 
-// とりあえず440Hzで生成
-static Array<OscillatorWavetable> OscWaveTables =
+class BandLimitedWaveTables
 {
-	OscillatorWavetable(2048, 440, WaveForm::Saw),
-	OscillatorWavetable(2048, 440, WaveForm::Sin),
-	OscillatorWavetable(2048, 440, WaveForm::Square),
-	OscillatorWavetable(SamplingFreq, 440, WaveForm::Noise),
+public:
+
+	BandLimitedWaveTables() = default;
+
+	BandLimitedWaveTables(size_t tableCount, size_t waveResolution, WaveForm waveType)
+	{
+		m_waveTables.reserve(tableCount);
+		m_tableFreqs.reserve(tableCount);
+
+		for (size_t i = 0; i < tableCount; ++i)
+		{
+			const double rate = 1.0 * i / tableCount;
+			const double freq = pow(2, Math::Lerp(m_minFreqLog, m_maxFreqLog, rate));
+
+			m_waveTables.emplace_back(waveResolution, freq, waveType);
+			m_tableFreqs.push_back(static_cast<float>(freq));
+		}
+	}
+
+	double get(double x, double freq) const
+	{
+		const auto nextIt = std::upper_bound(m_tableFreqs.begin(), m_tableFreqs.end(), freq);
+		const auto nextIndex = std::distance(m_tableFreqs.begin(), nextIt);
+		if (nextIndex == 0)
+		{
+			return m_waveTables.front().get(x);
+		}
+		if (static_cast<size_t>(nextIndex) == m_tableFreqs.size())
+		{
+			return m_waveTables.back().get(x);
+		}
+
+		const auto prevIndex = nextIndex - 1;
+		const auto rate = Math::InvLerp(m_tableFreqs[prevIndex], m_tableFreqs[nextIndex], freq);
+		return Math::Lerp(m_waveTables[prevIndex].get(x), m_waveTables[nextIndex].get(x), rate);
+	}
+
+private:
+
+	double m_minFreqLog = log2(MinFreq);
+	double m_maxFreqLog = log2(MaxFreq);
+	Array<OscillatorWavetable> m_waveTables;
+	Array<float> m_tableFreqs;
+};
+
+static Array<BandLimitedWaveTables> OscWaveTables =
+{
+	BandLimitedWaveTables(80, 2048, WaveForm::Saw),
+	BandLimitedWaveTables(1, 2048, WaveForm::Sin),
+	BandLimitedWaveTables(80, 2048, WaveForm::Square),
+	BandLimitedWaveTables(1, SamplingFreq, WaveForm::Noise),
 };
 
 const auto SliderHeight = 36;
@@ -261,7 +308,7 @@ public:
 			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
 			const auto frequency = NoteNumberToFrequency(noteNumber);
 
-			const auto osc = OscWaveTables[m_oscIndex].get(m_time * frequency * 2_pi);
+			const auto osc = OscWaveTables[m_oscIndex].get(m_time * frequency * 2_pi, frequency);
 			const auto w = static_cast<float>(osc * envLevel);
 			sample.left += w;
 			sample.right += w;
@@ -309,6 +356,11 @@ public:
 		m_noteState.clear();
 	}
 
+	ADSRConfig& adsr()
+	{
+		return m_adsr;
+	}
+
 private:
 
 	std::multimap<int8_t, NoteState> m_noteState;
@@ -370,7 +422,8 @@ Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
 
 void Main()
 {
-	auto midiDataOpt = LoadMidi(U"short_loop.mid");
+	Window::Resize(1280, 720);
+	auto midiDataOpt = LoadMidi(U"C1_B8.mid");
 	if (!midiDataOpt)
 	{
 		// ファイルが見つからない or 読み込みエラー
@@ -380,17 +433,20 @@ void Main()
 	const MidiData& midiData = midiDataOpt.value();
 
 	Synthesizer synth;
+	auto& adsr = synth.adsr();
+	adsr.attackTime = 0.02;
+	adsr.releaseTime = 0.02;
 
 	Audio audio(RenderWave(synth, midiData));
 	audio.play();
 
+	AudioVisualizer visualizer(Scene::Rect().stretched(-50), AudioVisualizer::Spectrogram, AudioVisualizer::LogScale);
+	visualizer.setFreqRange(100, 20000); // [100, 20000] Hz
+	visualizer.setSplRange(-120, -60); // [-120, -60] dB
+
 	while (System::Update())
 	{
-		Vec2 pos(20, 20 - SliderHeight);
-
-		synth.updateGUI(pos);
-
-		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
+		if (KeySpace.down())
 		{
 			audio.stop();
 			synth.clear();
@@ -398,5 +454,9 @@ void Main()
 			audio = Audio(RenderWave(synth, midiData));
 			audio.play();
 		}
+
+		visualizer.setInputWave(audio);
+		visualizer.updateFFT();
+		visualizer.draw();
 	}
 }
```
### ４．波形生成をリアルタイムに行う
```diff
@@ -373,90 +373,94 @@ private:
 	double m_time = 0;
 };
 
-Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
+class AudioRenderer : public IAudioStream
 {
-	const auto lengthOfSamples = static_cast<int64>(ceil(midiData.lengthOfTime() * Wave::DefaultSampleRate));
+public:
 
-	Wave wave(lengthOfSamples);
+	void setMidiData(const MidiData& midiData)
+	{
+		m_midiData = midiData;
+	}
 
-	for (int64 i = 0; i < lengthOfSamples; ++i)
+	void updateGUI(Vec2& pos)
 	{
-		const auto currentTime = 1.0 * i / wave.sampleRate();
-		const auto nextTime = 1.0 * (i + 1) / wave.sampleRate();
+		m_synth.updateGUI(pos);
+	}
 
-		const auto currentTick = midiData.secondsToTicks(currentTime);
-		const auto nextTick = midiData.secondsToTicks(nextTime);
+private:
 
-		// tick が進んだら MIDI イベントの処理を更新する
-		if (currentTick != nextTick)
+	void getAudio(float* left, float* right, const size_t samplesToWrite) override
+	{
+		for (size_t i = 0; i < samplesToWrite; ++i)
 		{
-			for (const auto& track : midiData.tracks())
-			{
-				if (track.isPercussionTrack())
-				{
-					continue;
-				}
+			const double currentTime = 1.0 * m_readMIDIPos / SamplingFreq;
+			const double nextTime = 1.0 * (m_readMIDIPos + 1) / SamplingFreq;
 
-				// 発生したノートオンイベントをシンセに登録
-				const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
-				for (auto& [tick, noteOn] : noteOnEvents)
-				{
-					synth.noteOn(noteOn.note_number, noteOn.velocity);
-				}
+			const auto currentTick = m_midiData.secondsToTicks(currentTime);
+			const auto nextTick = m_midiData.secondsToTicks(nextTime);
 
-				// 発生したノートオフイベントをシンセに登録
-				const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
-				for (auto& [tick, noteOff] : noteOffEvents)
+			// tick が進んだら MIDI イベントの処理を更新する
+			if (currentTick != nextTick)
+			{
+				for (const auto& track : m_midiData.tracks())
 				{
-					synth.noteOff(noteOff.note_number);
+					if (track.isPercussionTrack())
+					{
+						continue;
+					}
+
+					// 発生したノートオンイベントをシンセに登録
+					const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
+					for (auto& [tick, noteOn] : noteOnEvents)
+					{
+						m_synth.noteOn(noteOn.note_number, noteOn.velocity);
+					}
+
+					// 発生したノートオフイベントをシンセに登録
+					const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
+					for (auto& [tick, noteOff] : noteOffEvents)
+					{
+						m_synth.noteOff(noteOff.note_number);
+					}
 				}
 			}
-		}
 
-		// シンセを1サンプル更新して波形を書き込む
-		wave[i] = synth.renderSample();
+			const auto waveSample = m_synth.renderSample();
+
+			*left++ = waveSample.left;
+			*right++ = waveSample.right;
+
+			++m_readMIDIPos;
+		}
 	}
 
-	return wave;
-}
+	bool hasEnded() override { return false; }
+	void rewind() override {}
+
+	Synthesizer m_synth;
+	MidiData m_midiData;
+	size_t m_readMIDIPos = 0;
+};
 
 void Main()
 {
-	Window::Resize(1280, 720);
-	auto midiDataOpt = LoadMidi(U"C1_B8.mid");
+	auto midiDataOpt = LoadMidi(U"example/midi/test.mid");
 	if (!midiDataOpt)
 	{
 		// ファイルが見つからない or 読み込みエラー
 		return;
 	}
 
-	const MidiData& midiData = midiDataOpt.value();
-
-	Synthesizer synth;
-	auto& adsr = synth.adsr();
-	adsr.attackTime = 0.02;
-	adsr.releaseTime = 0.02;
+	std::shared_ptr<AudioRenderer> audioStream = std::make_shared<AudioRenderer>();
+	audioStream->setMidiData(midiDataOpt.value());
 
-	Audio audio(RenderWave(synth, midiData));
+	Audio audio(audioStream);
 	audio.play();
 
-	AudioVisualizer visualizer(Scene::Rect().stretched(-50), AudioVisualizer::Spectrogram, AudioVisualizer::LogScale);
-	visualizer.setFreqRange(100, 20000); // [100, 20000] Hz
-	visualizer.setSplRange(-120, -60); // [-120, -60] dB
-
 	while (System::Update())
 	{
-		if (KeySpace.down())
-		{
-			audio.stop();
-			synth.clear();
-
-			audio = Audio(RenderWave(synth, midiData));
-			audio.play();
-		}
+		Vec2 pos(20, 20 - SliderHeight);
 
-		visualizer.setInputWave(audio);
-		visualizer.updateFFT();
-		visualizer.draw();
+		audioStream->updateGUI(pos);
 	}
 }
```
### ５．パフォーマンスをもう少し最適化する
```diff
@@ -59,7 +59,8 @@ public:
 	OscillatorWavetable() = default;
 
 	OscillatorWavetable(size_t resolution, double frequency, WaveForm waveType) :
-		m_wave(resolution)
+		m_wave(resolution),
+		m_xToIndex(resolution / 2_pi)
 	{
 		const int mSaw = static_cast<int>(MaxFreq / frequency);
 		const int mSquare = static_cast<int>((MaxFreq + frequency) / (frequency * 2.0));
@@ -89,16 +90,26 @@ public:
 
 	double get(double x) const
 	{
-		const size_t resolution = m_wave.size();
-		const double indexFloat = fmod(x * resolution / 2_pi, resolution);
-		const int indexInt = static_cast<int>(indexFloat);
-		const double rate = indexFloat - indexInt;
-		return Math::Lerp(m_wave[indexInt], m_wave[(indexInt + 1) % resolution], rate);
+		auto indexFloat = x * m_xToIndex;
+		auto prevIndex = static_cast<size_t>(indexFloat);
+		if (m_wave.size() == prevIndex)
+		{
+			prevIndex -= m_wave.size();
+			indexFloat -= m_wave.size();
+		}
+		auto nextIndex = prevIndex + 1;
+		if (m_wave.size() == nextIndex)
+		{
+			nextIndex = 0;
+		}
+		const auto x01 = indexFloat - prevIndex;
+		return Math::Lerp(m_wave[prevIndex], m_wave[nextIndex], x01);
 	}
 
 private:
 
 	Array<float> m_wave;
+	double m_xToIndex = 0;
 };
 
 class BandLimitedWaveTables
@@ -120,12 +131,22 @@ public:
 			m_waveTables.emplace_back(waveResolution, freq, waveType);
 			m_tableFreqs.push_back(static_cast<float>(freq));
 		}
+
+		{
+			m_indices.resize(2048);
+			m_freqToIndex = m_indices.size() / (1.0 * MaxFreq);
+			for (int i = 0; i < m_indices.size(); ++i)
+			{
+				const float freq = static_cast<float>(i / m_freqToIndex);
+				const auto nextIt = std::upper_bound(m_tableFreqs.begin(), m_tableFreqs.end(), freq);
+				m_indices[i] = static_cast<uint32>(nextIt - m_tableFreqs.begin());
+			}
+		}
 	}
 
 	double get(double x, double freq) const
 	{
-		const auto nextIt = std::upper_bound(m_tableFreqs.begin(), m_tableFreqs.end(), freq);
-		const auto nextIndex = std::distance(m_tableFreqs.begin(), nextIt);
+		const auto nextIndex = m_indices[static_cast<int>(freq * m_freqToIndex)];
 		if (nextIndex == 0)
 		{
 			return m_waveTables.front().get(x);
@@ -146,6 +167,9 @@ private:
 	double m_maxFreqLog = log2(MaxFreq);
 	Array<OscillatorWavetable> m_waveTables;
 	Array<float> m_tableFreqs;
+
+	Array<uint32> m_indices;
+	double m_freqToIndex = 0;
 };
 
 static Array<BandLimitedWaveTables> OscWaveTables =
@@ -279,6 +303,7 @@ float NoteNumberToFrequency(int8_t d)
 
 struct NoteState
 {
+	double m_phase = 0;
 	float m_velocity = 1.f;
 	EnvGenerator m_envelope;
 };
@@ -308,14 +333,18 @@ public:
 			const auto envLevel = noteState.m_envelope.currentLevel() * noteState.m_velocity;
 			const auto frequency = NoteNumberToFrequency(noteNumber);
 
-			const auto osc = OscWaveTables[m_oscIndex].get(m_time * frequency * 2_pi, frequency);
+			const auto osc = OscWaveTables[m_oscIndex].get(noteState.m_phase, frequency);
+			noteState.m_phase += deltaT * frequency * 2_pi;
+			if (Math::TwoPi < noteState.m_phase)
+			{
+				noteState.m_phase -= Math::TwoPi;
+			}
+
 			const auto w = static_cast<float>(osc * envLevel);
 			sample.left += w;
 			sample.right += w;
 		}
 
-		m_time += deltaT;
-
 		return sample * static_cast<float>(m_amplitude);
 	}
 
@@ -369,69 +398,89 @@ private:
 
 	double m_amplitude = 0.1;
 	int m_oscIndex = 0;
-
-	double m_time = 0;
 };
 
 class AudioRenderer : public IAudioStream
 {
 public:
 
+	AudioRenderer()
+	{
+		// 100ms分のバッファを確保する
+		const size_t bufferSize = SamplingFreq / 10;
+		m_buffer.resize(bufferSize);
+	}
+
 	void setMidiData(const MidiData& midiData)
 	{
 		m_midiData = midiData;
 	}
 
-	void updateGUI(Vec2& pos)
+	void bufferSample()
 	{
-		m_synth.updateGUI(pos);
-	}
+		const double currentTime = 1.0 * m_readMIDIPos / SamplingFreq;
+		const double nextTime = 1.0 * (m_readMIDIPos + 1) / SamplingFreq;
 
-private:
+		const auto currentTick = m_midiData.secondsToTicks(currentTime);
+		const auto nextTick = m_midiData.secondsToTicks(nextTime);
 
-	void getAudio(float* left, float* right, const size_t samplesToWrite) override
-	{
-		for (size_t i = 0; i < samplesToWrite; ++i)
+		// tick が進んだら MIDI イベントの処理を更新する
+		if (currentTick != nextTick)
 		{
-			const double currentTime = 1.0 * m_readMIDIPos / SamplingFreq;
-			const double nextTime = 1.0 * (m_readMIDIPos + 1) / SamplingFreq;
+			for (const auto& track : m_midiData.tracks())
+			{
+				if (track.isPercussionTrack())
+				{
+					continue;
+				}
 
-			const auto currentTick = m_midiData.secondsToTicks(currentTime);
-			const auto nextTick = m_midiData.secondsToTicks(nextTime);
+				// 発生したノートオンイベントをシンセに登録
+				const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
+				for (auto& [tick, noteOn] : noteOnEvents)
+				{
+					m_synth.noteOn(noteOn.note_number, noteOn.velocity);
+				}
 
-			// tick が進んだら MIDI イベントの処理を更新する
-			if (currentTick != nextTick)
-			{
-				for (const auto& track : m_midiData.tracks())
+				// 発生したノートオフイベントをシンセに登録
+				const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
+				for (auto& [tick, noteOff] : noteOffEvents)
 				{
-					if (track.isPercussionTrack())
-					{
-						continue;
-					}
-
-					// 発生したノートオンイベントをシンセに登録
-					const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
-					for (auto& [tick, noteOn] : noteOnEvents)
-					{
-						m_synth.noteOn(noteOn.note_number, noteOn.velocity);
-					}
-
-					// 発生したノートオフイベントをシンセに登録
-					const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
-					for (auto& [tick, noteOff] : noteOffEvents)
-					{
-						m_synth.noteOff(noteOff.note_number);
-					}
+					m_synth.noteOff(noteOff.note_number);
 				}
 			}
+		}
+
+		const size_t writeIndex = m_bufferWritePos % m_buffer.size();
+
+		m_buffer[writeIndex] = m_synth.renderSample();
+
+		++m_bufferWritePos;
+		++m_readMIDIPos;
+	}
 
-			const auto waveSample = m_synth.renderSample();
+	bool bufferCompleted() const
+	{
+		return m_bufferReadPos + m_buffer.size() - 1 < m_bufferWritePos;
+	}
 
-			*left++ = waveSample.left;
-			*right++ = waveSample.right;
+	void updateGUI(Vec2& pos)
+	{
+		m_synth.updateGUI(pos);
+	}
+
+private:
+
+	void getAudio(float* left, float* right, const size_t samplesToWrite) override
+	{
+		for (size_t i = 0; i < samplesToWrite; ++i)
+		{
+			const auto& readSample = m_buffer[(m_bufferReadPos + i) % m_buffer.size()];
 
-			++m_readMIDIPos;
+			*left++ = readSample.left;
+			*right++ = readSample.right;
 		}
+
+		m_bufferReadPos += samplesToWrite;
 	}
 
 	bool hasEnded() override { return false; }
@@ -439,7 +488,10 @@ private:
 
 	Synthesizer m_synth;
 	MidiData m_midiData;
+	Array<WaveSample> m_buffer;
 	size_t m_readMIDIPos = 0;
+	size_t m_bufferReadPos = 0;
+	size_t m_bufferWritePos = 0;
 };
 
 void Main()
@@ -454,6 +506,23 @@ void Main()
 	std::shared_ptr<AudioRenderer> audioStream = std::make_shared<AudioRenderer>();
 	audioStream->setMidiData(midiDataOpt.value());
 
+	bool isRunning = true;
+
+	auto renderUpdate = [&]()
+	{
+		while (isRunning)
+		{
+			while (!audioStream->bufferCompleted())
+			{
+				audioStream->bufferSample();
+			}
+
+			std::this_thread::sleep_for(std::chrono::milliseconds(1));
+		}
+	};
+
+	std::thread audioRenderThread(renderUpdate);
+
 	Audio audio(audioStream);
 	audio.play();
 
@@ -463,4 +532,7 @@ void Main()
 
 		audioStream->updateGUI(pos);
 	}
+
+	isRunning = false;
+	audioRenderThread.join();
 }
```
