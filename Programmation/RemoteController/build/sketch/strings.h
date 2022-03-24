
const char MAIN_PAGE[] PROGMEM = R"=====(
    <div style='text-align:left;display:inline-block;color:#000;min-width:340px;'>
      <noscript>To use this interface, please enable JavaScript <br></noscript>
      <table style="width:100%">
        <tbody>
          <tr>
            <td id="light-status" style="width:100%;text-align:center;font-weight:bold;font-size:60px">-</td>
          </tr>
          <tr>
            <td id="lampshade-status" style="width:100%;text-align:center;font-weight:bold;font-size:40px">-</td>
          </tr>
        </tbody>
      </table>
      <div style="display: block;"></div>
      <p>
        <button type="button" onclick="xhr.abort(); fetch('cmd?light=toggle');">ON / OFF</button>
      </p>
      <p>
        <button type="button" id="lampshade-action" data-action="open" onclick="xhr.abort(); var btnLampshade = document.getElementById('lampshade-action'); btnLampshade.setAttribute('disabled', 'disabled'); fetch('cmd?light=' + btnLampshade.getAttribute('data-action'));">Open</button>
      </p>
      <p>
        <form style="display: block;" action="infos"><button type="submit">Informations</button></form>
      </p>
      <p>
        <form style="display: block;" action="config"><button type="submit">Configuration</button></form>
      </p>
    </div>
)=====";

const char INFOS_PAGE[] PROGMEM = R"=====(
<table style="width:100%">
   <tbody>
      <tr>
         <th>Firmware version</th>
         <td>{firmware}</td>
      </tr>
      <tr>
         <th>Hostname</th>
         <td>{host}</td>
      </tr>
      <tr>
         <th>Uptime</th>
         <td>{uptime_mins} min(s) {uptime_secs} second(s)</td>
      </tr>
      <tr>
         <th>Restart Reason</th>
         <td>{restartreason}</td>
      </tr>  
      <tr>
         <th>Circuit Voltage</th>
         <td>{vcc} mV</td>
      </tr>
      <tr>
         <th>Lampshade opening time</th>
         <td>{opentime} seconds</td>
      </tr> 
      <tr>
         <th>Lampshade closing time</th>
         <td>{closetime} seconds</td>
      </tr>              
      <tr>
         <th></th>
         <td>&nbsp;</td>
      </tr>
      <tr>
         <th>SSID</th>
         <td>{ssid}</td>
      </tr>
      <tr>
         <th>MAC Address</th>
         <td>{mac}</td>
      </tr>
      <tr>
         <th>IP Address</th>
         <td>{ip}</td>
      </tr>
      <tr>
         <th>Gateway</th>
         <td>{gw}</td>
      </tr>
      <tr>
         <th>Subnet Mask</th>
         <td>{mask}</td>
      </tr>
      <tr>
         <th>DNS Server</th>
         <td>{dns}</td>
      </tr>
      <tr>
         <th>Wifi connected</th>
         <td>{isconnected}</td>
      </tr>
      <tr>
         <th>Auto reconnection</th>
         <td>{autocon}</td>
      </tr>      
      <tr>
         <th></th>
         <td>&nbsp;</td>
      </tr>
      <tr>
         <th>MQTT</th>
         <td>{mqttenable}</td>
      </tr>
      <tr>
         <th>HUE Emulator</th>
         <td>{hueenable}</td>
      </tr>
      <tr>
         <th></th>
         <td>&nbsp;</td>
      </tr>
      <tr>
         <th>ESP Chip Id</th>
         <td>{fchipid}</td>
      </tr>
      <tr>
         <th>Core / SDK Version</th>
         <td>{corever} / {sdkver}</td>
      </tr>
      <tr>
         <th>Boot Version</th>
         <td>{bootver}</td>
      </tr>
      <tr>
         <th>CPU Frequency</th>
         <td>{cpufreq}</td>
      </tr>      
      <tr>
         <th>Real Flash Size</th>
         <td>{flashsize} bytes</td>
      </tr>
      <tr>
         <th>Flash Size</th>
         <td>{idesize} bytes</td>
      </tr>        
      <tr>
         <th>Memory</th>
         <td><progress value='{memsmeter_current}' max='{memsmeter_max}'></progress></td>
      </tr>
      <tr>
         <th>Memory Sketch Size</th>
         <td>{memsketch_current} / {memsketch_max}</td>
      </tr> 
      <tr>
         <th>Memory Free Heap</th>
         <td>{freeheap} bytes available</td>
      </tr>               
   </tbody>
</table>
<br/>
<form action="/">
  <button type="submit">Back</button></form>
</form>
)=====";

const char CONFIG_PAGE[] PROGMEM = R"=====(
<div style='text-align:left;display:inline-block;color:#000;min-width:340px;'>
  <p>
    <form style="display: block;" action="/system"><button type="submit">System</button></form>
  </p>
  <p>
    <form style="display: block;" action="/hue"><button type="submit">HUE Emulator</button></form>
  </p>
  <p>
    <form style="display: block;" action="/mqtt"><button type="submit">MQTT</button></form>
  </p>
  <p>
    <form style="display: block;" action="/wifi"><button type="submit">Network</button></form>
  </p>
  <p>
    <form action="/">
      <button type="submit">Back</button>
    </form>
  </p>
</div>
)=====";

const char HTTP_FORM_WIFI[] PROGMEM = R"=====(
<p><button type='button' onclick='window.location.reload();'>Refresh</button></p>
<br/>  
<fieldset>
  <legend><b>{title}</b></legend>
  <form action='/wifisave' method='POST'>
    <p><b><label for='ssid'>SSID</label><input id='ssid' name='ssid' maxlength='32' autocorrect='off' autocapitalize='none' value='{ssid}'></b></p>
    <p><b><label for='password'>Password</label><input id='password' name='password' maxlength='64' type='text' value='{password}'></b></p>
    <p><button type="submit" name="save-ssid" value="on" class='danger'>Save</button></p>
  </form>
</fieldset>  
)=====";

const char HTTP_FORM_WIFI_PARAMS[] PROGMEM = R"=====(
<br/>
<fieldset>
   <legend><b>Custom parameters</b></legend>
   <form method="POST" action="/wifisave">
      <p><label><input style="width:50px;" type="checkbox" {checked} name="static"><b>Enable static</b></label></p>
      <p><b>IP address</b><br><input placeholder="{pip}" value="{ip}" name="ip"></p>
      <p><b>Subnet</b><br><input placeholder="{psubnet}" value="{subnet}" name="subnet"></p>
      <p><b>Gateway</b><br><input placeholder="{pgateway}" value="{gateway}" name="gateway"></p>
      <p><b>DNS</b><br><input placeholder="{pdns}" value="{dns}" name="dns"></p>
      <br><button name="save-network" type="submit">Save</button>
   </form>
</fieldset>
)=====";

const char HTTP_FORM_WIFI_BACK[] PROGMEM = R"=====(
<p>
  <form action="/config">
    <button type="submit">Back</button>
  </form>
</p>  
)=====";

const char SYSTEM_PAGE[] PROGMEM = R"=====(
<fieldset>
   <legend><b>{title}</b></legend>
   <form method="POST" action="/systemsave">
      <p><b>Opening time</b><br><input type="number" value="{opentime}" name="opentime"></p>
      <p><b>Closing time</b><br><input type="number" value="{closetime}" name="closetime"></p>
      <p><label><input style="width:50px;" type="checkbox" {synclight} name="synclight"><b>Sync light with lampshade</b></label></p>
      <p><b>Learning max drop voltage</b><br><input type="number" value="{maxdropvoltage}" name="maxdropvoltage"></p>
      <p><b>Learning max detection time</b><br><input type="number" value="{maxdetectiontime}" name="maxdetectiontime"></p>
      <br><button name="save" type="submit">Save</button>
   </form>
</fieldset>
<br/>
<p>
  <form style="display: block;" action="/update"><button type="submit">Check update</button></form>
</p>
<p>
  <button type="button" style="background-color:#ff0000;" onclick="confirmLearning();">Learning</button>
</p>
<p>
  <button type="button" style="background-color:#ff0000;" onclick="confirmReset();">Reset</button>
</p>
<br/>
<form action="/config">
  <button type="submit">Back</button>
</form>
)=====";

const char MQTT_PAGE[] PROGMEM = R"=====(
<fieldset>
   <legend><b>{title}</b></legend>
   <form method="POST" action="/mqttsave">
      <p><label><input style="width:50px;" type="checkbox" {checked} name="enable"><b>Enable</b></label></p>
      <p><b>Host</b><br><input value="{host}" name="host"></p>
      <p><b>Port</b><br><input type="number" value="{port}" name="port"></p>
      <p><b>Client</b><br><input value="{client}" value="{client}" name="client"></p>
      <p><b>User</b><br><input value="{user}" name="user"></p>
      <p><b>Password</b><br><input value="{pass}" name="pass"></p>
      <p><b>Topic</b><br><input value="{topic}" name="topic"></p>
      <p><b>Full Topic</b><br><input value="{fulltopic}" name="fulltopic"></p>
      <br><button name="save" type="submit">Save</button>
   </form>
</fieldset>
<br/>
<form action="/config">
  <button type="submit">Back</button>
</form>
)=====";

const char HUE_PAGE[] PROGMEM = R"=====(
<fieldset>
   <legend><b>{title}</b></legend>
   <form method="POST" action="/huesave">
      <p><label><input style="width:50px;" type="checkbox" {checked} name="enable"><b>Enable</b></label></p>
      <p><b>Light (device name)</b><br><input value="{lightname}" name="light-name"></p>
      <p><b>Lampshade (device name)</b><br><input value="{lampshadename}" name="lampshade-name"></p>
      <br><button name="save" type="submit">Save</button>
   </form>
</fieldset>
<br/>
<form action="/config">
  <button type="submit">Back</button>
</form>
)=====";


const char HTTP_HEAD_START[]       PROGMEM = R"=====(
<!DOCTYPE html>
    <html>
        <head>
        <meta charset='UTF-8'>
        <meta  name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'/>
        <title>{title}</title>
)=====";

const char HTTP_SCRIPT[] PROGMEM =  R"=====(
<script>
function confirmLearning() {
    if (confirm('Are you sure ?')) {
        fetch('cmd?learning');
    }
}

function confirmReset() {
    if (confirm('Are you sure to reset ?')) {
        fetch('cmd?reset');
    }
}

function wifiAutoComplete(el) {
    document.getElementById('ssid').value = el.innerText || el.textContent;
    p = el.nextElementSibling.classList.contains('lock');
    document.getElementById('password').disabled = !p;
    if (p) {
        document.getElementById('password').value = ''; 
        document.getElementById('password').focus();
    }
}
</script>
)=====";


const char JS_lib[] PROGMEM = R"=====(
var ft, lt;
var xhr = null;

function getStatus() {

    clearTimeout(ft);
    clearTimeout(lt);
    if (xhr != null) {
        xhr.abort()
    }
    xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      
        var btnLampshade = document.getElementById('lampshade-action');

        if (xhr.readyState == 4 && xhr.status == 200) {

            var data = JSON.parse(xhr.responseText);

            if (data.success) {
                if (data.status) {

                    document.getElementById('light-status').innerHTML = data.status.light.toUpperCase();
                    document.getElementById('lampshade-status').innerHTML = data.status.lampshade.toUpperCase();

             
                    if (data.status.lampshade == 'closed') {
                        document.getElementById('lampshade-status').style.color = '#CB0603';
                        btnLampshade.removeAttribute('disabled');
                        btnLampshade.innerHTML = 'Open';
                        btnLampshade.setAttribute('data-action', 'open');
                    } else if (data.status.lampshade == 'opened') {
                        document.getElementById('lampshade-status').style.color = '#27CB03';
                        btnLampshade.removeAttribute('disabled');
                        btnLampshade.innerHTML = 'Close';
                        btnLampshade.setAttribute('data-action', 'close');
                    } else if (data.status.lampshade == 'closing' || data.status.lampshade == 'opening') {
                        btnLampshade.setAttribute('disabled', 'true');
                    }
                }
            } else {
                btnLampshade.removeAttribute('disabled');
            }

            clearTimeout(ft);
            clearTimeout(lt);
            lt = setTimeout(getStatus, 2500);
        }
    };
    xhr.open('GET', 'cmd?status', true);
    xhr.send();
    ft = setTimeout(getStatus, 20000);
}

window.addEventListener('load', getStatus());
)=====";

const char HTTP_STYLE[] PROGMEM = R"=====(
<style>
body {
  text-align: center;
  font-family: verdana
}

div,
input {
  padding: 5px;
  font-size: 1em;
  margin: 5px 0;
  box-sizing: border-box;
}

input,
button,
.msg {
  border-radius: .3rem;
  width: 100%
}

,
input[type=radio] {
  width: auto
}

button,
input[type='button'],
input[type='submit'] {
  cursor: pointer;
  border: 0;
  background-color: #1fa3ec;
  color: #fff;
  line-height: 2.4rem;
  font-size: 1.2rem;
  width: 100%
}

.wrap {
  text-align: left;
  display: inline-block;
  min-width: 340px;
  max-width: 500px
}

a {
  color: #000;
  font-weight: 700;
  text-decoration: none
}

a:hover {
  color: #1fa3ec;
  text-decoration: underline
}

.signal {
  height: 16px;
  margin: 0;
  padding: 0 5px;
  text-align: right;
  min-width: 38px;
  float: right
}

.signal.level-0:after {
  background-position-x: 0
}

.signal.level-1:after {
  background-position-x: -16px
}

.signal.level-2:after {
  background-position-x: -32px
}

.signal.level-3:after {
  background-position-x: -48px
}

.signal.level-4:after {
  background-position-x: -64px
}

.signal.lock:before {
  background-position-x: -80px;
  padding-right: 5px
}

.signal:after,
.signal:before {
  content: '';
  width: 16px;
  height: 16px;
  display: inline-block;
  background-repeat: no-repeat;
  background-position: 16px 0;
  background-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGAAAAAQCAMAAADeZIrLAAAAJFBMVEX///8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADHJj5lAAAAC3RSTlMAIjN3iJmqu8zd7vF8pzcAAABsSURBVHja7Y1BCsAwCASNSVo3/v+/BUEiXnIoXkoX5jAQMxTHzK9cVSnvDxwD8bFx8PhZ9q8FmghXBhqA1faxk92PsxvRc2CCCFdhQCbRkLoAQ3q/wWUBqG35ZxtVzW4Ed6LngPyBU2CobdIDQ5oPWI5nCUwAAAAASUVORK5CYII=');
}

@media (-webkit-min-device-pixel-ratio: 2),
(min-resolution: 192dpi) {
  .signal:before,
  .signal:after {
    background-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAALwAAAAgCAMAAACfM+KhAAAALVBMVEX///8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADAOrOgAAAADnRSTlMAESIzRGZ3iJmqu8zd7gKjCLQAAACmSURBVHgB7dDBCoMwEEXRmKlVY3L//3NLhyzqIqSUggy8uxnhCR5Mo8xLt+14aZ7wwgsvvPA/ofv9+44334UXXngvb6XsFhO/VoC2RsSv9J7x8BnYLW+AjT56ud/uePMdb7IP8Bsc/e7h8Cfk912ghsNXWPpDC4hvN+D1560A1QPORyh84VKLjjdvfPFm++i9EWq0348XXnjhhT+4dIbCW+WjZim9AKk4UZMnnCEuAAAAAElFTkSuQmCC');
    background-size: 95px 16px;
  }
}

.msg {
  padding: 20px;
  margin: 20px 0;
  border: 1px solid #eee;
  border-left-width: 5px;
  border-left-color: #777
}

.msg h4 {
  margin-top: 0;
  margin-bottom: 5px
}

.msg.primary {
  border-left-color: #1fa3ec
}

.msg.primary h4 {
  color: #1fa3ec
}

.msg.danger {
  border-left-color: #dc3630
}

.msg.danger h4 {
  color: #dc3630
}

.msg.S {
  border-left-color: #5cb85c
}

.msg.S h4 {
  color: #5cb85c
}

td {
  vertical-align: top;
}

.h {
  display: none
}

button.danger {
  background-color: #dc3630
}

input:disabled {
  opacity: 0.5;
}

button:disabled {
  opacity: 0.5;
}
</style>
)=====";

const char HTTP_REFRESH[] PROGMEM = "<meta http-equiv='refresh' content='{time};url={url}' />";

const char HTTP_HEAD_END[] PROGMEM = "</head><body><div class='wrap'>";

const char HTTP_END[] PROGMEM = "<div style='text-align:right;font-size:11px;'><hr><a href='https://www.domotique-passion.fr/' target='_blank'>ECLIPSE by Maxime Maucourant</a></div></div></body></html>";

const char HTTP_WIFI_CONNECT[] PROGMEM = "<strong style='text-align:center;'>Try connect to '{ssid}' network</strong>";
const char HTTP_WIFI_CONNECT_SUCCESS[] PROGMEM = "<strong style='text-align:center;'>Connection successful</strong><br/><span>Click to redirect to <a href='{url}'>{url}</a> or wait {time} seconds</span>";
const char HTTP_WIFI_CONNECT_FAIL[] = "<strong style='text-align:center;'>Failed to connect</strong><br/><span>Click to redirect to <a href='{url}'>{url}</a> or wait {time} seconds</span>";
const char HTTP_WIFI_PARAMS_SUCCESS[] PROGMEM = "<div style='text-align:center;'><strong>Network parameters saved</strong><br/><p>System restart in progess...</p><p>Please wait {time} seconds.</p></div>";

const char HTTP_MQTT_SUCCESS[] PROGMEM = "<div style='text-align:center;'><strong>MQTT Configuration saved</strong><br/><p>System restart in progess...</p><p>Please wait {time} seconds.</p></div>";

const char HTTP_HUE_SUCCESS[] PROGMEM = "<div style='text-align:center;'><strong>HUE Configuration saved</strong><br/><p>System restart in progess...</p><p>Please wait {time} seconds.</p></div>";

const char HTTP_SYSTEM_SUCCESS[] PROGMEM = "<div style='text-align:center;'><strong>System Configuration saved</strong><br/><p>Please wait {time} seconds.</p></div>";

const char HTTP_UPDATE_START[] PROGMEM = "<div style='text-align:center;'><strong>Update in progress...</strong><br/><p>Please wait {time} seconds.</p></div>";
const char HTTP_UPDATE_RESULT[] PROGMEM = "<div style='text-align:center;'><strong>{updateStatus}</strong></div>";

const char HTTP_ITEM_QI[] PROGMEM = "<div role='img' aria-label='{power}%' title='{power}%' class='signal level-{level} {lock}'></div>";
const char HTTP_ITEM_QP[] PROGMEM = "<div class='signal'>{power}%</div>";
const char HTTP_ITEM[] PROGMEM = "<div><a href='#' onclick='wifiAutoComplete(this)'>{ssid}</a>{qi}{qp}</div>"; 

const char NO_NETWORKS[] PROGMEM = "No networks found. Refresh to scan again.";