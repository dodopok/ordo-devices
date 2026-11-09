#pragma once

#include <pgmspace.h>

// The page a phone sees after joining the compass' own network: the Wi-Fi
// networks in range as a list, a password field, and what happened. One
// file, no external resource (the phone has no internet at this point), in
// the Ordo app's dark theme. Portuguese, English or Spanish by the phone's
// language.
//
// Talks to three routes of net/wifi.cpp:
//   GET  /api/networks[?rescan=1]  -> { scanning, networks: [{ ssid, rssi, secure }] }
//   POST /api/connect  ssid, pass  -> { ok }
//   GET  /api/status               -> { state: waiting|connecting|connected|failed, ssid }
static const char ORDO_PORTAL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>Ordo · Wi-Fi</title>
<style>
:root{--bg:#0F1115;--surface:#171A20;--raised:#1E222A;--line:#262B33;--fg:#E8E6E3;--muted:#A3A3A0;--faint:#6B6F78;--gold:#E8C968;--green:#66BB6A;--red:#EF5350;--blue:#7FB0FF}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
html,body{background:var(--bg);color:var(--fg);font:16px/1.45 "Raleway","Avenir Next","Segoe UI",system-ui,-apple-system,sans-serif}
main{max-width:440px;margin:0 auto;padding:max(28px,env(safe-area-inset-top)) 20px 40px;display:flex;flex-direction:column;gap:22px;min-height:100vh}
header{display:flex;flex-direction:column;align-items:center;gap:10px;text-align:center;padding-top:6px}
header svg{width:84px;height:84px}
h1{font:500 34px/1 "Newsreader","Iowan Old Style","New York",Georgia,serif;letter-spacing:.01em}
.sub{color:var(--muted);font-size:15px}
h2{font:500 22px/1.2 "Newsreader","Iowan Old Style","New York",Georgia,serif}
.cap{font-size:11px;letter-spacing:.16em;text-transform:uppercase;color:var(--faint);font-weight:600;padding:0 4px}
.list{display:flex;flex-direction:column;border:1px solid var(--line);border-radius:18px;overflow:hidden;background:var(--surface)}
.net{display:flex;align-items:center;gap:12px;width:100%;padding:15px 16px;background:none;border:0;border-top:1px solid var(--line);color:var(--fg);font:inherit;text-align:left;cursor:pointer}
.net:first-child{border-top:0}
.net:active{background:var(--raised)}
.net .name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:500}
.net svg{flex:none;width:15px;height:15px;fill:var(--faint)}
.bars{flex:none;display:flex;align-items:flex-end;gap:2px;height:14px}
.bars i{width:3px;border-radius:1px;background:var(--line)}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}.bars i:nth-child(3){height:10px}.bars i:nth-child(4){height:14px}
.bars.l1 i:nth-child(-n+1),.bars.l2 i:nth-child(-n+2),.bars.l3 i:nth-child(-n+3),.bars.l4 i{background:var(--gold)}
.empty{padding:22px 16px;color:var(--muted);text-align:center;font-size:15px}
.row{display:flex;gap:10px}
button.btn{flex:1;padding:14px 16px;border-radius:14px;font:600 15px/1 inherit;font-family:inherit;border:1px solid var(--line);background:var(--surface);color:var(--fg);cursor:pointer}
button.btn.primary{background:var(--gold);border-color:var(--gold);color:#17130a}
button.btn:disabled{opacity:.5}
.card{background:var(--surface);border:1px solid var(--line);border-radius:18px;padding:20px;display:flex;flex-direction:column;gap:16px}
label.field{display:flex;flex-direction:column;gap:7px;font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:var(--faint);font-weight:600}
input[type=text],input[type=password]{width:100%;padding:14px;border-radius:12px;border:1px solid var(--line);background:var(--bg);color:var(--fg);font:16px/1.2 inherit;font-family:inherit;letter-spacing:0;text-transform:none}
input:focus{outline:2px solid var(--gold);outline-offset:1px}
label.check{display:flex;align-items:center;gap:10px;color:var(--muted);font-size:14px}
label.check input{width:18px;height:18px;accent-color:var(--gold)}
.state{display:flex;flex-direction:column;align-items:center;gap:14px;text-align:center;padding:26px 20px}
.spin{width:38px;height:38px;border-radius:50%;border:3px solid var(--line);border-top-color:var(--gold);animation:spin 1s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.ok{width:46px;height:46px;border-radius:50%;background:var(--green);display:grid;place-items:center}
.ok svg{width:24px;height:24px;fill:none;stroke:#0b1a0c;stroke-width:3;stroke-linecap:round;stroke-linejoin:round}
.err{color:var(--red);font-size:14px}
.hint{color:var(--muted);font-size:14px}
footer{margin-top:auto;color:var(--faint);font-size:12.5px;text-align:center;line-height:1.5}
[hidden]{display:none!important}
</style>
</head>
<body>
<main>
<header>
<svg viewBox="0 0 100 100" aria-hidden="true">
<circle cx="50" cy="50" r="42" fill="none" stroke="#1E222A" stroke-width="7"/>
<path d="M50 8a42 42 0 0 1 17.1 3.6" fill="none" stroke="#A99BD1" stroke-width="7"/>
<path d="M69.6 12.9a42 42 0 0 1 6.5 4.2" fill="none" stroke="#E8C968" stroke-width="7"/>
<path d="M78.6 19.2a42 42 0 0 1 12.6 22.3" fill="none" stroke="#66BB6A" stroke-width="7"/>
<path d="M91.7 44.6a42 42 0 0 1-2.6 20.9" fill="none" stroke="#A99BD1" stroke-width="7"/>
<path d="M87.9 68.3a42 42 0 0 1-1.5 2.7" fill="none" stroke="#EF5350" stroke-width="7"/>
<path d="M84.8 73.6a42 42 0 0 1-31.9 18.3" fill="none" stroke="#E8C968" stroke-width="7"/>
<path d="M49.8 92a42 42 0 0 1-.2-84" fill="none" stroke="#66BB6A" stroke-width="7"/>
<circle cx="35.6" cy="10.5" r="5.5" fill="#0F1115" stroke="#fff" stroke-width="2.5"/>
</svg>
<h1>Ordo</h1>
<p class="sub" data-t="sub"></p>
</header>

<section id="pick">
<p class="cap" data-t="nearby" style="margin-bottom:8px"></p>
<div class="list" id="list"><div class="empty" data-t="looking"></div></div>
<div class="row" style="margin-top:14px">
<button class="btn" id="rescan" data-t="rescan"></button>
<button class="btn" id="other" data-t="other"></button>
</div>
</section>

<section id="form" class="card" hidden>
<h2 id="chosen"></h2>
<label class="field" id="ssidField" hidden><span data-t="netname"></span><input type="text" id="ssid" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false"></label>
<label class="field" id="passField"><span data-t="password"></span><input type="password" id="pass" maxlength="63" autocapitalize="none" autocorrect="off" spellcheck="false" autocomplete="off"></label>
<label class="check"><input type="checkbox" id="show"><span data-t="show"></span></label>
<p class="err" id="error" hidden></p>
<div class="row">
<button class="btn" id="back" data-t="back"></button>
<button class="btn primary" id="join" data-t="join"></button>
</div>
</section>

<section id="busy" class="card state" hidden>
<div class="spin"></div>
<h2 id="busyTitle"></h2>
<p class="hint" data-t="busyHint"></p>
</section>

<section id="done" class="card state" hidden>
<div class="ok"><svg viewBox="0 0 24 24"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg></div>
<h2 id="doneTitle"></h2>
<p class="hint" data-t="doneHint"></p>
</section>

<footer data-t="foot"></footer>
</main>
<script>
var L=(navigator.language||'pt').slice(0,2);
var T={pt:{sub:'Escolha o Wi-Fi que a bússola vai usar',nearby:'Redes por perto',looking:'Procurando redes…',none:'Nenhuma rede encontrada. Chegue mais perto do roteador e procure de novo.',rescan:'Procurar de novo',other:'Outra rede',netname:'Nome da rede',password:'Senha',show:'Mostrar a senha',back:'Voltar',join:'Conectar',connecting:'Conectando a ',busyHint:'Esta página pode parar de responder por um instante. O resultado também aparece na tela da bússola.',connected:'A bússola entrou em ',doneHint:'Pronto. Pode fechar esta página e voltar ao seu Wi-Fi.',failed:'Não conectou. Confira a senha e tente de novo.',short:'A senha de um Wi-Fi tem pelo menos 8 caracteres.',needName:'Escreva o nome da rede.',lost:'Sem resposta da bússola. Veja na tela dela se a conexão deu certo.',foot:'A bússola guarda a senha só nela e usa a rede apenas para buscar o seu dia litúrgico.'},
en:{sub:'Pick the Wi-Fi the compass will use',nearby:'Networks nearby',looking:'Looking for networks…',none:'No network found. Move closer to the router and look again.',rescan:'Look again',other:'Other network',netname:'Network name',password:'Password',show:'Show the password',back:'Back',join:'Connect',connecting:'Connecting to ',busyHint:'This page may stop answering for a moment. The result also shows on the compass.',connected:'The compass joined ',doneHint:'Done. You can close this page and go back to your Wi-Fi.',failed:'Could not connect. Check the password and try again.',short:'A Wi-Fi password has at least 8 characters.',needName:'Type the network name.',lost:'No answer from the compass. Check its screen to see whether it connected.',foot:'The compass keeps the password to itself and uses the network only to fetch your liturgical day.'},
es:{sub:'Elija el Wi-Fi que usará la brújula',nearby:'Redes cercanas',looking:'Buscando redes…',none:'No se encontró ninguna red. Acérquese al router y busque de nuevo.',rescan:'Buscar de nuevo',other:'Otra red',netname:'Nombre de la red',password:'Contraseña',show:'Mostrar la contraseña',back:'Volver',join:'Conectar',connecting:'Conectando a ',busyHint:'Esta página puede dejar de responder un momento. El resultado también aparece en la brújula.',connected:'La brújula entró en ',doneHint:'Listo. Puede cerrar esta página y volver a su Wi-Fi.',failed:'No se conectó. Revise la contraseña e inténtelo de nuevo.',short:'Una contraseña de Wi-Fi tiene al menos 8 caracteres.',needName:'Escriba el nombre de la red.',lost:'La brújula no responde. Mire su pantalla para saber si se conectó.',foot:'La brújula guarda la contraseña solo en ella y usa la red únicamente para traer su día litúrgico.'}};
var t=T[L]||T.pt;
document.documentElement.lang=T[L]?L:'pt';
[].forEach.call(document.querySelectorAll('[data-t]'),function(e){e.textContent=t[e.getAttribute('data-t')]});
function $(id){return document.getElementById(id)}
var chosen='',secure=true,manual=false,tries=0;
function show(id){['pick','form','busy','done'].forEach(function(s){$(s).hidden=s!==id})}
function bars(rssi){return rssi>-55?4:rssi>-67?3:rssi>-78?2:1}
function render(nets){
  var list=$('list');list.textContent='';
  if(!nets.length){var e=document.createElement('div');e.className='empty';e.textContent=t.none;list.appendChild(e);return}
  nets.forEach(function(n){
    var b=document.createElement('button');b.className='net';b.type='button';
    var name=document.createElement('span');name.className='name';name.textContent=n.ssid;b.appendChild(name);
    if(n.secure){var s=document.createElementNS('http://www.w3.org/2000/svg','svg');s.setAttribute('viewBox','0 0 16 16');
      var p=document.createElementNS('http://www.w3.org/2000/svg','path');p.setAttribute('d','M4.5 7V5a3.5 3.5 0 0 1 7 0v2H12a1 1 0 0 1 1 1v5a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V8a1 1 0 0 1 1-1h.5zm1.5 0h4V5a2 2 0 0 0-4 0v2z');s.appendChild(p);b.appendChild(s)}
    var g=document.createElement('span');g.className='bars l'+bars(n.rssi);g.innerHTML='<i></i><i></i><i></i><i></i>';b.appendChild(g);
    b.onclick=function(){pick(n.ssid,n.secure)};
    list.appendChild(b)})}
function load(rescan){
  fetch('/api/networks'+(rescan?'?rescan=1':'')).then(function(r){return r.json()}).then(function(d){
    if(d.scanning&&!d.networks.length){var e=$('list');e.textContent='';var m=document.createElement('div');m.className='empty';m.textContent=t.looking;e.appendChild(m)}
    else render(d.networks);
    if(d.scanning)setTimeout(function(){load(false)},1300)
  }).catch(function(){setTimeout(function(){load(false)},2000)})}
function pick(ssid,sec){
  manual=false;chosen=ssid;secure=sec;$('chosen').textContent=ssid;$('ssidField').hidden=true;$('passField').hidden=!sec;$('pass').value='';$('error').hidden=true;
  if(!sec){join();return}
  show('form');$('pass').focus()}
function fail(msg){$('error').textContent=msg;$('error').hidden=false;show('form')}
function join(){
  var ssid=manual?$('ssid').value.trim():chosen,pass=$('pass').value;
  if(!ssid){fail(t.needName);return}
  if((manual||secure)&&pass.length>0&&pass.length<8){fail(t.short);return}
  if(secure&&!manual&&pass.length<8){fail(t.short);return}
  chosen=ssid;$('busyTitle').textContent=t.connecting+ssid;show('busy');tries=0;
  var body='ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);
  fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(function(){setTimeout(poll,1200)}).catch(function(){setTimeout(poll,1500)})}
function poll(){
  fetch('/api/status').then(function(r){return r.json()}).then(function(d){
    tries=0;
    if(d.state==='connected'){$('doneTitle').textContent=t.connected+chosen;show('done');return}
    if(d.state==='failed'){fail(t.failed);return}
    setTimeout(poll,1000)
  }).catch(function(){
    if(++tries>25){$('busyTitle').textContent=t.lost;return}
    setTimeout(poll,1200)})}
$('rescan').onclick=function(){var e=$('list');e.textContent='';var m=document.createElement('div');m.className='empty';m.textContent=t.looking;e.appendChild(m);load(true)};
$('other').onclick=function(){manual=true;secure=true;$('chosen').textContent=t.other;$('ssidField').hidden=false;$('passField').hidden=false;$('ssid').value='';$('pass').value='';$('error').hidden=true;show('form');$('ssid').focus()};
$('back').onclick=function(){show('pick')};
$('join').onclick=join;
$('show').onchange=function(){$('pass').type=this.checked?'text':'password'};
$('pass').onkeydown=function(e){if(e.key==='Enter')join()};
load(false);
</script>
</body>
</html>
)HTML";
