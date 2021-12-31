#pragma once

const int MAGIC = 0x0B00B135; // see if eeprom written already, write defaults if not

// non-volatile memory defs
#define EEP_TOTSZ               128
#define EEPADDR_MAGIC           0     // int, len 4
#define EEPADDR_SETTINGS_CRC	4
#define EEPADDR_SETTINGS        8

// pin defs
#define IN1 D5
#define IN2 D6
#define EXT_BTN_TOGGLE D4

// other defs

#define PWM_CNTS  1023
#define NUM_PRESETS             4
#define DEFAULT_PORT 80
#define MPU       0x68  // for dy521 accel/gyro
#define MOTOR_MAX_TURNS 10
#define MOTOR_MAX_SPEED 4
#define POS_DEF_TOLERANCE 200
#define NULL_ANGLE	1080
#define DEFAULT_MID_ANGLE	0
#define DEFAULT_TILT_ANGLE	45
#define DEFAULT_ANGLE_TOLERANCE	1.5
#define DEFAULT_MOTORDIR_DOWN	-1
#define DEFAULT_MOTORSPEED	1.0
#define MPU_UPDATEMS	50
#define MOTOR_STALLCHECK_MS	200
#define MOTOR_STALLCHECKINIT_MS	500
#define DEFAULT_MOTORSTALLCHECK_DX_ANGLE	1.0
#define MOTORSTALLCHECK_DXTHRESH	(settings.stallcheck_angledx / ((1000.0 / MOTOR_STALLCHECK_MS)))	//  degree/sec: if less, then stall event occured
#define MOTORMOVE_TIMEO_MS	7000

enum E_STATUS { STATUS_IDLE, STATUS_STALL, STATUS_MOVING };
enum E_MOTORTILT_DIR { CW = -1, IDLE = 0, CC = 1 };

// request result repr
const char* RESP_RESULT = "result";
const char* RESP_ERROR = "error";
const char* RESP_OK = "ok";

// request args repr
const String strOren = "oren";
const String strPreset = "preset";
const String strDown = "down";
const String strMid = "mid";
const String strUp = "up";
const String strSpeed = "speed";
const String strButton = "btn";
const String strTolerance = "tolerance";
const String strDir = "dir";
const String strReset = "reset";
const String strStallCheckAngle_dx = "stallangledx";

const String strSTATUS[] = {"IDLE", "STALL", "MOVING"};

// settings
typedef struct {
	float angle_down, angle_mid, angle_up, angle_btntoggle, angle_tolerance, speed, stallcheck_angledx;
	int dir;
	float angle_presets[NUM_PRESETS];
	
} settings_t;

// 2wire button implementation (no pullups needed)
class Button
{
private:
	uint8_t btn;
	uint16_t state;
public:
	void begin(uint8_t button)
	{
		btn = button;
		state = 0;
		pinMode(btn, INPUT_PULLUP);
	}

	bool pressed()
	{
		state = (state << 1) | digitalRead(btn) | 0xfe00;
		return (state == 0xff00);
	}
};

// crc for checking if settings struct changed, which would reload defaults
#define POLY 0xedb88320
uint32_t crc32(uint8_t * buf, size_t len)
{
	int k;

	uint32_t crc = ~MAGIC;
	while (len--) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
	}
	return ~crc;
}

// func defs
void mpu_init();
void mpu_update();
void wifi_init();
void server_init();
void load_settings();
void save_settings(settings_t* settings);
void handle_btnpress();
void motor_stop();
void motor_checkstall();

// root status/control html/etc
// see home.full.html for unminified
// minified using https://www.willpeavy.com/tools/minifier/
static const char html[] PROGMEM = "<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta http-equiv='X-UA-Compatible' content='IE=edge'> <meta name='viewport' content='width=device-width, initial-scale=1.0'> <title>mblinds</title> <style>body{text-align:center;font-family:verdana; font-size: 1.5em;background-color:black;color:white;}button{border:0;border-radius:0.3em;background-color:#1fa3ec;color:#fff;line-height:2.4em;font-size:1.2em;width:100%;display:block; margin-top: 15px;}</style></head><body> <div style='text-align:left;display:inline-block;width:320px;padding:5px'> <p id='id'>ID: </p><p id='st'>STATE: </p><button id='bo' onclick=\"go(this,'0')\">&#x2600; Open</button> <button id='bc' onclick=\"go(this,'1')\">&#x263C; Close</button> </div><script>function g(i){return document.getElementById(i);}; function s(i,t){g(i).innerHTML+=t;}; function go(o,i){d(o); fetch('motorgo?oren='+i);}; function d(o){o.disabled=true; o.style.backgroundColor='#ccc'; setTimeout(function(){o.disabled=false; o.style.backgroundColor='#1fa3ec'; location.reload();},5000);}window.addEventListener('load', function(){fetch('motor') .then((response)=>{return response.json();}).then((data)=>{var id=data['id']; document.title=id; s('id', id); var p=data['pos']; var ps=p[0]=='u'||p[0]=='d'?'closed':'open'; s('st', ps);});}) </script></body></html>";


