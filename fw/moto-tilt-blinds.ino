#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <Arduino_JSON.h>
#include <ESP_EEPROM.h>
#include <TMCStepper.h>
#include <elapsedMillis.h>
#include "moto-tilt-blinds.h"

// *************** UPDATE HOSTNAME/WIFI HERE **************** //

// set hostname
const char* HOSTNAME = "mblindsv2-0";

// set the wifi ssid/password
const char* SSID = "ssid";
const char* PASSWORD = "password";

// ********************************************************** //

TMC2209Stepper driver(&Serial1, R_SENSE , DRIVER_ADDRESS);
ESP8266WebServer server(DEFAULT_PORT);
Button btnToggle;			// toggle button

elapsedMillis em_cal;
elapsedMillis em_move;

int cal_state = -1;           // -1: not calibrating. see state machine in main loop
int movetopos = -1;           // 0: closed 1: open -1: not moving
int poscur = -1;              // -1: unknown, 0: closed, 1: open
char buftx[128];			        // debug serial output buffer
int settings_sz;

// default settings
const settings_t default_settings =
{
	.dir = 1,
  .motcurr_ma = 1200,
  .speed = 250,
  .msteps = 0,
  .stall_val = 10,
  .fullswing_ms = -1
};

settings_t settings;	// local settings

void setup() 
{
  // pin setup
  pinMode(PIN_STALL, 0);
  pinMode(PIN_EN, 1);
  digitalWrite(PIN_EN, 1);

  btnToggle.begin(PIN_EXT_BTN);

  // debug serial
  Serial.begin(115200);
  while (!Serial);

    // tmc2209 uart (tx only)
  Serial1.begin(19200);
  while (!Serial1);

  EEPROM.begin(EEP_TOTSZ);
  load_settings();

  driver.begin();
  driver.toff(4);
  driver.blank_time(24);
  driver.rms_current(settings.motcurr_ma); 
  driver.microsteps(settings.msteps);
  driver.TCOOLTHRS(0xFFFFF);
  driver.pwm_autoscale(true); 
  driver.semin(5);
  driver.semax(2);
  driver.sedn(0b01);
  driver.SGTHRS(settings.stall_val);

  wifi_init();

	server_init();

  motor_stop(); // reset status signal

}

void update_motor_settings()
{
   driver.rms_current(settings.motcurr_ma); 
   driver.microsteps(settings.msteps);
   driver.SGTHRS(settings.stall_val);
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

  settings_sz = sizeof(settings) / sizeof(int);
  settings.fullswing_ms = default_settings.fullswing_ms;  // need to calibrate every bootup
}

void motor_go(int speed)
{
    driver.VACTUAL(0);
    digitalWrite(PIN_EN, 0);
    delay(64);
    
    digitalWrite(PIN_EN, 0);
    int dir = speed < 0 ? -1 : 1;
    
    for (int i=0; i < abs(speed); i+=2)
    {
      driver.VACTUAL(i * dir);
      delay(1);
    }
    em_move = 0;  // reset motor move timer
}

void motor_stop()
{
    driver.VACTUAL(0);
    digitalWrite(PIN_EN, 1);

    for (int i=0; i<200; i++)
    {
      digitalWrite(PIN_EN, 0); 
      delayMicroseconds(100);
      digitalWrite(PIN_EN, 1);
      delayMicroseconds(100);
    }
    
    digitalWrite(PIN_EN, 1);   
}

void handle_btnpress()
{
    if (poscur != -1)
    {
      movetopos = poscur == 0 ? 1 : 0;
      int dir = poscur == 1 && movetopos == 0  ? 1 : -1;
      motor_go(dir * settings.dir * settings.speed);
    }
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

void server_init()
{
  server.on("/settings", srv_settings);
  server.on("/motorgo", srv_motor_go);
  server.on("/", srv_home);

  server.enableCORS(true);

  server.begin();

  sprintf(&buftx[0], "Server listening on port %d", DEFAULT_PORT);
  Serial.println(buftx);

  sprintf(&buftx[0],"%s-ota", HOSTNAME);	
  ArduinoOTA.setHostname(buftx);	// appending '-ota' to hostname for ArduinoOTA host
  ArduinoOTA.begin();
}

void srv_home()
{
	server.send(200, "text/html", FPSTR(html));
}

// get or set settings, endpoint /settings
void srv_settings()
{
	JSONVar resp;
	bool save = true;
  resp["result"] = RESP_OK;

  if (server.args())
  {
    
    if (server.hasArg("reset"))
    {
      EEPROM.wipe();
      resp["result"] = "Power cycle to reset settings";
      save = false;
    } else if (server.hasArg("estop"))
    {
       motor_stop();
       cal_state = movetopos = -1;
       save = false;
    }
    else if (server.hasArg("runcal"))
    {
       cal_state = 0;

    }
    else if (server.hasArg("toggledir"))
    {
      settings.dir *= -1;
    }
    else
    {
      int var_idx = server.argName(0).toInt();
      if (var_idx >= 0 && var_idx < settings_sz) 
      {
        int * varp = (int *)&settings;
        varp += var_idx;
        *varp = server.arg(server.argName(0)).toInt();
        update_motor_settings();
      } else
      {
        resp["result"] = RESP_ERROR;
      }
    }

  } else  // no args, return all settings
  {
      int * varp = (int *)&settings;
      for (int i=0; i < settings_sz; ++i)
      {
        resp[String(i)] = *varp;
        varp++;
      }
      // tag on extra info, id, pos
      resp["id"] = HOSTNAME;
      resp["pos"] = poscur;
      save = false;
        
  }

  if (save)
	{
		EEPROM.put(EEPADDR_SETTINGS, settings);
		EEPROM.commit();
	}
	
	server.send(200, "text/plain", JSON.stringify(resp));

}

void srv_motor_go()
{
	JSONVar resp;
	
  resp[RESP_RESULT] = RESP_OK;
  
  if (server.hasArg(strPos))
  {
    if (settings.fullswing_ms == -1 || poscur == -1)
    {
      resp[RESP_RESULT] = RESP_ERRCAL;
    } else
    {
        int pos = server.arg(strPos).toInt();
        // validate target pos
        if (pos != poscur && (pos == 0 || pos == 1))
        {
            int dir = poscur == 1 && pos == 0  ? 1 : -1;
            movetopos = pos;
            motor_go(dir * settings.dir * settings.speed);
        }
            
    }
    
  } else
  {
    resp[RESP_RESULT] = RESP_ERRARG;
  }

  server.send(200, "text/plain", JSON.stringify(resp));
}

void loop() 
{
  ArduinoOTA.handle();

  bool stalled = digitalRead(PIN_STALL);  // motor_stop will handle successive 
  // see github.com/teemuatlut/TMCStepper/issues/191
  // and github.com/teemuatlut/TMCStepper/issues/174

  // calibration/move state machine
  if (cal_state != -1)
  {
    switch (cal_state)
    {
      case 0:
        motor_go(settings.dir * settings.speed);
        cal_state = 1;
        break;
      case 1:
        if (stalled)
        {
            motor_stop();
            motor_go(-settings.dir * settings.speed); // move over the whole range (full swing)
            em_cal = 0;   // start the timer to capture fullswing_ms
            cal_state = 2;

        }
        break;
      case 2:
        if (stalled)
        {
          motor_stop();
          settings.fullswing_ms = em_cal;  // got full swing
          motor_go(settings.dir * settings.speed);  // start moving to halfway/open
          cal_state = 3;
        }
        break;
      case 3:
        if (em_move >= settings.fullswing_ms / 2 || stalled)    // stop at halfway/open
        {
          motor_stop();
          cal_state = -1;
          poscur = 1;
        }
        break;
    }
  } else if (movetopos != -1)
  {
    // force stall condition if half fullswing time elapsed
    if (em_move >= settings.fullswing_ms / 2)
      stalled = true;

    if (stalled)
    {
        motor_stop();
        poscur = movetopos;
        movetopos = -1;
    }
    
  }

  if (btnToggle.pressed())
		handle_btnpress();

  server.handleClient();

}
