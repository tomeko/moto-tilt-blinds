#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <elapsedMillis.h>
#include <ESP_EEPROM.h>
#include "mblinds_fw.h"

// *************** UPDATE HOSTNAME/WIFI HERE **************** //

// set hostname
const char* HOSTNAME = "mblinds1";

// set the wifi ssid/password
const char* SSID = "ssid";
const char* PASSWORD = "pw";

// ********************************************************** //

// default settings
const settings_t default_settings =
{
	.angle_down = -DEFAULT_TILT_ANGLE,
	.angle_mid = DEFAULT_MID_ANGLE,
	.angle_up = DEFAULT_TILT_ANGLE,
	.angle_btntoggle = DEFAULT_TILT_ANGLE,
	.angle_tolerance = DEFAULT_ANGLE_TOLERANCE,
	.speed = DEFAULT_MOTORSPEED,
	.dir = 1,
	.stallcheck_ms = DEFAULT_MOTOR_STALLCHECK_MS,
	.angle_presets = { NULL_ANGLE, NULL_ANGLE, NULL_ANGLE, NULL_ANGLE }
};

settings_t settings;	// local settings

MPU6050 mpu(Wire);		// accel/gyro (mpu)
ESP8266WebServer server(DEFAULT_PORT);
elapsedMillis motor_ms;		// tracks time for motor movement (ms)
elapsedMillis mpupdate_ms;	// mpu update interval
Button btnToggle;			// toggle button

float angle_curr;					// current angle
float angle_last = NULL_ANGLE;		// last angle
float speed_curr;					// current speed
float tgt_angle = NULL_ANGLE;		// target angle
char buftx[128] = { 0 };			// serial output buffer
char bufstatus[128] = { 0 };		// status buffer
int status = STATUS_IDLE;			// see E_STATUS enum
bool motor_searching;				// motor searching/moving flag

void setup()
{
	// setup pins for motor
	pinMode(IN1, 1);
	pinMode(IN2, 1);
	digitalWrite(IN1, 0);
	digitalWrite(IN2, 0);
	
	btnToggle.begin(EXT_BTN_TOGGLE);

	EEPROM.begin(EEP_TOTSZ);
	load_settings();

	mpu_init();

	Serial.begin(115200);

	wifi_init();

	server_init();
}

// loads settings, from "eeprom"
void load_settings()
{
	int m;
	uint32_t crc;
	uint32_t crc_defset = crc32((uint8_t*)&default_settings, sizeof(settings_t));
	
	EEPROM.get(EEPADDR_MAGIC, m);
	EEPROM.get(EEPADDR_SETTINGS_CRC, crc);
	
	// if magic u32 not set or default settings changed then load defaults
	if (m != MAGIC || crc != crc_defset)
	{
		EEPROM.put(EEPADDR_MAGIC, MAGIC);
		EEPROM.put(EEPADDR_SETTINGS_CRC, crc_defset);
		EEPROM.put(EEPADDR_SETTINGS, default_settings);
		EEPROM.commit();

		memcpy(&settings, &default_settings, sizeof(settings_t));
		Serial.println("Loaded defaults");
	}
	else
	{
		EEPROM.get(EEPADDR_SETTINGS, settings);		// otherwise read saved settings from eeprom
	}
}

void loop()
{

	ArduinoOTA.handle();

	// update angle
	if (mpupdate_ms % MPU_UPDATEMS == 0)
		mpu_update();

	// if motor looking for position
	if (motor_searching)
	{
		status = STATUS_MOVING;
		motor_checkstall();
		// check if target reached, stop if so
		if (angle_curr > (tgt_angle - settings.angle_tolerance) && angle_curr <= (tgt_angle + settings.angle_tolerance))
			motor_stop();
	}
	
	if (btnToggle.pressed())
		handle_btnpress();

	server.handleClient();
}

// tell motor to start moving towards angle
void motor_go(float angle)
{
	int dir = angle < angle_curr ? CC : CW;
	dir *= settings.dir;	// ability to flip direction here, 1 or -1, default = 1
	tgt_angle = angle;

	int in1 = dir < 0 ? settings.speed * PWM_CNTS : 0;
	int in2 = dir < 0 ? 0 : settings.speed * PWM_CNTS;

	analogWrite(IN1, in1);
	analogWrite(IN2, in2);

	motor_searching = true;
	motor_ms = 1;
}

void motor_stop()
{
	tgt_angle = angle_last = NULL_ANGLE;
	analogWrite(IN1, 0);
	analogWrite(IN2, 0);
	motor_searching = false;
	status = STATUS_IDLE;
}

// for now, stall check is a simple timeout
void motor_checkstall()
{
	if (motor_ms % settings.stallcheck_ms == 0)
		motor_stop();
}

// handle button press, the angle is set in settings.angle_btntoggle
void handle_btnpress()
{
	int angle_set;
	if (angle_curr > settings.angle_mid + 15 || angle_curr < settings.angle_mid - 15)
		angle_set = settings.angle_mid;
	else
		angle_set = settings.angle_btntoggle;
	
	if (motor_searching)
		motor_stop();
	else
		motor_go(angle_set);

}

void wifi_init()
{
	WiFi.mode(WIFI_STA);
	WiFi.hostname(HOSTNAME);
	WiFi.begin(SSID, PASSWORD);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	sprintf(&buftx[0], "\r\nConnected to: %s IP: %s hostname: %s", SSID, WiFi.localIP().toString().c_str(), WiFi.hostname().c_str());
	Serial.println(buftx);

}

// initialize server, ssdp
// ssdp cmd example: gssdp-discover --target=urn:schemas-upnp-org:device:motoblinds:1
void server_init()
{

	server.on("/settings", srv_settings);
	server.on("/motorgo", srv_motor_go);
	server.on("/motor", srv_motor_query);
	server.on("/", srv_home);
	
	server.on("/description.xml", HTTP_GET, []() 
	{
      SSDP.schema(server.client());
    });

    server.enableCORS(true);

    server.begin();
    SSDP.setDeviceType("urn:schemas-upnp-org:device:motoblinds:1");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("MotoBlinds");
    SSDP.setSerialNumber(HOSTNAME);
    SSDP.setURL("index.html");
    SSDP.setModelName("MotoBlinds");
    SSDP.setModelNumber("1");
    SSDP.setModelURL("http://github.com/tomeko");
    SSDP.setManufacturer("http://github.com/tomeko");
    SSDP.setManufacturerURL("http://github.com/tomeko");
    SSDP.begin();
	
	sprintf(&buftx[0], "Server listening on port %d", DEFAULT_PORT);
	Serial.println(buftx);

	sprintf(&buftx[0],"%s-ota", HOSTNAME);	
	ArduinoOTA.setHostname(buftx);	// appending '-ota' to hostname for ArduinoOTA host
	ArduinoOTA.begin();
}

// get or set settings, endpoint /settings
void srv_settings()
{
	JSONVar resp;
	bool save = true;

	if (server.hasArg(strUp))
		settings.angle_up = angle_curr;
	else if (server.hasArg(strMid))
		settings.angle_mid = angle_curr;
	else if (server.hasArg(strDown))
		settings.angle_down = angle_curr;
	else if (server.hasArg(strButton))
		settings.angle_btntoggle = angle_curr;
	else if (server.hasArg(strTolerance))
	{
		if (server.arg(strTolerance) != "")
			settings.angle_tolerance = server.arg(strTolerance).toFloat();
		else
			resp["result"] = RESP_ERROR;
	} else if (server.hasArg(strSpeed))
	{
		if (server.arg(strSpeed) != "")
			settings.speed = abs(server.arg(strSpeed).toFloat());
		else
			resp["result"] = RESP_ERROR;
	} else if (server.hasArg(strDir))
	{
		if (server.arg(strDir) != "")
		{
			int dir = server.arg(strDir).toInt();
			if (dir == -1 || dir == 1)
				settings.dir = dir;
		}
	}
	else if (server.hasArg(strPreset))
	{
		if (server.arg(strPreset) != "")
		{
			int num = server.arg(strPreset).toInt();
			if (num > 0 && num <= NUM_PRESETS)
				settings.angle_presets[num] = angle_curr;
			else
				resp["result"] = RESP_ERROR;
		}
	}
	else if (server.hasArg(strToggleDir))
	{
		settings.dir *= -1;
	}
	else if (server.hasArg(strStallCheck_ms))
	{
		if (server.arg(strStallCheck_ms) != "")
			settings.stallcheck_ms = server.arg(strStallCheck_ms).toInt();
		else
			resp["result"] = RESP_ERROR;
	}
	else if (server.hasArg("reset"))
	{
		EEPROM.wipe();
		resp["result"] = "Power cycle to reset settings";
		save = false;
	}
	else
	{
		resp[strDown] = settings.angle_down;
		resp[strMid] = settings.angle_mid;
		resp[strUp] = settings.angle_up;
		resp[strButton] = settings.angle_btntoggle;
		resp[strTolerance] = settings.angle_tolerance;
		resp[strSpeed] = settings.speed;
		resp[strDir] = settings.dir;
		resp[strStallCheck_ms] = settings.stallcheck_ms;
		save = false;
	}

	if (save)
	{
		EEPROM.put(EEPADDR_SETTINGS, settings);
		EEPROM.commit();
	}
		
	
	server.send(200, "text/plain", JSON.stringify(resp));
}

// start motor movement by preset or orientation
void srv_motor_go()
{
	JSONVar resp;
	resp[RESP_RESULT] = RESP_OK;

	if (server.hasArg(strOren))
	{
		if (server.arg(strOren) != "")
		{
			int dir = server.arg(strOren).toInt();
			float angle_set = NULL_ANGLE;
			if (dir == -1)
				angle_set = settings.angle_down;
			else if (dir == 0)
				angle_set = settings.angle_mid;
			else if (dir == 1)
				angle_set = settings.angle_up;
			if (angle_set != NULL_ANGLE)
				motor_go(angle_set);
		}
	}
	else if (server.hasArg(strPreset))
	{
		if (server.arg(strPreset) != "")
		{
			// todo: checks
			int pidx = server.arg(strPreset).toInt();
			motor_go(settings.angle_presets[pidx]);
		}
	}
	else if (server.hasArg(strMoverel))
	{
		if (server.arg(strMoverel) != "")
		{
			int rangle = server.arg(strMoverel).toInt();
			if (rangle >= -90 && rangle <= 90)
				motor_go(angle_curr + rangle);
		}
	}
	else
	{
		resp[RESP_RESULT] = RESP_ERROR;
	}
	server.send(200, "text/plain", JSON.stringify(resp));
}

// root html control/status page endpoint /
void srv_home()
{
	server.send(200, "text/html", FPSTR(html));
}

// motor status query endpoint /motor
void srv_motor_query()
{
	JSONVar resp;
	resp["angle"] = angle_curr;
	resp["status"] = strSTATUS[status];
	String angle_curr_str = angle_curr < settings.angle_mid - 5 ? strDown : angle_curr > settings.angle_mid + 5 ? strUp : strMid;
	resp["pos"] = angle_curr_str;
	resp["id"] = HOSTNAME;
	resp["dir"] = settings.dir;
	resp["ip"] = WiFi.localIP().toString();

	if (status == STATUS_MOVING)
		resp["tgt_angle"] = tgt_angle;
	server.send(200, "text/plain", JSON.stringify(resp));
}

void mpu_init()
{

	Wire.begin();
	mpu.begin();
	delay(1000);		// settletime for mpu6050
	mpu.calcOffsets();
}

// update current angle
void mpu_update()
{
	mpu.update();
	angle_curr = mpu.getAngleY();
}

