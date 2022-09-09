#pragma once

#define PIN_EN D1
#define PIN_STALL D6
#define PIN_EXT_BTN D7

#define DRIVER_ADDRESS   0b00       // TMC2209 Driver address according to MS1 and MS2
#define R_SENSE          0.11f      // E_SENSE for current calc.  

#define DEFAULT_PORT 80

#define CALFAIL_THRESH_MS   100    // if calibration runs under this time, likely failed
#define STOP_DELAY_MS 100

const int MAGIC = 0x0B00B135; // see if eeprom written already, write defaults if not

// non-volatile memory defs
#define EEP_TOTSZ               128
#define EEPADDR_MAGIC           0     // int, len 4
#define EEPADDR_SETTINGS_CRC	  4
#define EEPADDR_SETTINGS        8

#define GET_VARNAME(var) (#var)

// request result repr
const char* RESP_RESULT = "result";
const char* RESP_ERROR = "error";
const char* RESP_OK = "ok";
const char* RESP_ERRCAL = "error: run calib first";
const char* RESP_ERRARG = "error: arg";
const String strPos = "pos";


// settings
typedef struct {
	int dir;            // 0
  int motcurr_ma;     // 1
  int speed;          // 2
  int msteps;         // 3
  int stall_val;      // 4
  int fullswing_ms;   // 5
} settings_t;

// sample button w/debounce
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

void motor_go();
void motor_stop();
void update_motor_settings();
void handle_btnpress();
void load_settings();
void wifi_init();
void server_init();
void srv_settings();
void srv_motor_go();
void srv_home();

static const char html[] PROGMEM = "<!DOCTYPE html><html lang='en'><head> <meta charset='UTF-8'> <meta http-equiv='X-UA-Compatible' content='IE=edge'> <meta name='viewport' content='width=device-width, initial-scale=1.0'> <title>mblinds</title> <style>body{font-family:Helvetica; font-size: 24px; background-color:black; color:#fff;}button{border:0; border-radius:0.3em; background-color:#067da1; color:#fff; line-height:2.0em; font-size:18px; width: 200px; display: inline-block; margin: 10px 5px 10px auto;}.val{font-size: 18px; color: #aaa; margin: 0 0 0 10px;}.wrap{position: relative; width: 300px; margin: 0 auto;}</style></head><body> <div class='wrap'> <div> <p id='id'>ID: </p><p id='st'>State: </p></div><div id='rcload' style='display: none'>Please refresh when complete.</div><div id='btnsgo'> <button onclick='go(this,1)'>&#9776; Open</button> <button onclick='go(this,0)'>&#9608; Close</button> </div><p style='margin-top: 1.5em;'>Config:</p><div id='btn_cfg'></div><p style='margin-top: 1.5em;'>Funcs:</p><div id='btn_funcs'></div></div><script>var loc;var dat; var d_config={0: 'Dir', 1: 'MotorCurr (ms)', 2: 'Speed', 3: 'mSteps', 4: 'StallVal', 5: 'FullSwing (ms)'}; var d_funcs={'Calibrate': 'settings?runcal', 'Reset' : 'settings?reset'}; function g(i){return document.getElementById(i);}; function app(i,t){g(i).innerHTML+=t;}; function go(o,i){var dly_ms=dat[6]==-1 ? 2000 : dat[6]; dly(o, dly_ms); fetch(loc + `motorgo?pos=${i}`);}; function ce(n){return document.createElement(n);}function sa(o,a,v){o.setAttribute(a, v);}function ac(o1,o2){o1.appendChild(o2);}function tc(o, t){o.textContent=t;}function cal(o){dly(o,0); fetch(loc + `settings?runcal`); g('rcload').style.display='block';}function dly(o, t=2000){o.disabled=true; o.style.backgroundColor='#ccc'; if (t > 0) setTimeout(function(){o.disabled=false; o.style.backgroundColor='#1fa3ec'; location.reload();},t);}function doset(b,cm){var pr=prompt('Set ' + b.innerHTML, dat[cm.toString()]);if (pr){var url=`${loc}settings?${cm}=${pr}`; fetch(url); dly(b, 1000);}}function dofunc(b,cm){if (confirm(b.innerHTML + '?')) fetch(loc + cm);}window.addEventListener('load', function(){loc=window.location.href; fetch(loc + 'settings') .then((response)=>{return response.json();}).then((data)=>{dat=data; var id=dat['id']; document.title=id; app('id', id); var p=parseInt(dat['pos']); var ps=p==0 ? 'closed': p==1 ? 'open' : 'NeedCal'; app('st', ps); if (p < 0) g('btnsgo').style.display='none'; for (var key in d_config){var div=ce('div'); var btn=ce('button'); var inf=ce('span'); tc(btn, d_config[key]); tc(inf, dat[key]); sa(inf, 'class', 'val'); sa(btn, 'onclick', `doset(this, ${key});`); ac(div, btn); ac(div, inf); ac(g('btn_cfg'), div);}for (var key in d_funcs){var div=ce('div'); var btn=ce('button'); tc(btn, key); sa(btn, 'onclick', `dofunc(this, '${d_funcs[key]}');`); ac(div, btn); ac(g('btn_funcs'), div);}});}); </script></body></html>";
