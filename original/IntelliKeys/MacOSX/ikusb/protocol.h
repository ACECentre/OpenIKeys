/*
 *  protocol.h
 *  ikusb
 *
 *  Created by fred ross-perry on 7/10/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

//
//  command codes sent to the device
//  see firmware documentation for details
//

#define CMD_BASE 0
#define IK_CMD_GET_VERSION			CMD_BASE+1
#define IK_CMD_LED					CMD_BASE+2
#define IK_CMD_SCAN					CMD_BASE+3
#define IK_CMD_TONE					CMD_BASE+4
#define IK_CMD_GET_EVENT			CMD_BASE+5
#define IK_CMD_INIT					CMD_BASE+6
#define IK_CMD_EEPROM_READ			CMD_BASE+7
#define IK_CMD_EEPROM_WRITE			CMD_BASE+8
#define IK_CMD_ONOFFSWITCH			CMD_BASE+9
#define IK_CMD_CORRECT				CMD_BASE+10
#define IK_CMD_EEPROM_READBYTE		CMD_BASE+11
#define IK_CMD_RESET_DEVICE         CMD_BASE+12
#define IK_CMD_START_AUTO			CMD_BASE+13
#define IK_CMD_STOP_AUTO            CMD_BASE+14
#define IK_CMD_ALL_LEDS				CMD_BASE+15
#define IK_CMD_START_OUTPUT			CMD_BASE+16
#define IK_CMD_STOP_OUTPUT			CMD_BASE+17
#define IK_CMD_ALL_SENSORS			CMD_BASE+18
#define IK_CMD_REFLECT_KEYSTROKE	CMD_BASE+21
#define IK_CMD_REFLECT_MOUSE_MOVE	CMD_BASE+22


//  starting here, these are new for IK4

#define IK_CMD_LOCKSETTINGS			CMD_BASE+23
#define IK_CMD_SIGNALSTRENGTH		CMD_BASE+25
#define IK_CMD_BATTERYCAPACITY		CMD_BASE+26
#define IK_CMD_INACTIVITY			CMD_BASE+27
#define IK_CMD_OVERLAYNAME			CMD_BASE+28
#define IK_CMD_GETUSERSETTINGS		CMD_BASE+29
#define IK_CMD_CHANGEUSERSETTINGS	CMD_BASE+30

#define IK_CMD_STORE_OVERLAY		CMD_BASE+31
#define IK_CMD_NO_OVERLAY			CMD_BASE+32


//======================================================

//
//  result codes/data sent to the software
//  see firmware documentation for details
//
#define EVENT_BASE 50
#define IK_EVENT_ACK				EVENT_BASE+1
#define IK_EVENT_MEMBRANE_PRESS		EVENT_BASE+2
#define IK_EVENT_MEMBRANE_RELEASE	EVENT_BASE+3
#define IK_EVENT_SWITCH				EVENT_BASE+4
#define IK_EVENT_SENSOR_CHANGE		EVENT_BASE+5
#define IK_EVENT_VERSION			EVENT_BASE+6
#define IK_EVENT_EEPROM_READ		EVENT_BASE+7
#define IK_EVENT_ONOFFSWITCH	    EVENT_BASE+8
#define IK_EVENT_NOMOREEVENTS		EVENT_BASE+9
#define IK_EVENT_MEMBRANE_REPEAT    EVENT_BASE+10
#define IK_EVENT_SWITCH_REPEAT      EVENT_BASE+11
#define IK_EVENT_CORRECT_MEMBRANE   EVENT_BASE+12
#define IK_EVENT_CORRECT_SWITCH     EVENT_BASE+13
#define IK_EVENT_CORRECT_DONE		EVENT_BASE+14
#define IK_EVENT_EEPROM_READBYTE	EVENT_BASE+15
#define IK_EVENT_DEVICEREADY        EVENT_BASE+16  
#define IK_EVENT_AUTOPILOT_STATE    EVENT_BASE+17
#define IK_EVENT_DELAY              EVENT_BASE+18
#define IK_EVENT_ALL_SENSORS		EVENT_BASE+19


//  starting here, the following are new for IK4

//  the next three are for sending events to the engine
//  using a series of smaller messages.
#define IK_EVENT_MULTIPART_BEGIN	EVENT_BASE+20
#define IK_EVENT_MULTIPART			EVENT_BASE+21
#define IK_EVENT_MULTIPART_END		EVENT_BASE+22

//  for launching docs and apps
#define IK_EVENT_LAUNCH_APP			EVENT_BASE+23
#define IK_EVENT_LAUNCH_DOC			EVENT_BASE+24

#define IK_EVENT_LOCKSETTINGS		EVENT_BASE+25
#define IK_EVENT_SIGNALSTRENGTH		EVENT_BASE+26
#define IK_EVENT_BATTERYCAPACITY	EVENT_BASE+27
#define IK_EVENT_INACTIVITY			EVENT_BASE+28
#define IK_EVENT_OVERLAYNAME		EVENT_BASE+29
#define IK_EVENT_SETTINGSDATA		EVENT_BASE+30

#define IK_EVENT_GET_OVERLAY		EVENT_BASE+31
#define IK_EVENT_SHOWHELP			EVENT_BASE+32
#define IK_EVENT_LISTFEATURES		EVENT_BASE+33
#define IK_EVENT_TOGGLE_CP			EVENT_BASE+34
#define IK_EVENT_REFRESH_CP			EVENT_BASE+35

#define IK_EVENT_MEMBRANE_PRESS_4		EVENT_BASE+36
#define IK_EVENT_MEMBRANE_RELEASE_4		EVENT_BASE+37
#define IK_EVENT_SWITCH_4				EVENT_BASE+38
#define IK_EVENT_ONOFFSWITCH_4			EVENT_BASE+39

#define IK_FIRSTUNUSED_EVENTCODE    EVENT_BASE+40

//  some internal commands that don't make it to the device
#define COMMAND_BASE 100
#define IK_CMD_DELAY				COMMAND_BASE+1 // (msec)
#define IK_CMD_MOUSE_MOVE			COMMAND_BASE+2 // (x, y)
#define IK_CMD_MOUSE_BUTTON			COMMAND_BASE+3 // (left/right, down/up)
#define IK_CMD_KEYBOARD				COMMAND_BASE+4 // (keycode, down/up)
#define IK_CMD_KEY_DONE				COMMAND_BASE+5 // signal downstream that a key is done
#define IK_CMD_KEY_START			COMMAND_BASE+6 // signal downstream that a key is starting
#define IK_CMD_KEY_REPEAT			COMMAND_BASE+7 // signal downstream that a key is repeating

#define IK_CMD_CP_HELP				COMMAND_BASE+8
#define IK_CMD_CP_LIST_FEATURES		COMMAND_BASE+9
#define IK_CMD_CP_REFRESH			COMMAND_BASE+10
#define IK_CMD_CP_TOGGLE			COMMAND_BASE+11

#define IK_CMD_KEYBOARD_UNICODE		COMMAND_BASE+12 // lead, trail, down/up

#define IK_CMD_LIFTALLMODIFIERS     COMMAND_BASE+13

#define IK_CMD_CP_REPORT_REALTIME	COMMAND_BASE+14

#define IK_CMD_LIFTNONMODIFIERS     COMMAND_BASE+15
#define IK_CMD_LOCK					COMMAND_BASE+16
#define IK_CMD_UNLOCK				COMMAND_BASE+17

#define IK_CMD_SEND_SETTINGS_TO_DEV		COMMAND_BASE+18

#define IK_CMD_IFSHIFT				COMMAND_BASE+19
#define IK_CMD_ELSE					COMMAND_BASE+20
#define IK_CMD_ENDIF				COMMAND_BASE+21

#define IK_CMD_SOUND				COMMAND_BASE+22


