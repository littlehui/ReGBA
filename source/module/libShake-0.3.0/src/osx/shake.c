/* libShake - a basic haptic library */

#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>

#include "shake.h"
#include "./shake_private.h"
#include "../common/helpers.h"
#include "../common/error.h"

listElement *listHead;
unsigned int numOfDevices;

/* Prototypes */

Shake_Status Shake_Probe(Shake_Device *dev);
Shake_Status Shake_Query(Shake_Device *dev);
int convertMagnitude(int magnitude);

int convertMagnitude(int magnitude)
{
	return ((float)magnitude/0x7FFF) * FF_FFNOMINALMAX;
}

/* Public functions */

Shake_Status Shake_Init()
{
	IOReturn ret;
	io_iterator_t iter;
	CFDictionaryRef match;
	io_service_t device;

	match = IOServiceMatching(kIOHIDDeviceKey);

	if (!match)
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}

	ret = IOServiceGetMatchingServices(kIOMasterPortDefault, match, &iter);

	if (ret != kIOReturnSuccess)
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}

	if (!IOIteratorIsValid(iter))
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}

	while ((device = IOIteratorNext(iter)) != IO_OBJECT_NULL)
	{
		Shake_Device dev;
		dev.service = device;
		dev.effectList = NULL;

		if (Shake_Probe(&dev) == SHAKE_OK)
		{
			dev.id = numOfDevices;
			listHead = listElementPrepend(listHead);
			listHead->item = malloc(sizeof(Shake_Device));
			memcpy(listHead->item, &dev, sizeof(Shake_Device));
			++numOfDevices;
		}
		else
		{
			IOObjectRelease(device);
		}
	}

	IOObjectRelease(iter);

	return SHAKE_OK;
}

void Shake_Quit()
{
	if (listHead != NULL)
	{
		listElement *curElem = listHead;

		while(curElem)
		{
			Shake_Device *dev;
			listElement *toDelElem = curElem;
			curElem = curElem->next;
			dev = (Shake_Device *)toDelElem->item;

			Shake_Close(dev);
			if (dev->service)
				IOObjectRelease(dev->service);
		}

		listElementDeleteAll(listHead);
	}
}

int Shake_NumOfDevices()
{
	return numOfDevices;
}

Shake_Status Shake_Probe(Shake_Device *dev)
{
	if ((FFIsForceFeedback(dev->service)) != FF_OK)
	{
		return SHAKE_ERROR;
	}

	if (Shake_Query(dev))
	{
		return SHAKE_ERROR;
	}

	return SHAKE_OK;
}

Shake_Device *Shake_Open(unsigned int id)
{
	HRESULT result;
	Shake_Device *dev;

	if (id >= numOfDevices)
	{
		Shake_EmitErrorCode(SHAKE_EC_ARG);
		return NULL;
	}

	listElement *element = listElementGet(listHead, numOfDevices - 1 - id);
	dev = (Shake_Device *)element->item;

	if(!dev || !dev->service)
	{
		Shake_EmitErrorCode(SHAKE_EC_DEVICE);
		return NULL;
	}

	result = FFCreateDevice(dev->service, &dev->device);

	if (result != FF_OK)
	{
		Shake_EmitErrorCode(SHAKE_EC_DEVICE);
		return NULL;
	}

	return dev;
}

Shake_Status Shake_Query(Shake_Device *dev)
{
	HRESULT result;
	io_name_t deviceName;

	if(!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);
	if (!dev->service)
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	result = FFCreateDevice(dev->service, &dev->device);
	if (result != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}

	result = FFDeviceGetForceFeedbackCapabilities(dev->device, &dev->features);
	if (result != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_QUERY);
	}

	if (!dev->features.supportedEffects) /* Device doesn't support any force feedback effects. Ignore it. */
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}

	dev->capacity = dev->features.storageCapacity;

	if (dev->capacity <= 0) /* Device doesn't support uploading effects. Ignore it. */
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	IORegistryEntryGetName(dev->service, deviceName); /* Get device name */
	if (strlen((char *)deviceName))
	{
		strncpy(dev->name, (char *)deviceName, sizeof(dev->name));
	}
	else
	{
		strncpy(dev->name, "Unknown", sizeof(dev->name));
	}

	if (FFReleaseDevice(dev->device) == FF_OK)
	{
		dev->device = 0;
	}

	return SHAKE_OK;
}

int Shake_DeviceId(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->id : SHAKE_ERROR;
}

const char *Shake_DeviceName(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->name : NULL;
}

int Shake_DeviceEffectCapacity(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->capacity : SHAKE_ERROR;
}

Shake_Bool Shake_QueryEffectSupport(Shake_Device *dev, Shake_EffectType type)
{
	FFCapabilitiesEffectType query;

	switch (type)
	{
		case SHAKE_EFFECT_RUMBLE:
			/* Emulate EFFECT_RUMBLE with EFFECT_PERIODIC. */
			return Shake_QueryWaveformSupport(dev, SHAKE_PERIODIC_SINE) ? SHAKE_TRUE : SHAKE_FALSE;
		break;
		case SHAKE_EFFECT_PERIODIC:
		{
			Shake_PeriodicWaveform waveform;

			for (waveform = SHAKE_PERIODIC_SQUARE; waveform < SHAKE_PERIODIC_COUNT; ++waveform)
			{
				if (Shake_QueryWaveformSupport(dev, waveform))
					return SHAKE_TRUE;
			}

			return SHAKE_FALSE;
		}
		break;
		case SHAKE_EFFECT_CONSTANT:
			query = FFCAP_ET_CONSTANTFORCE;
		break;
		case SHAKE_EFFECT_SPRING:
			query = FFCAP_ET_SPRING;
		break;
		case SHAKE_EFFECT_FRICTION:
			query = FFCAP_ET_FRICTION;
		break;
		case SHAKE_EFFECT_DAMPER:
			query = FFCAP_ET_DAMPER;
		break;
		case SHAKE_EFFECT_INERTIA:
			query = FFCAP_ET_INERTIA;
		break;
		case SHAKE_EFFECT_RAMP:
			query = FFCAP_ET_RAMPFORCE;
		break;

		default:
		return SHAKE_FALSE;
	}

	return test_bit(query, dev->features.supportedEffects) ? SHAKE_TRUE : SHAKE_FALSE;
}

Shake_Bool Shake_QueryWaveformSupport(Shake_Device *dev, Shake_PeriodicWaveform waveform)
{
	FFCapabilitiesEffectType query;

	switch (waveform)
	{
		case SHAKE_PERIODIC_SQUARE:
			query = FFCAP_ET_SQUARE;
		break;
		case SHAKE_PERIODIC_TRIANGLE:
			query = FFCAP_ET_TRIANGLE;
		break;
		case SHAKE_PERIODIC_SINE:
			query = FFCAP_ET_SINE;
		break;
		case SHAKE_PERIODIC_SAW_UP:
			query = FFCAP_ET_SAWTOOTHUP;
		break;
		case SHAKE_PERIODIC_SAW_DOWN:
			query = FFCAP_ET_SAWTOOTHDOWN;
		break;
		case SHAKE_PERIODIC_CUSTOM:
			query = FFCAP_ET_CUSTOMFORCE;
		break;

		default:
		return SHAKE_FALSE;
	}

	return test_bit(query, dev->features.supportedEffects) ? SHAKE_TRUE : SHAKE_FALSE;
}

Shake_Bool Shake_QueryGainSupport(Shake_Device *dev)
{
	HRESULT result;
	int value = 0; /* Unused for now. */

	result = FFDeviceGetForceFeedbackProperty(dev->device, FFPROP_FFGAIN, &value, sizeof(value));

	if (result == FF_OK)
	{
		return SHAKE_TRUE;
	}
	else if (result != FFERR_UNSUPPORTED)
	{
		Shake_EmitErrorCode(SHAKE_EC_QUERY);
	}

	return SHAKE_FALSE;
}

Shake_Bool Shake_QueryAutocenterSupport(Shake_Device *dev)
{
	HRESULT result;
	int value = 0; /* Unused for now. */

	result = FFDeviceGetForceFeedbackProperty(dev->device, FFPROP_AUTOCENTER, &value, sizeof(value));

	if (result == FF_OK)
	{
		return SHAKE_TRUE;
	}
	else if (result != FFERR_UNSUPPORTED)
	{
		Shake_EmitErrorCode(SHAKE_EC_QUERY);
	}

	return SHAKE_FALSE;
}

Shake_Status Shake_SetGain(Shake_Device *dev, int gain)
{
	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (gain < 0)
		gain = 0;
	if (gain > 100)
		gain = 100;

	gain = ((float)gain/100) * FF_FFNOMINALMAX;

	if (FFDeviceSetForceFeedbackProperty(dev->device, FFPROP_FFGAIN, &gain) != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return SHAKE_OK;
}

Shake_Status Shake_SetAutocenter(Shake_Device *dev, int autocenter)
{
	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (autocenter) /* OSX supports only OFF and ON values */
	{
		autocenter = 1;
	}

	if (FFDeviceSetForceFeedbackProperty(dev->device, FFPROP_AUTOCENTER, &autocenter) != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return SHAKE_OK;
}

Shake_Status Shake_InitEffect(Shake_Effect *effect, Shake_EffectType type)
{
	if (!effect || type >= SHAKE_EFFECT_COUNT)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	memset(effect, 0, sizeof(*effect));
	effect->type = type;
	effect->id = -1;

	return SHAKE_OK;
}

int Shake_UploadEffect(Shake_Device *dev, Shake_Effect *effect)
{
	HRESULT result;
	FFEFFECT e;
	CFUUIDRef effectType;
	effectContainer *container = NULL;
	DWORD rgdwAxes[2];
	LONG rglDirection[2];

	if (!dev || !effect || effect->id < -1)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	rgdwAxes[0] = FFJOFS_X;
	rgdwAxes[1] = FFJOFS_Y;
	rglDirection[0] = effect->direction;
	rglDirection[1] = 0;

	/* Common parameters. */
	memset(&e, 0, sizeof(e));
	e.dwSize = sizeof(FFEFFECT);
	e.dwFlags = FFEFF_POLAR | FFEFF_OBJECTOFFSETS;
	e.dwDuration = effect->length * 1000;
	e.dwSamplePeriod = 0;
	e.cAxes = 2;
	e.rgdwAxes = rgdwAxes;
	e.rglDirection = rglDirection;
	e.dwStartDelay = effect->delay;
	e.dwTriggerButton = FFEB_NOTRIGGER;
	e.dwTriggerRepeatInterval = 0;

	e.dwGain = FF_FFNOMINALMAX;

	/* Effect type specific parameters. */
	if(effect->type == SHAKE_EFFECT_RUMBLE)
	{
		/* Emulate EFFECT_RUMBLE with EFFECT_PERIODIC. */
		int magnitude;
		FFPERIODIC pf;

		/*
		 * The magnitude is calculated as average of
		 * 2/3 of strongMagnitude and 1/3 of weakMagnitude.
		 * This follows the same ratios as in the Linux kernel.
		 */
		magnitude = effect->u.rumble.strongMagnitude/3 + effect->u.rumble.weakMagnitude/6;

		if (magnitude > SHAKE_PERIODIC_MAGNITUDE_MAX)
		{
			magnitude = SHAKE_PERIODIC_MAGNITUDE_MAX;
		}

		pf.dwMagnitude = convertMagnitude(magnitude);
		pf.lOffset = 0;
		pf.dwPhase = 0;
		pf.dwPeriod = 50 * 1000; /* Magic number from the Linux kernel implementation. */

		effectType = kFFEffectType_Sine_ID;
		e.lpEnvelope = malloc(sizeof(FFENVELOPE));
		e.lpEnvelope->dwSize = sizeof(FFENVELOPE);
		e.lpEnvelope->dwAttackTime = 0;
		e.lpEnvelope->dwAttackLevel = 0;
		e.lpEnvelope->dwFadeTime = 0;
		e.lpEnvelope->dwFadeLevel = 0;

		e.cbTypeSpecificParams = sizeof(FFPERIODIC);
		e.lpvTypeSpecificParams = &pf;
	}
	else if(effect->type == SHAKE_EFFECT_PERIODIC)
	{
		FFPERIODIC pf;

		switch (effect->u.periodic.waveform)
		{
			case SHAKE_PERIODIC_SQUARE:
				effectType = kFFEffectType_Square_ID;
			break;
			case SHAKE_PERIODIC_TRIANGLE:
				effectType = kFFEffectType_Triangle_ID;
			break;
			case SHAKE_PERIODIC_SINE:
				effectType = kFFEffectType_Sine_ID;
			break;
			case SHAKE_PERIODIC_SAW_UP:
				effectType = kFFEffectType_SawtoothUp_ID;
			break;
			case SHAKE_PERIODIC_SAW_DOWN:
				effectType = kFFEffectType_SawtoothDown_ID;
			break;
			case SHAKE_PERIODIC_CUSTOM:
				effectType = kFFEffectType_CustomForce_ID;
			break;

			default:
			return Shake_EmitErrorCode(SHAKE_EC_SUPPORT);
		}

		pf.dwMagnitude = convertMagnitude(effect->u.periodic.magnitude);
		pf.lOffset = convertMagnitude(effect->u.periodic.offset);
		pf.dwPhase = ((float)effect->u.periodic.phase/SHAKE_PERIODIC_PHASE_MAX) * OSX_PERIODIC_PHASE_MAX;
		pf.dwPeriod = effect->u.periodic.period * 1000;

		e.lpEnvelope = malloc(sizeof(FFENVELOPE));
		e.lpEnvelope->dwSize = sizeof(FFENVELOPE);
		e.lpEnvelope->dwAttackTime = effect->u.periodic.envelope.attackLength * 1000;
		e.lpEnvelope->dwAttackLevel = effect->u.periodic.envelope.attackLevel;
		e.lpEnvelope->dwFadeTime = effect->u.periodic.envelope.fadeLength * 1000;
		e.lpEnvelope->dwFadeLevel = effect->u.periodic.envelope.fadeLevel;

		e.cbTypeSpecificParams = sizeof(FFPERIODIC);
		e.lpvTypeSpecificParams = &pf;
	}
	else if(effect->type == SHAKE_EFFECT_CONSTANT)
	{
		FFCONSTANTFORCE cf;

		cf.lMagnitude = convertMagnitude(effect->u.constant.level);

		effectType = kFFEffectType_ConstantForce_ID;
		e.lpEnvelope = malloc(sizeof(FFENVELOPE));
		e.lpEnvelope->dwSize = sizeof(FFENVELOPE);
		e.lpEnvelope->dwAttackTime = effect->u.constant.envelope.attackLength * 1000;
		e.lpEnvelope->dwAttackLevel = effect->u.constant.envelope.attackLevel;
		e.lpEnvelope->dwFadeTime = effect->u.constant.envelope.fadeLength * 1000;
		e.lpEnvelope->dwFadeLevel = effect->u.constant.envelope.fadeLevel;

		e.cbTypeSpecificParams = sizeof(FFCONSTANTFORCE);
		e.lpvTypeSpecificParams = &cf;
	}
	else if(effect->type == SHAKE_EFFECT_RAMP)
	{
		FFRAMPFORCE rf;

		rf.lStart = ((float)effect->u.ramp.startLevel/SHAKE_RAMP_START_LEVEL_MAX) * FF_FFNOMINALMAX;
		rf.lEnd = ((float)effect->u.ramp.endLevel/SHAKE_RAMP_END_LEVEL_MAX) * FF_FFNOMINALMAX;

		effectType = kFFEffectType_RampForce_ID;
		e.lpEnvelope = malloc(sizeof(FFENVELOPE));
		e.lpEnvelope->dwSize = sizeof(FFENVELOPE);
		e.lpEnvelope->dwAttackTime = effect->u.ramp.envelope.attackLength * 1000;
		e.lpEnvelope->dwAttackLevel = effect->u.ramp.envelope.attackLevel;
		e.lpEnvelope->dwFadeTime = effect->u.ramp.envelope.fadeLength * 1000;
		e.lpEnvelope->dwFadeLevel = effect->u.ramp.envelope.fadeLevel;

		e.cbTypeSpecificParams = sizeof(FFRAMPFORCE);
		e.lpvTypeSpecificParams = &rf;
	}
	else
	{
		return (effect->type >= SHAKE_EFFECT_COUNT ? Shake_EmitErrorCode(SHAKE_EC_ARG) : Shake_EmitErrorCode(SHAKE_EC_SUPPORT));
	}

	if (effect->id == -1) /* Create a new effect. */
	{
		dev->effectList = listElementPrepend(dev->effectList);
		dev->effectList->item = malloc(sizeof(effectContainer));
		container = dev->effectList->item;
		container->id = listLength(dev->effectList) - 1;
		container->effect = 0;

		result = FFDeviceCreateEffect(dev->device, effectType, &e, &container->effect);

		if ((unsigned int)result != FF_OK)
		{
			free(e.lpEnvelope);
			dev->effectList = listElementDelete(dev->effectList, dev->effectList);
			return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
		}
	}
	else /* Update existing effect. */
	{
		listElement *node = dev->effectList;
		effectContainer *item;

		while (node)
		{
			item = (effectContainer *)node->item;
			if (item->id == effect->id)
			{
				container = item;
				break;
			}

			node = node->next;
		}

		if (container)
		{
			int flags = FFEP_AXES | FFEP_DIRECTION | FFEP_DURATION | FFEP_ENVELOPE | FFEP_GAIN | FFEP_SAMPLEPERIOD | FFEP_STARTDELAY | FFEP_TRIGGERBUTTON | FFEP_TYPESPECIFICPARAMS;

			result = FFEffectSetParameters(container->effect, &e, flags);

			if ((unsigned int)result != FF_OK)
			{
				free(e.lpEnvelope);
				return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
			}
		}
	}

	free(e.lpEnvelope);

	return container ? container->id : Shake_EmitErrorCode(SHAKE_EC_EFFECT);
}

Shake_Status Shake_EraseEffect(Shake_Device *dev, int id)
{
	listElement *node;
	effectContainer *effect = NULL;

	if(!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	node = dev->effectList;

	while (node)
	{
		effect = (effectContainer *)node->item;
		if (effect->id == id)
		{
			break;
		}

		node = node->next;
	}

	if (!node || !effect)
	{
		return Shake_EmitErrorCode(SHAKE_EC_EFFECT);
	}

	if (FFDeviceReleaseEffect(dev->device, effect->effect))
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	dev->effectList = listElementDelete(dev->effectList, node);

	return SHAKE_OK;
}

Shake_Status Shake_Play(Shake_Device *dev, int id)
{
	listElement *node;
	effectContainer *effect = NULL;

	if(!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	node = dev->effectList;

	while (node)
	{
		effect = (effectContainer *)node->item;
		if (effect->id == id)
		{
			break;
		}

		node = node->next;
	}

	if (!node || !effect)
	{
		return Shake_EmitErrorCode(SHAKE_EC_EFFECT);
	}

	if (FFEffectStart(effect->effect, 1, 0) != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return SHAKE_OK;
}

Shake_Status Shake_Stop(Shake_Device *dev, int id)
{
	listElement *node;
	effectContainer *effect = NULL;

	if(!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	node = dev->effectList;

	while (node)
	{
		effect = (effectContainer *)node->item;
		if (effect->id == id)
		{
			break;
		}

		node = node->next;
	}

	if (!node || !effect)
	{
		return Shake_EmitErrorCode(SHAKE_EC_EFFECT);
	}

	if (FFEffectStop(effect->effect))
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return SHAKE_OK;
}

Shake_Status Shake_Close(Shake_Device *dev)
{
	int effectLen;
	int i;

	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);
	if (!dev->device)
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	effectLen = listLength(dev->effectList);

	for (i = 0; i < effectLen; ++i)
	{
		effectContainer *effect = (effectContainer *)listElementGet(dev->effectList, i);
		if (FFDeviceReleaseEffect(dev->device, effect->effect))
		{
			return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
		}
	}

	dev->effectList = listElementDeleteAll(dev->effectList);
	if (FFReleaseDevice(dev->device) != FF_OK)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	dev->device = 0;

	return SHAKE_OK;
}
