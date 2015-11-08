#include "global.h"
#include "InputHandler_Linux_Event.h"
#include "RageLog.h"
#include "RageUtil.h"

#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>

REGISTER_INPUT_HANDLER_CLASS2( Event, Linux_Event );

bool InputHandler_Linux_Event::m_bFoundAnyJoysticks;

static RString BustypeToString( int iBus )
{
	switch( iBus )
	{
//	case BUS_ADB:
//	case BUS_AMIGA: return "amiga input";
	case BUS_BLUETOOTH: return "Bluetooth";
	case BUS_GAMEPORT: return "gameport";
//	case BUS_HIL:
//	case BUS_HOST:
//	case BUS_I2C:
	case BUS_I8042: return "keyboard";
	case BUS_ISA: return "ISA";
	case BUS_ISAPNP: return "ISAPNP";
	case BUS_PARPORT: return "parallel port";
	case BUS_PCI: return "PCI";
	case BUS_RS232: return "serial port";
	case BUS_USB: return "USB";
	case BUS_XTKBD: return "XT keyboard";
	default: return ssprintf("unknown bus %x", iBus);
	}
}

struct EventDevice
{
	EventDevice();
	~EventDevice();
	bool Open( RString sFile, InputDevice dev );
	bool IsOpen() const { return m_iFD != -1; }
	void Close()
	{
		if( m_iFD != -1 )
			close( m_iFD );
		m_iFD = -1;
	}

	int m_iFD;
	RString m_sPath;
	RString m_sName;
	InputDevice m_Dev;

	int aiAbsMin[ABS_MAX];
	int aiAbsMax[ABS_MAX];
	DeviceButton aiAbsMappingHigh[ABS_MAX];
	DeviceButton aiAbsMappingLow[ABS_MAX];
};

static vector<EventDevice *> g_apEventDevices;

/* Return true if the numbered event device exists.  sysfs may not always be
 * there; return false if we don't know. */
static bool EventDeviceExists( int iNum )
{
	RString sDir = ssprintf( "/sys/class" );
	struct stat st;
	if( stat(sDir, &st) == -1 )
		return true;

	RString sFile = ssprintf( "/sys/class/input/event%i", iNum );
	return stat(sFile, &st) == 0;
}

static bool BitIsSet( const uint8_t *pArray, uint32_t iBit )
{
	return !!(pArray[iBit/8] & (1<<(iBit%8)));
}

EventDevice::EventDevice()
{
	m_iFD = -1;
}

bool EventDevice::Open( RString sFile, InputDevice dev )
{
	m_sPath = sFile;
	m_Dev = dev;
	m_iFD = open( sFile, O_RDWR );
	if( m_iFD == -1 )
	{
		if( errno == ENODEV )
			return false;

		if( !EventDeviceExists(m_iFD) )
			return false;

		LOG->Warn( "Error opening %s: %s", sFile.c_str(), strerror(errno) );
		return false;
	}

	static bool bLogged = false;
	if( !bLogged )
	{
		bLogged = true;
		int iVersion;
		if( ioctl(m_iFD, EVIOCGVERSION, &iVersion) == -1 )
			LOG->Warn( "ioctl(EVIOCGVERSION): %s", strerror(errno) );
		else
			LOG->Info( "Event driver: v%i.%i.%i", (iVersion >> 16) & 0xFF, (iVersion >> 8) & 0xFF, iVersion & 0xFF ); 
	}

	char szName[1024];
	if( ioctl(m_iFD, EVIOCGNAME(sizeof(szName)), szName) == -1 )
	{
		LOG->Warn( "ioctl(EVIOCGNAME): %s", strerror(errno) );
		
		m_sName = "(unknown)";
	}
	else
	{
		m_sName = szName;
	}

	input_id DevInfo;
	if( ioctl(m_iFD, EVIOCGID, &DevInfo) == -1 )
	{
		LOG->Warn( "ioctl(EVIOCGID): %s", strerror(errno) );
	}
	else
	{
		LOG->Info( "Input device: %s: %s device, ID %04x:%04x, version %x: %s", sFile.c_str(),
			BustypeToString(DevInfo.bustype).c_str(), DevInfo.vendor, DevInfo.product,
			DevInfo.version, m_sName.c_str() );
	}

	uint8_t iABSMask[ABS_MAX/8 + 1];
	memset( iABSMask, 0, sizeof(iABSMask) );
	if( ioctl(m_iFD, EVIOCGBIT(EV_ABS, sizeof(iABSMask)), iABSMask) < 0 )
		LOG->Warn( "ioctl(EVIOCGBIT(EV_ABS)): %s", strerror(errno) );

	if( !BitIsSet(iABSMask, ABS_X) && !BitIsSet(iABSMask, ABS_THROTTLE) && !BitIsSet(iABSMask, ABS_WHEEL) )
	{
		LOG->Info( "    Not a joystick; ignored" );
		Close();
		return false;
	}

	uint8_t iKeyMask[KEY_MAX/8 + 1];
	memset( iKeyMask, 0, sizeof(iKeyMask) );
	if( ioctl(m_iFD, EVIOCGBIT(EV_KEY, sizeof(iKeyMask)), iKeyMask) < 0 )
		LOG->Warn( "ioctl(EVIOCGBIT(EV_KEY)): %s", strerror(errno) );

	uint8_t iEventTypes[EV_MAX/8];
	memset( iEventTypes, 0, sizeof(iEventTypes) );
	if( ioctl(m_iFD, EVIOCGBIT(0, EV_MAX), iEventTypes) == -1 )
		LOG->Warn( "ioctl(EV_MAX): %s", strerror(errno) );

	{
		vector<RString> setEventTypes;

		if( BitIsSet(iEventTypes, EV_SYN) )		setEventTypes.push_back( "syn" );
		if( BitIsSet(iEventTypes, EV_KEY) )		setEventTypes.push_back( "key" );
		if( BitIsSet(iEventTypes, EV_REL) )		setEventTypes.push_back( "rel" );
		if( BitIsSet(iEventTypes, EV_ABS) )		setEventTypes.push_back( "abs" );
		if( BitIsSet(iEventTypes, EV_MSC) )		setEventTypes.push_back( "misc" );
		if( BitIsSet(iEventTypes, EV_SW) )		setEventTypes.push_back( "sw" );
		if( BitIsSet(iEventTypes, EV_LED) )		setEventTypes.push_back( "led" );
		if( BitIsSet(iEventTypes, EV_SND) )		setEventTypes.push_back( "snd" );
		if( BitIsSet(iEventTypes, EV_REP) )		setEventTypes.push_back( "rep" );
		if( BitIsSet(iEventTypes, EV_FF) )		setEventTypes.push_back( "ff" );
		if( BitIsSet(iEventTypes, EV_PWR) )		setEventTypes.push_back( "pwr" );
		if( BitIsSet(iEventTypes, EV_FF_STATUS) )	setEventTypes.push_back( "ff_status" );

		LOG->Info( "    Event types: %s", join(", ", setEventTypes).c_str() );
	}
	
	int iTotalKeys = 0;
	for( int i = 0; i < KEY_MAX; ++i )
	{
		if( !BitIsSet(iKeyMask, i) )
			continue;
		++iTotalKeys;
	}

	int iTotalAxes = 0;
	const DeviceButton iExtraAxes[] = { JOY_LEFT_2, JOY_UP_2, JOY_AUX_1, JOY_AUX_3 };
	int iNextExtraAxis = 0;
	for( int i = 0; i < ABS_MAX; ++i )
	{
		if( !BitIsSet(iABSMask, i) )
			continue;
		struct input_absinfo absinfo;
		if( ioctl(m_iFD, EVIOCGABS(i), &absinfo) < 0 )
		{
			LOG->Warn( "ioctl(EVIOCGABS): %s", strerror(errno) );
			continue;
		}

		//LOG->Info( "    Axis %i: min: %i; max: %i; fuzz: %i; flat: %i",
		//		i, absinfo.minimum, absinfo.maximum, absinfo.fuzz, absinfo.flat );
		aiAbsMin[i] = absinfo.minimum;
		aiAbsMax[i] = absinfo.maximum;
		aiAbsMappingHigh[i] = enum_add2(JOY_RIGHT, 2*i);
		aiAbsMappingLow[i] = enum_add2(JOY_LEFT, 2*i);

		if( i == ABS_X )
		{
			aiAbsMappingHigh[i] = JOY_RIGHT;
			aiAbsMappingLow[i] = JOY_LEFT;
		}
		else if( i == ABS_Y )
		{
			aiAbsMappingHigh[i] = JOY_DOWN;
			aiAbsMappingLow[i] = JOY_UP;
		}
		else if( i == ABS_Z )
		{
			aiAbsMappingHigh[i] = JOY_Z_DOWN;
			aiAbsMappingLow[i] = JOY_Z_UP;
		}
		else if( i == ABS_RX )
		{
			aiAbsMappingHigh[i] = JOY_ROT_RIGHT;
			aiAbsMappingLow[i] = JOY_ROT_LEFT;
		}
		else if( i == ABS_RY )
		{
			aiAbsMappingHigh[i] = JOY_ROT_DOWN;
			aiAbsMappingLow[i] = JOY_ROT_UP;
		}
		else if( i == ABS_RZ )
		{
			aiAbsMappingHigh[i] = JOY_ROT_Z_DOWN;
			aiAbsMappingLow[i] = JOY_ROT_Z_UP;
		}
		else if( i == ABS_HAT0X )
		{
			aiAbsMappingHigh[i] = JOY_HAT_RIGHT;
			aiAbsMappingLow[i] = JOY_HAT_LEFT;
		}
		else if( i == ABS_HAT0Y )
		{
			aiAbsMappingHigh[i] = JOY_HAT_UP;
			aiAbsMappingLow[i] = JOY_HAT_DOWN;
		}
		else
		{
			if( iNextExtraAxis < (int) ARRAYLEN(iExtraAxes) )
			{
				aiAbsMappingLow[i] = iExtraAxes[iNextExtraAxis];
				aiAbsMappingHigh[i] = enum_add2( aiAbsMappingLow[i], 1 );
				++iNextExtraAxis;
			}
		}

		++iTotalAxes;
	}
	LOG->Info( "    Total keys: %i; total axes: %i", iTotalKeys, iTotalAxes );

	return true;
}

EventDevice::~EventDevice()
{
	Close();
}

InputHandler_Linux_Event::InputHandler_Linux_Event()
{
	if( InputHandler_Linux_Event::m_bFoundAnyJoysticks )
	{
		LOG->Trace( "InputHandler_Linux_Event disabled (joystick driver already loaded)" );
		return;
	}

	/* Permission problems are likely.  We want to warn about them only if there's actually
	 * an underlying device, but if we can't open the device, the only way we can tell if
	 * there'd be anything there is sysfs.  That won't always be there. */
	m_bFoundAnyJoysticks = false;
	InputDevice NextDevice = DEVICE_JOY1;
	for( int i = 0; i < 64; ++i )
	{
		RString sFile = ssprintf( "/dev/input/event%i", i );

		g_apEventDevices.push_back( new EventDevice );
		EventDevice *pDev = g_apEventDevices.back();
		if( !pDev->Open(sFile, NextDevice) )
		{
			delete pDev;
			g_apEventDevices.pop_back();
			continue;
		}
		
		NextDevice = enum_add2(NextDevice, 1);
		m_bFoundAnyJoysticks = true;
	}

	m_bShutdown = false;

	if( m_bFoundAnyJoysticks )
	{
		m_InputThread.SetName( "Event input thread" );
		m_InputThread.Create( InputThread_Start, this );

		/* We loaded joysticks, so disable joydev. */
	}
}
	
InputHandler_Linux_Event::~InputHandler_Linux_Event()
{
	if( m_InputThread.IsCreated() )
	{
		m_bShutdown = true;
		LOG->Trace( "Shutting down joystick thread ..." );
		m_InputThread.Wait();
		LOG->Trace( "Joystick thread shut down." );
	}

	for( int i = 0; i < (int) g_apEventDevices.size(); ++i )
		delete g_apEventDevices[i];
	g_apEventDevices.clear();
}

int InputHandler_Linux_Event::InputThread_Start( void *p )
{
	((InputHandler_Linux_Event *) p)->InputThread();
	return 0;
}

void InputHandler_Linux_Event::InputThread()
{
	while( !m_bShutdown )
	{
		fd_set fdset;
		FD_ZERO( &fdset );
		int iMaxFD = -1;
		
		for( int i = 0; i < (int) g_apEventDevices.size(); ++i )
		{
			int iFD = g_apEventDevices[i]->m_iFD;
			if( !g_apEventDevices[i]->IsOpen() )
				continue;

			FD_SET( iFD, &fdset );
			iMaxFD = max( iMaxFD, iFD );
		}

		if( iMaxFD == -1 )
			break;

		struct timeval zero = {0,100000};
		if( select(iMaxFD+1, &fdset, NULL, NULL, &zero) <= 0 )
			continue;
		RageTimer now;

		for( int i = 0; i < (int) g_apEventDevices.size(); ++i )
		{
			if( !g_apEventDevices[i]->IsOpen() )
				continue;

			if( !FD_ISSET(g_apEventDevices[i]->m_iFD, &fdset) )
				continue;

			input_event event;
			int ret = read( g_apEventDevices[i]->m_iFD, &event, sizeof(event) );
			if( ret == -1 )
			{
				LOG->Warn( "Error reading from %s: %s; disabled", g_apEventDevices[i]->m_sPath.c_str(), strerror(errno) );
				g_apEventDevices[i]->Close();
				continue;
			}

			if( ret != sizeof(event) )
			{
				LOG->Warn("Unexpected packet (size %i != %i) from joystick %i; disabled", ret, (int)sizeof(event), i);
				g_apEventDevices[i]->Close();
				continue;
			}

			switch (event.type) {
			case EV_KEY: {
				int iNum = event.code;
				// In 2.6.11 using an EMS USB2, the event number for P1 Tri (the first button)
				// is being reported as 32 instead of 0.  Correct for this.
				wrap( iNum, 32 );	// max number of joystick buttons.  Make this a constant?
				ButtonPressed( DeviceInput(g_apEventDevices[i]->m_Dev, enum_add2(JOY_BUTTON_1, iNum), event.value != 0, now) );
				break;
			}
				
			case EV_ABS: {
				ASSERT_M( event.code < ABS_MAX, ssprintf("%i", event.code) );
				DeviceButton neg = g_apEventDevices[i]->aiAbsMappingLow[event.code];
				DeviceButton pos = g_apEventDevices[i]->aiAbsMappingHigh[event.code];

				float l = SCALE( int(event.value), (float) g_apEventDevices[i]->aiAbsMin[event.code], (float) g_apEventDevices[i]->aiAbsMax[event.code], -1.0f, 1.0f );
				ButtonPressed( DeviceInput(g_apEventDevices[i]->m_Dev, neg, max(-l,0), now) );
				ButtonPressed( DeviceInput(g_apEventDevices[i]->m_Dev, pos, max(+l,0), now) );
				break;
			}
			}

		}

	}

	InputHandler::UpdateTimer();
}

void InputHandler_Linux_Event::GetDevicesAndDescriptions( vector<InputDeviceInfo>& vDevicesOut )
{
	for( unsigned i = 0; i < g_apEventDevices.size(); ++i )
	{
		EventDevice *pDev = g_apEventDevices[i];
                vDevicesOut.push_back( InputDeviceInfo(pDev->m_Dev, pDev->m_sName) );
	}
}

/*
 * (c) 2003-2008 Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
