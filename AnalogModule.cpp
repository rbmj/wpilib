/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2008. All Rights Reserved.							  */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in $(WIND_BASE)/WPILib.  */
/*----------------------------------------------------------------------------*/

#include "AnalogModule.h"
#include "Synchronized.h"
#include "Timer.h"
#include "WPIErrors.h"
#include "NetworkCommunication/AICalibration.h"

#if __cplusplus >= 201103L
constexpr long AnalogModule::kTimebase; ///< 40 MHz clock
constexpr long AnalogModule::kDefaultOversampleBits;
constexpr long AnalogModule::kDefaultAverageBits;
constexpr float AnalogModule::kDefaultSampleRate;
#else
const long AnalogModule::kTimebase; ///< 40 MHz clock
const long AnalogModule::kDefaultOversampleBits;
const long AnalogModule::kDefaultAverageBits;
const float AnalogModule::kDefaultSampleRate;
#endif
SEM_ID AnalogModule::m_registerWindowSemaphore = NULL;

/**
 * Get an instance of an Analog Module.
 * 
 * Singleton analog module creation where a module is allocated on the first use
 * and the same module is returned on subsequent uses.
 * 
 * @param moduleNumber The analog module to get (1 or 2).
 * @return A pointer to the AnalogModule.
 */
AnalogModule* AnalogModule::GetInstance(UINT8 moduleNumber)
{
	if (CheckAnalogModule(moduleNumber))
	{
		return (AnalogModule*)GetModule(nLoadOut::kModuleType_Analog, moduleNumber);
	}

	// If this wasn't caught before now, make sure we say what's wrong before we crash
	char buf[64];
	snprintf(buf, 64, "Analog Module %d", moduleNumber);
	wpi_setGlobalWPIErrorWithContext(ModuleIndexOutOfRange, buf);

	return NULL;
}

/**
 * Create a new instance of an analog module.
 * 
 * Create an instance of the analog module object. Initialize all the parameters
 * to reasonable values on start.
 * Setting a global value on an analog module can be done only once unless subsequent
 * values are set the previously set value.
 * Analog modules are a singleton, so the constructor is never called outside of this class.
 * 
 * @param moduleNumber The analog module to create (1 or 2).
 */
AnalogModule::AnalogModule(UINT8 moduleNumber)
	: Module(nLoadOut::kModuleType_Analog, moduleNumber)
	, m_module (NULL)
	, m_sampleRateSet (false)
	, m_numChannelsToActivate (0)
{
	AddToSingletonList();
	tRioStatusCode localStatus = NiFpga_Status_Success;
	m_module = tAI::create(m_moduleNumber - 1, &localStatus);
	wpi_setError(localStatus);
	SetNumChannelsToActivate(kAnalogChannels);
	SetSampleRate(kDefaultSampleRate);

	for (UINT32 i = 0; i < kAnalogChannels; i++)
	{
		m_module->writeScanList(i, i, &localStatus);
		wpi_setError(localStatus);
		SetAverageBits(i + 1, kDefaultAverageBits);
		SetOversampleBits(i + 1, kDefaultOversampleBits);
	}

	if (m_registerWindowSemaphore == NULL)
	{
		// Needs to be global since the protected resource spans both module singletons.
		m_registerWindowSemaphore = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	}
}

/**
 * Destructor for AnalogModule.
 */
AnalogModule::~AnalogModule()
{
	delete m_module;
}

/**
 * Set the sample rate on the module.
 * 
 * This is a global setting for the module and effects all channels.
 * 
 * @param samplesPerSecond The number of samples per channel per second.
 */
void AnalogModule::SetSampleRate(float samplesPerSecond)
{
	// TODO: This will change when variable size scan lists are implemented.
	// TODO: Need float comparison with epsilon.
	//wpi_assert(!sampleRateSet || GetSampleRate() == samplesPerSecond);
	m_sampleRateSet = true;

	// Compute the convert rate
	UINT32 ticksPerSample = (UINT32)((float)kTimebase / samplesPerSecond);
	UINT32 ticksPerConversion = ticksPerSample / GetNumChannelsToActivate();
	// ticksPerConversion must be at least 80
	if (ticksPerConversion < 80)
	{
		wpi_setWPIError(SampleRateTooHigh);
		ticksPerConversion = 80;
	}

	// Atomically set the scan size and the convert rate so that the sample rate is constant
	tAI::tConfig config;
	config.ScanSize = GetNumChannelsToActivate();
	config.ConvertRate = ticksPerConversion;
	tRioStatusCode localStatus = NiFpga_Status_Success;
	m_module->writeConfig(config, &localStatus);
	wpi_setError(localStatus);

	// Indicate that the scan size has been commited to hardware.
	SetNumChannelsToActivate(0);
}

/**
 * Get the current sample rate on the module.
 * 
 * This assumes one entry in the scan list.
 * This is a global setting for the module and effects all channels.
 * 
 * @return Sample rate.
 */
float AnalogModule::GetSampleRate()
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	UINT32 ticksPerConversion = m_module->readLoopTiming(&localStatus);
	wpi_setError(localStatus);
	UINT32 ticksPerSample = ticksPerConversion * GetNumActiveChannels();
	return (float)kTimebase / (float)ticksPerSample;
}

/**
 * Return the number of channels on the module in use.
 * 
 * @return Active channels.
 */
UINT32 AnalogModule::GetNumActiveChannels()
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	UINT32 scanSize = m_module->readConfig_ScanSize(&localStatus);
	wpi_setError(localStatus);
	if (scanSize == 0)
		return 8;
	return scanSize;
}

/**
 * Get the number of active channels.
 * 
 * This is an internal function to allow the atomic update of both the 
 * number of active channels and the sample rate.
 * 
 * When the number of channels changes, use the new value.  Otherwise,
 * return the curent value.
 * 
 * @return Value to write to the active channels field.
 */
UINT32 AnalogModule::GetNumChannelsToActivate()
{
	if(m_numChannelsToActivate == 0) return GetNumActiveChannels();
	return m_numChannelsToActivate;
}

/**
 * Set the number of active channels.
 * 
 * Store the number of active channels to set.  Don't actually commit to hardware
 * until SetSampleRate().
 * 
 * @param channels Number of active channels.
 */
void AnalogModule::SetNumChannelsToActivate(UINT32 channels)
{
	m_numChannelsToActivate = channels;
}

/**
 * Set the number of averaging bits.
 * 
 * This sets the number of averaging bits. The actual number of averaged samples is 2**bits.
 * Use averaging to improve the stability of your measurement at the expense of sampling rate.
 * The averaging is done automatically in the FPGA.
 * 
 * @param channel Analog channel to configure.
 * @param bits Number of bits to average.
 */
void AnalogModule::SetAverageBits(UINT32 channel, UINT32 bits)
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	m_module->writeAverageBits(channel - 1, bits, &localStatus);
	wpi_setError(localStatus);
}

/**
 * Get the number of averaging bits.
 * 
 * This gets the number of averaging bits from the FPGA. The actual number of averaged samples is 2**bits.
 * The averaging is done automatically in the FPGA.
 * 
 * @param channel Channel to address.
 * @return Bits to average.
 */
UINT32 AnalogModule::GetAverageBits(UINT32 channel)
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	UINT32 result = m_module->readAverageBits(channel - 1, &localStatus);
	wpi_setError(localStatus);
	return result;
}

/**
 * Set the number of oversample bits.
 * 
 * This sets the number of oversample bits. The actual number of oversampled values is 2**bits.
 * Use oversampling to improve the resolution of your measurements at the expense of sampling rate.
 * The oversampling is done automatically in the FPGA.
 * 
 * @param channel Analog channel to configure.
 * @param bits Number of bits to oversample.
 */
void AnalogModule::SetOversampleBits(UINT32 channel, UINT32 bits)
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	m_module->writeOversampleBits(channel - 1, bits, &localStatus);
	wpi_setError(localStatus);
}

/**
 * Get the number of oversample bits.
 * 
 * This gets the number of oversample bits from the FPGA. The actual number of oversampled values is
 * 2**bits. The oversampling is done automatically in the FPGA.
 * 
 * @param channel Channel to address.
 * @return Bits to oversample.
 */
UINT32 AnalogModule::GetOversampleBits(UINT32 channel)
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	UINT32 result = m_module->readOversampleBits(channel - 1, &localStatus);
	wpi_setError(localStatus);
	return result;
}

/**
 * Get a sample straight from the channel on this module.
 * 
 * The sample is a 12-bit value representing the -10V to 10V range of the A/D converter in the module.
 * The units are in A/D converter codes.  Use GetVoltage() to get the analog value in calibrated units.
 * 
 * @return A sample straight from the channel on this module.
 */
INT16 AnalogModule::GetValue(UINT32 channel)
{
	INT16 value;
	CheckAnalogChannel(channel);

	tAI::tReadSelect readSelect;
	readSelect.Channel = channel - 1;
	readSelect.Module = m_moduleNumber - 1;
	readSelect.Averaged = false;
	tRioStatusCode localStatus = NiFpga_Status_Success;

	{
		Synchronized sync(m_registerWindowSemaphore);
		m_module->writeReadSelect(readSelect, &localStatus);
		m_module->strobeLatchOutput(&localStatus);
		value = (INT16) m_module->readOutput(&localStatus);
	}

	wpi_setError(localStatus);
	return value;
}

/**
 * Get a sample from the output of the oversample and average engine for the channel.
 * 
 * The sample is 12-bit + the value configured in SetOversampleBits().
 * The value configured in SetAverageBits() will cause this value to be averaged 2**bits number of samples.
 * This is not a sliding window.  The sample will not change until 2**(OversamplBits + AverageBits) samples
 * have been acquired from the module on this channel.
 * Use GetAverageVoltage() to get the analog value in calibrated units.
 * 
 * @param channel Channel number to read.
 * @return A sample from the oversample and average engine for the channel.
 */
INT32 AnalogModule::GetAverageValue(UINT32 channel)
{
	INT32 value;
	CheckAnalogChannel(channel);

	tAI::tReadSelect readSelect;
	readSelect.Channel = channel - 1;
	readSelect.Module = m_moduleNumber - 1;
	readSelect.Averaged = true;
	tRioStatusCode localStatus = NiFpga_Status_Success;

	{
		Synchronized sync(m_registerWindowSemaphore);
		m_module->writeReadSelect(readSelect, &localStatus);
		m_module->strobeLatchOutput(&localStatus);
		value = m_module->readOutput(&localStatus);
	}

	wpi_setError(localStatus);
	return value;
}

/**
 * Convert a voltage to a raw value for a specified channel.
 * 
 * This process depends on the calibration of each channel, so the channel
 * must be specified.
 * 
 * @todo This assumes raw values.  Oversampling not supported as is.
 * 
 * @param channel The channel to convert for.
 * @param voltage The voltage to convert.
 * @return The raw value for the channel.
 */
INT32 AnalogModule::VoltsToValue(INT32 channel, float voltage)
{
	if (voltage > 10.0)
	{
		voltage = 10.0;
		wpi_setWPIError(VoltageOutOfRange);
	}
	if (voltage < -10.0)
	{
		voltage = -10.0;
		wpi_setWPIError(VoltageOutOfRange);
	}
	UINT32 LSBWeight = GetLSBWeight(channel);
	INT32 offset = GetOffset(channel);
	INT32 value = (INT32) ((voltage + offset * 1.0e-9) / (LSBWeight * 1.0e-9));
	return value;
}

/**
 * Get a scaled sample straight from the channel on this module.
 * 
 * The value is scaled to units of Volts using the calibrated scaling data from GetLSBWeight() and GetOffset().
 * 
 * @param channel The channel to read.
 * @return A scaled sample straight from the channel on this module.
 */
float AnalogModule::GetVoltage(UINT32 channel)
{
	INT16 value = GetValue(channel);
	UINT32 LSBWeight = GetLSBWeight(channel);
	INT32 offset = GetOffset(channel);
	float voltage = LSBWeight * 1.0e-9 * value - offset * 1.0e-9;
	return voltage;
}

/**
 * Get a scaled sample from the output of the oversample and average engine for the channel.
 * 
 * The value is scaled to units of Volts using the calibrated scaling data from GetLSBWeight() and GetOffset().
 * Using oversampling will cause this value to be higher resolution, but it will update more slowly.
 * Using averaging will cause this value to be more stable, but it will update more slowly.
 * 
 * @param channel The channel to read.
 * @return A scaled sample from the output of the oversample and average engine for the channel.
 */
float AnalogModule::GetAverageVoltage(UINT32 channel)
{
	INT32 value = GetAverageValue(channel);
	UINT32 LSBWeight = GetLSBWeight(channel);
	INT32 offset = GetOffset(channel);
	UINT32 oversampleBits = GetOversampleBits(channel);
	float voltage = ((LSBWeight * 1.0e-9 * value) / (float)(1 << oversampleBits)) - offset * 1.0e-9;
	return voltage;
}

/**
 * Get the factory scaling least significant bit weight constant.
 * The least significant bit weight constant for the channel that was calibrated in
 * manufacturing and stored in an eeprom in the module.
 * 
 * Volts = ((LSB_Weight * 1e-9) * raw) - (Offset * 1e-9)
 * 
 * @param channel The channel to get calibration data for.
 * @return Least significant bit weight.
 */
UINT32 AnalogModule::GetLSBWeight(UINT32 channel) 
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	UINT32 lsbWeight = FRC_NetworkCommunication_nAICalibration_getLSBWeight(m_module->getSystemIndex(), channel - 1, (INT32*)&localStatus);
	wpi_setError(localStatus);
	return lsbWeight;
}

/**
 * Get the factory scaling offset constant.
 * The offset constant for the channel that was calibrated in manufacturing and stored
 * in an eeprom in the module.
 * 
 * Volts = ((LSB_Weight * 1e-9) * raw) - (Offset * 1e-9)
 * 
 * @param channel The channel to get calibration data for.
 * @return Offset constant.
 */
INT32 AnalogModule::GetOffset(UINT32 channel)
{
	tRioStatusCode localStatus = NiFpga_Status_Success;
	INT32 offset = FRC_NetworkCommunication_nAICalibration_getOffset(m_module->getSystemIndex(), channel - 1, (INT32*)&localStatus);
	wpi_setError(localStatus);
	return offset;
}


