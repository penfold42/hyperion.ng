/*
A bit of theory of operations....

This driver assumes:
- dma capable SPI driver that can send the request in ONE HIT.
- a stable SPI clock (which on the Ras Pi requires a stable core clock)

From the ws2812 spec sheet typical values:
A '0' is signalled as a high/low pulse time of 350ns/800ns
A '1' is signalled as a high/low pulse time of 700ns/600ns
A reset/latch is signalled as a low signal for 50,000ns

The spi bit rate combined with the bitpair_to_byte table set this timing
At 3,000,000 bit/second (333nS per bit):
0: 1000 = high/low for 333nS/999nS
1: 1100 = high/low for 666nS/666nS

The reset pulse is sent as SPI_FRAME_END_LATCH_BYTES (3 bytes = 24) bits of zeros
At 3,000,000 bit/second (333nS per bit) = 8000nS
This is well below spec, but seems to work well with ws2812 and sk6812w leds

If you want to be within the specs, you could change it:
ws2812	50,000ns	(150bits = 19 bytes)
ws2813 300,000ns	(900buits = 113 bytes)
sk6812	80,000ns	(240bits = 80 bytes)

*/

#include "LedDeviceWs2812SPI.h"

LedDeviceWs2812SPI::LedDeviceWs2812SPI(const QJsonObject &deviceConfig)
	: ProviderSpi()
	, SPI_BYTES_PER_COLOUR(4)
	, bitpair_to_byte {
		0b10001000,
		0b10001100,
		0b11001000,
		0b11001100,
	}
{
	_deviceReady = init(deviceConfig);
}

LedDevice* LedDeviceWs2812SPI::construct(const QJsonObject &deviceConfig)
{
	return new LedDeviceWs2812SPI(deviceConfig);
}

bool LedDeviceWs2812SPI::init(const QJsonObject &deviceConfig)
{
	_baudRate_Hz = 3000000;
	if ( !ProviderSpi::init(deviceConfig) )
	{
		return false;
	}
	WarningIf(( _baudRate_Hz < 2050000 || _baudRate_Hz > 4000000 ), _log, "SPI rate %d outside recommended range (2050000 -> 4000000)", _baudRate_Hz);

	const int SPI_FRAME_END_LATCH_BYTES = 3;
	// 2 * SPI_FRAME_END_LATCH_BYTES because we'll also do a reset at the start
	_ledBuffer.resize(_ledRGBCount * SPI_BYTES_PER_COLOUR + 2*SPI_FRAME_END_LATCH_BYTES, 0x00);

	return true;
}

int LedDeviceWs2812SPI::write(const std::vector<ColorRgb> &ledValues)
{
	unsigned spi_ptr = SPI_FRAME_END_LATCH_BYTES;
	// SPI_FRAME_END_LATCH_BYTES because we'll also do a reset at the start
	const int SPI_BYTES_PER_LED = sizeof(ColorRgb) * SPI_BYTES_PER_COLOUR;

	for (const ColorRgb& color : ledValues)
	{
		uint32_t colorBits = ((unsigned int)color.red << 16)
			| ((unsigned int)color.green << 8)
			| color.blue;

		for (int j=SPI_BYTES_PER_LED - 1; j>=0; j--)
		{
			_ledBuffer[spi_ptr+j] = bitpair_to_byte[ colorBits & 0x3 ];
			colorBits >>= 2;
		}
		spi_ptr += SPI_BYTES_PER_LED;
	}

	_ledBuffer[spi_ptr++] = 0;
	_ledBuffer[spi_ptr++] = 0;
	_ledBuffer[spi_ptr++] = 0;

	return writeBytes(_ledBuffer.size(), _ledBuffer.data());
}
