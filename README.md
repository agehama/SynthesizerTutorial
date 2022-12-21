# SynthesizerTutorial
ソフトウェアシンセサイザーを作成するチュートリアルのソースコードです。

記事：  
https://qiita.com/agehama_/items/7da430491400e9a2b6a7

## ビルド環境
- Visual Studio 2022
- OpenSiv3D v0.6.6

## 各ステップの差分

### １．サイン波を再生する

```diff
@@ -0,0 +1,45 @@
+ # include <Siv3D.hpp> // OpenSiv3D v0.6.6
+
+const auto SliderHeight = 36;
+const auto SliderWidth = 400;
+const auto LabelWidth = 200;
+
+Wave RenderWave(uint32 seconds, double amplitude, double frequency)
+{
+       const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
+
+       Wave wave(lengthOfSamples);
+
+       for (uint32 i = 0; i < lengthOfSamples; ++i)
+       {
+               const double sec = 1.0f * i / Wave::DefaultSampleRate;
+               const double w = sin(Math::TwoPiF * frequency * sec) * amplitude;
+               wave[i].left = wave[i].right = static_cast<float>(w);
+       }
+
+       return wave;
+}
+
+void Main()
+{
+       double amplitude = 0.2;
+       double frequency = 440.0;
+
+       uint32 seconds = 3;
+
+       Audio audio(RenderWave(seconds, amplitude, frequency));
+       audio.play();
+
+       while (System::Update())
+       {
+               Vec2 pos(20, 20 - SliderHeight);
+               SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+               SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+
+               if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
+               {
+                       audio = Audio(RenderWave(seconds, amplitude, frequency));
+                       audio.play();
+               }
+       }
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
+       double attackTime = 0.01;
+       double decayTime = 0.01;
+       double sustainLevel = 0.6;
+       double releaseTime = 0.4;
+
+       void updateGUI(Vec2& pos)
+       {
+               SimpleGUI::Slider(U"attack : {:.2f}"_fmt(attackTime), attackTime, 0.0, 0.5, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+               SimpleGUI::Slider(U"decay : {:.2f}"_fmt(decayTime), decayTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+               SimpleGUI::Slider(U"sustain : {:.2f}"_fmt(sustainLevel), sustainLevel, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+               SimpleGUI::Slider(U"release : {:.2f}"_fmt(releaseTime), releaseTime, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+       }
+};
+
+class EnvGenerator
+{
+public:
+
+       enum class State
+       {
+               Attack, Decay, Sustain, Release
+       };
+
+       void noteOff()
+       {
+               if (m_state != State::Release)
+               {
+                       m_elapsed = 0;
+                       m_state = State::Release;
+               }
+       }
+
+       void reset(State state)
+       {
+               m_elapsed = 0;
+               m_state = state;
+       }
+
+       void update(const ADSRConfig& adsr, double dt)
+       {
+               switch (m_state)
+               {
+               case State::Attack: // 0.0 から 1.0 まで attackTime かけて増幅する
+                       if (m_elapsed < adsr.attackTime)
+                       {
+                               m_currentLevel = m_elapsed / adsr.attackTime;
+                               break;
+                       }
+                       m_elapsed -= adsr.attackTime;
+                       m_state = State::Decay;
+                       [[fallthrough]]; // Decay処理にそのまま続く
+
+               case State::Decay: // 1.0 から sustainLevel まで decayTime かけて減衰する
+                       if (m_elapsed < adsr.decayTime)
+                       {
+                               m_currentLevel = Math::Lerp(1.0, adsr.sustainLevel, m_elapsed / adsr.decayTime);
+                               break;
+                       }
+                       m_elapsed -= adsr.decayTime;
+                       m_state = State::Sustain;
+                       [[fallthrough]]; // Sustain処理にそのまま続く
+
+
+               case State::Sustain: // ノートオンの間 sustainLevel を維持する
+                       m_currentLevel = adsr.sustainLevel;
+                       break;
+
+               case State::Release: // sustainLevel から 0.0 まで releaseTime かけて減衰する
+                       m_currentLevel = m_elapsed < adsr.releaseTime
+                               ? Math::Lerp(adsr.sustainLevel, 0.0, m_elapsed / adsr.releaseTime)
+                               : 0.0;
+                       break;
+
+               default: break;
+               }
+
+               m_elapsed += dt;
+       }
+
+       bool isReleased(const ADSRConfig& adsr) const
+       {
+               return m_state == State::Release && adsr.releaseTime <= m_elapsed;
+       }
+
+       double currentLevel() const
+       {
+               return m_currentLevel;
+       }
+
+       State state() const
+       {
+               return m_state;
+       }
+
+private:
+
+       State m_state = State::Attack;
+       double m_elapsed = 0; // ステート変更からの経過秒数
+       double m_currentLevel = 0; // 現在のレベル [0, 1]
+};
+
+Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRConfig& adsr)
 {
        const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;

        Wave wave(lengthOfSamples);

+       // 0サンプル目でノートオン
+       EnvGenerator envelope;
+
+       // 半分経過したところでノートオフ
+       const auto noteOffSample = lengthOfSamples / 2;
+
+       const float deltaT = 1.0f / Wave::DefaultSampleRate;
+       float time = 0;
        for (uint32 i = 0; i < lengthOfSamples; ++i)
        {
-               const double sec = 1.0f * i / Wave::DefaultSampleRate;
-               const double w = sin(Math::TwoPiF * frequency * sec) * amplitude;
+               if (i == noteOffSample)
+               {
+                       envelope.noteOff();
+               }
+               const auto w = sin(Math::TwoPiF * frequency * time)
+                       * amplitude * envelope.currentLevel();
                wave[i].left = wave[i].right = static_cast<float>(w);
+               time += deltaT;
+               envelope.update(adsr, deltaT);
        }

        return wave;
@@ -27,7 +144,13 @@ void Main()

        uint32 seconds = 3;

-       Audio audio(RenderWave(seconds, amplitude, frequency));
+       ADSRConfig adsr;
+       adsr.attackTime = 0.1;
+       adsr.decayTime = 0.1;
+       adsr.sustainLevel = 0.8;
+       adsr.releaseTime = 0.5;
+
+       Audio audio(RenderWave(seconds, amplitude, frequency, adsr));
        audio.play();

        while (System::Update())
@@ -36,9 +159,11 @@ void Main()
                SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
                SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

+               adsr.updateGUI(pos);
+
                if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
                {
-                       audio = Audio(RenderWave(seconds, amplitude, frequency));
+                       audio = Audio(RenderWave(seconds, amplitude, frequency, adsr));
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
+       return 440.0f * pow(2.0f, (d - 69) / 12.0f);
+}
+
+Wave RenderWave(uint32 seconds, double amplitude, const Array<float>& frequencies, const ADSRConfig& adsr)
 {
        const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;

@@ -121,14 +126,22 @@ Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRCo

        const float deltaT = 1.0f / Wave::DefaultSampleRate;
        float time = 0;
+
        for (uint32 i = 0; i < lengthOfSamples; ++i)
        {
                if (i == noteOffSample)
                {
                        envelope.noteOff();
                }
-               const auto w = sin(Math::TwoPiF * frequency * time)
-                       * amplitude * envelope.currentLevel();
+
+               // 和音の各波形を加算合成する
+               double w = 0;
+               for (auto freq : frequencies)
+               {
+                       w += sin(Math::TwoPiF * freq * time)
+                               * amplitude * envelope.currentLevel();
+               }
+
                wave[i].left = wave[i].right = static_cast<float>(w);
                time += deltaT;
                envelope.update(adsr, deltaT);
@@ -140,7 +153,6 @@ Wave RenderWave(uint32 seconds, double amplitude, double frequency, const ADSRCo
 void Main()
 {
        double amplitude = 0.2;
-       double frequency = 440.0;

        uint32 seconds = 3;

@@ -150,20 +162,26 @@ void Main()
        adsr.sustainLevel = 0.8;
        adsr.releaseTime = 0.5;

-       Audio audio(RenderWave(seconds, amplitude, frequency, adsr));
+       const Array<float> frequencies =
+       {
+               NoteNumberToFrequency(60), // C_4
+               NoteNumberToFrequency(64), // E_4
+               NoteNumberToFrequency(67), // G_4
+       };
+
+       Audio audio(RenderWave(seconds, amplitude, frequencies, adsr));
        audio.play();

        while (System::Update())
        {
                Vec2 pos(20, 20 - SliderHeight);
                SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
-               SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

                adsr.updateGUI(pos);

                if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
                {
-                       audio = Audio(RenderWave(seconds, amplitude, frequency, adsr));
+                       audio = Audio(RenderWave(seconds, amplitude, frequencies, adsr));
                        audio.play();
                }
        }
```

### ４．シンセサイザーを定義する
```diff
@@ -112,39 +112,114 @@ float NoteNumberToFrequency(int8_t d)
        return 440.0f * pow(2.0f, (d - 69) / 12.0f);
 }

-Wave RenderWave(uint32 seconds, double amplitude, const Array<float>& frequencies, const ADSRConfig& adsr)
+struct NoteState
+{
+       EnvGenerator m_envelope;
+};
+
+class Synthesizer
+{
+public:
+
+       // 1サンプル波形を生成して返す
+       WaveSample renderSample()
+       {
+               const auto deltaT = 1.0 / Wave::DefaultSampleRate;
+
+               // エンベロープの更新
+               for (auto& [noteNumber, noteState] : m_noteState)
+               {
+                       noteState.m_envelope.update(m_adsr, deltaT);
+               }
+
+               // リリースが終了したノートを削除する
+               std::erase_if(m_noteState, [&](const auto& noteState) { return noteState.second.m_envelope.isReleased(m_adsr); });
+
+               // 入力中の波形を加算して書き込む
+               WaveSample sample(0, 0);
+               for (auto& [noteNumber, noteState] : m_noteState)
+               {
+                       const auto amplitude = noteState.m_envelope.currentLevel();
+                       const auto frequency = NoteNumberToFrequency(noteNumber);
+
+                       const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * amplitude);
+                       sample.left += w;
+                       sample.right += w;
+               }
+
+               m_time += deltaT;
+
+               return sample * static_cast<float>(m_amplitude);
+       }
+
+       void noteOn(int8_t noteNumber)
+       {
+               m_noteState.emplace(noteNumber, NoteState());
+       }
+
+       void noteOff(int8_t noteNumber)
+       {
+               auto [beginIt, endIt] = m_noteState.equal_range(noteNumber);
+
+               for (auto it = beginIt; it != endIt; ++it)
+               {
+                       auto& envelope = it->second.m_envelope;
+
+                       // noteOnになっている最初の要素をnoteOffにする
+                       if (envelope.state() != EnvGenerator::State::Release)
+                       {
+                               envelope.noteOff();
+                               break;
+                       }
+               }
+       }
+
+       void updateGUI(Vec2& pos)
+       {
+               SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(m_amplitude), m_amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
+
+               m_adsr.updateGUI(pos);
+       }
+
+       void clear()
+       {
+               m_noteState.clear();
+       }
+
+private:
+
+       std::multimap<int8_t, NoteState> m_noteState;
+
+       ADSRConfig m_adsr;
+
+       double m_amplitude = 0.2;
+
+       double m_time = 0;
+};
+
+Wave RenderWave(uint32 seconds, Synthesizer& synth)
 {
        const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;

        Wave wave(lengthOfSamples);

-       // 0サンプル目でノートオン
-       EnvGenerator envelope;
-
        // 半分経過したところでノートオフ
        const auto noteOffSample = lengthOfSamples / 2;

-       const float deltaT = 1.0f / Wave::DefaultSampleRate;
-       float time = 0;
+       synth.noteOn(60); // C_4
+       synth.noteOn(64); // E_4
+       synth.noteOn(67); // G_4

        for (uint32 i = 0; i < lengthOfSamples; ++i)
        {
                if (i == noteOffSample)
                {
-                       envelope.noteOff();
-               }
-
-               // 和音の各波形を加算合成する
-               double w = 0;
-               for (auto freq : frequencies)
-               {
-                       w += sin(Math::TwoPiF * freq * time)
-                               * amplitude * envelope.currentLevel();
+                       synth.noteOff(60);
+                       synth.noteOff(64);
+                       synth.noteOff(67);
                }

-               wave[i].left = wave[i].right = static_cast<float>(w);
-               time += deltaT;
-               envelope.update(adsr, deltaT);
+               wave[i] = synth.renderSample();
        }

        return wave;
@@ -152,36 +227,24 @@ Wave RenderWave(uint32 seconds, double amplitude, const Array<float>& frequencie

 void Main()
 {
-       double amplitude = 0.2;
-
        uint32 seconds = 3;

-       ADSRConfig adsr;
-       adsr.attackTime = 0.1;
-       adsr.decayTime = 0.1;
-       adsr.sustainLevel = 0.8;
-       adsr.releaseTime = 0.5;
+       Synthesizer synth;

-       const Array<float> frequencies =
-       {
-               NoteNumberToFrequency(60), // C_4
-               NoteNumberToFrequency(64), // E_4
-               NoteNumberToFrequency(67), // G_4
-       };
-
-       Audio audio(RenderWave(seconds, amplitude, frequencies, adsr));
+       Audio audio(RenderWave(seconds, synth));
        audio.play();

        while (System::Update())
        {
                Vec2 pos(20, 20 - SliderHeight);
-               SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

-               adsr.updateGUI(pos);
+               synth.updateGUI(pos);

                if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
                {
-                       audio = Audio(RenderWave(seconds, amplitude, frequencies, adsr));
+                       synth.clear();
+
+                       audio = Audio(RenderWave(seconds, synth));
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
+       float m_velocity;
        EnvGenerator m_envelope;
 };

@@ -139,7 +142,7 @@ public:
                WaveSample sample(0, 0);
                for (auto& [noteNumber, noteState] : m_noteState)
                {
-                       const auto amplitude = noteState.m_envelope.currentLevel();
+                       const auto amplitude = noteState.m_envelope.currentLevel() * noteState.m_velocity;
                        const auto frequency = NoteNumberToFrequency(noteNumber);

                        const auto w = static_cast<float>(sin(Math::TwoPiF * frequency * m_time) * amplitude);
@@ -152,9 +155,11 @@ public:
                return sample * static_cast<float>(m_amplitude);
        }

-       void noteOn(int8_t noteNumber)
+       void noteOn(int8_t noteNumber, int8_t velocity)
        {
-               m_noteState.emplace(noteNumber, NoteState());
+               NoteState noteState;
+               noteState.m_velocity = velocity / 127.0f;
+               m_noteState.emplace(noteNumber, noteState);
        }

        void noteOff(int8_t noteNumber)
@@ -192,33 +197,52 @@ private:

        ADSRConfig m_adsr;

-       double m_amplitude = 0.2;
+       double m_amplitude = 0.1;

        double m_time = 0;
 };

-Wave RenderWave(uint32 seconds, Synthesizer& synth)
+Wave RenderWave(Synthesizer& synth, const MidiData& midiData)
 {
-       const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;
+       const auto lengthOfSamples = static_cast<int64>(ceil(midiData.lengthOfTime() * Wave::DefaultSampleRate));

        Wave wave(lengthOfSamples);

-       // 半分経過したところでノートオフ
-       const auto noteOffSample = lengthOfSamples / 2;
+       for (int64 i = 0; i < lengthOfSamples; ++i)
+       {
+               const auto currentTime = 1.0 * i / wave.sampleRate();
+               const auto nextTime = 1.0 * (i + 1) / wave.sampleRate();

-       synth.noteOn(60); // C_4
-       synth.noteOn(64); // E_4
-       synth.noteOn(67); // G_4
+               const auto currentTick = midiData.secondsToTicks(currentTime);
+               const auto nextTick = midiData.secondsToTicks(nextTime);

-       for (uint32 i = 0; i < lengthOfSamples; ++i)
-       {
-               if (i == noteOffSample)
+               // tick が進んだら MIDI イベントの処理を更新する
+               if (currentTick != nextTick)
                {
-                       synth.noteOff(60);
-                       synth.noteOff(64);
-                       synth.noteOff(67);
+                       for (const auto& track : midiData.tracks())
+                       {
+                               if (track.isPercussionTrack())
+                               {
+                                       continue;
+                               }
+
+                               // 発生したノートオンイベントをシンセに登録
+                               const auto noteOnEvents = track.getMIDIEvent<NoteOnEvent>(currentTick, nextTick);
+                               for (auto& [tick, noteOn] : noteOnEvents)
+                               {
+                                       synth.noteOn(noteOn.note_number, noteOn.velocity);
+                               }
+
+                               // 発生したノートオフイベントをシンセに登録
+                               const auto noteOffEvents = track.getMIDIEvent<NoteOffEvent>(currentTick, nextTick);
+                               for (auto& [tick, noteOff] : noteOffEvents)
+                               {
+                                       synth.noteOff(noteOff.note_number);
+                               }
+                       }
                }

+               // シンセを1サンプル更新して波形を書き込む
                wave[i] = synth.renderSample();
        }

@@ -227,11 +251,18 @@ Wave RenderWave(uint32 seconds, Synthesizer& synth)

 void Main()
 {
-       uint32 seconds = 3;
+       auto midiDataOpt = LoadMidi(U"short_loop.mid");
+       if (!midiDataOpt)
+       {
+               // ファイルが見つからない or 読み込みエラー
+               return;
+       }
+
+       const MidiData& midiData = midiDataOpt.value();

        Synthesizer synth;

-       Audio audio(RenderWave(seconds, synth));
+       Audio audio(RenderWave(synth, midiData));
        audio.play();

        while (System::Update())
@@ -242,9 +273,10 @@ void Main()

                if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
                {
+                       audio.stop();
                        synth.clear();

-                       audio = Audio(RenderWave(seconds, synth));
+                       audio = Audio(RenderWave(synth, midiData));
                        audio.play();
                }
        }
```
