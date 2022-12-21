# include <Siv3D.hpp> // OpenSiv3D v0.6.6

const auto SliderHeight = 36;
const auto SliderWidth = 400;
const auto LabelWidth = 200;

Wave RenderWave(uint32 seconds, double amplitude, double frequency)
{
	const auto lengthOfSamples = seconds * Wave::DefaultSampleRate;

	Wave wave(lengthOfSamples);

	for (uint32 i = 0; i < lengthOfSamples; ++i)
	{
		const double sec = 1.0f * i / Wave::DefaultSampleRate;
		const double w = sin(Math::TwoPiF * frequency * sec) * amplitude;
		wave[i].left = wave[i].right = static_cast<float>(w);
	}

	return wave;
}

void Main()
{
	double amplitude = 0.2;
	double frequency = 440.0;

	uint32 seconds = 3;

	Audio audio(RenderWave(seconds, amplitude, frequency));
	audio.play();

	while (System::Update())
	{
		Vec2 pos(20, 20 - SliderHeight);
		SimpleGUI::Slider(U"amplitude : {:.2f}"_fmt(amplitude), amplitude, 0.0, 1.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);
		SimpleGUI::Slider(U"frequency : {:.0f}"_fmt(frequency), frequency, 100.0, 1000.0, Vec2{ pos.x, pos.y += SliderHeight }, LabelWidth, SliderWidth);

		if (SimpleGUI::Button(U"波形を再生成", Vec2{ pos.x, pos.y += SliderHeight }))
		{
			audio = Audio(RenderWave(seconds, amplitude, frequency));
			audio.play();
		}
	}
}
