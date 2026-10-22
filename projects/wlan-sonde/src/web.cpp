#include "web.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "anzeige.h"
#include "einstellungen.h"
#include "messung.h"
#include "sender.h"
#include "speicher.h"

static WebServer server(80);

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN-Sonde</title>
<style>
:root{--bg:#fff;--fg:#1f2328;--mut:#59636e;--line:#d1d9e0;--card:#f6f8fa;--acc:#0969da}
@media(prefers-color-scheme:dark){:root{--bg:#0d1117;--fg:#e6edf3;--mut:#9198a1;--line:#30363d;--card:#161b22;--acc:#4493f8}}
*{box-sizing:border-box}
body{margin:0 auto;max-width:58em;padding:12px 16px 40px;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
h1{font-size:1.35em;margin:4px 0}h2{font-size:1.05em;margin:0 0 8px}a{color:var(--acc)}
.mut{color:var(--mut)}.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 16px;margin:12px 0}
.big{font-size:2.2em;font-weight:700}.tag{display:inline-block;padding:1px 8px;border-radius:9px;background:var(--acc);color:#fff;font-size:.7em;vertical-align:middle}
table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:5px 6px;border-bottom:1px solid var(--line);vertical-align:top}
.n{text-align:right;font-variant-numeric:tabular-nums}.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px}
button,input,select{font:inherit;padding:6px 10px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg)}
button{cursor:pointer}button.pri{background:var(--acc);color:#fff;border-color:var(--acc)}button:disabled{opacity:.5}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(16em,1fr));gap:12px}
.zone{border:1px solid var(--line);border-radius:8px;padding:8px 10px;background:var(--bg)}
.sd{padding:6px 0;border-top:1px solid var(--line);margin-top:6px}
.ev{font-family:ui-monospace,monospace;font-size:.85em;white-space:pre-wrap}.hide{display:none}
.bar{height:10px;border-radius:5px;background:var(--line);overflow:hidden}.bar>div{height:100%;background:var(--acc);transition:width .3s}
label{display:block;margin:8px 0 2px;font-size:.9em;color:var(--mut)}label input,label select{width:100%}
</style></head><body>
<h1 id="titel">WLAN-Sonde</h1><div class="mut" id="sub">lade …</div>

<div class="card">
 <div class="row" style="justify-content:space-between;align-items:baseline">
  <div><span class="big" id="rssi">–</span> dBm <span id="bew"></span></div><div class="mut" id="mittel"></div></div>
 <table><tr><td>Verbunden mit</td><td id="bssid"></td></tr><tr><td>Kanal</td><td id="kanal"></td></tr>
 <tr><td>Abbrüche / AP-Wechsel</td><td id="ab"></td></tr><tr><td>Laufzeit</td><td id="up"></td></tr></table>
</div>

<div class="card hide" id="k-mobil"><h2>Begehung – Messpunkt erfassen</h2>
 <div class="row"><input id="p-id" placeholder="Punkt, z. B. A-EG-1" size="16">
  <select id="p-ber"><option>A</option><option>Halle</option><option>B</option></select>
  <button class="pri" id="p-start" onclick="punkt()">Messen</button></div>
 <div class="bar" style="margin:10px 0"><div id="p-bar" style="width:0"></div></div>
 <table id="punkte"></table>
 <div class="row" style="margin-top:8px"><a href="/api/export">Export (JSONL)</a>
  <button onclick="if(confirm('Letzten Messpunkt löschen?'))post('/api/punkt/loeschen','letzter','text/plain')">Letzten löschen</button>
  <button onclick="if(confirm('ALLE Messpunkte löschen?'))post('/api/punkt/loeschen','alle','text/plain')">Alle löschen</button></div>
</div>

<div class="card hide" id="k-zentrale"><h2>Sonden</h2><div class="grid" id="zonen"></div>
 <p><a href="/api/export">Export aller gesammelten Messungen (JSONL)</a> <span class="mut" id="z-puffer"></span></p></div>

<div class="card hide" id="k-knoten"><h2>Mesh-Knoten (2,4 GHz)</h2><table id="knoten"></table>
 <p class="mut">Namen vergeben, z. B. „FRITZ!Box“ oder „Repeater Halle“. Ein Knoten, der an einer Sonde
 sehr stark ankommt, steht meist in ihrer Nähe.</p></div>

<div class="card hide" id="k-messung"><h2>Letzte Messung <span class="mut" id="m-zeit"></span></h2>
 <table id="m-werte"></table>
 <h2 style="margin-top:12px">Knoten mit gleicher SSID</h2><table id="m-knoten"></table>
 <h2 style="margin-top:12px">Fremde Netze je Kanal</h2><div id="m-fremd" class="mut"></div>
 <div class="row" style="margin-top:10px" id="m-aktion"><button onclick="post('/api/messen','')">Jetzt messen</button>
  <a href="/api/export">Export (JSONL)</a></div></div>

<div class="card"><h2>Ereignisse</h2><div class="ev" id="ev"></div></div>

<details class="card"><summary><b>Einstellungen</b></summary>
<form id="f" onsubmit="return speichern(event)">
 <label>Name (Hostname, erreichbar als name.local)<input name="name"></label>
 <label>Rolle<select name="rolle"><option value="sonde">Sonde – fest montiert, misst regelmäßig</option>
  <option value="mobil">Mobil – Begehung mit Messpunkten</option><option value="zentrale">Zentrale – sammelt alle Sonden</option></select></label>
 <label>Bereich<input name="bereich" list="bl"><datalist id="bl"><option>A</option><option>Halle</option><option>B</option></datalist></label>
 <label>Ort (Beschreibung)<input name="ort" placeholder="z. B. Halle Mitte, Säule 4, 3 m hoch"></label>
 <label>Zentrale (nur Rolle Sonde)<input name="zentrale" placeholder="http://wlan-zentrale.local"></label>
 <label>Messintervall in Sekunden<input name="intervall" type="number" min="10" max="3600"></label>
 <div class="row" style="margin-top:12px"><button class="pri">Speichern &amp; neu starten</button>
  <button type="button" onclick="if(confirm('WLAN-Zugangsdaten löschen und Hotspot öffnen?'))post('/api/reset-wifi','')">WLAN zurücksetzen</button></div>
</form></details>

<script>
const $=id=>document.getElementById(id);
const col=r=>r==null?'#8c959f':r>=-55?'#1a7f37':r>=-65?'#6cc644':r>=-72?'#d4a72c':r>=-80?'#e16f24':'#cf222e';
const dot=r=>`<span class="dot" style="background:${col(r)}"></span>`;
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const dauer=s=>s>=3600?`${Math.floor(s/3600)} h ${Math.floor(s/60)%60} min`:s>=60?`${Math.floor(s/60)} min ${s%60} s`:`${s} s`;
const zeit=(t,u)=>t?new Date(t*1000).toLocaleString('de-DE'):`nach ${dauer(u)} Laufzeit`;
const spalte=b=>{b=String(b||'').trim().toLowerCase();if(b.includes('hall'))return 1;if(/^a|[ -]a$/.test(b))return 0;if(/^b|[ -]b$/.test(b))return 2;return 1};
const BEREICHE=['Gebäude A','Halle','Gebäude B'];
let S=null,init=false,namen={},punkteZahl=-1;

async function post(u,b,t='application/json'){
 const r=await fetch(u,{method:'POST',headers:{'Content-Type':t},body:b});
 if(!r.ok)alert('Fehler: '+(await r.text()||r.status));load();return r}
const antw=m=>m.antwort_ms==null?'keine Antwort':`${m.antwort_ms} ms`;
const kname=b=>namen[b]?`${esc(namen[b])} <span class="mut">${b}</span>`:b;

function werte(m){return `<tr><td>Signal Mittel / min / max</td><td>${m.verbunden?`${dot(m.rssi)}${m.rssi} / ${m.rssi_min} / ${m.rssi_max} dBm`:'<b>nicht verbunden</b>'}</td></tr>
 <tr><td>Antwortzeit Router</td><td>${antw(m)}, Verlust ${m.verlust} %</td></tr>`}
function knotenTab(m){
 if(!m.knoten||!m.knoten.length)return '<tr><td class="mut">keine gefunden</td></tr>';
 return '<tr><th>BSSID</th><th class="n">dBm</th><th class="n">Kanal</th><th></th></tr>'+m.knoten.map(k=>
  `<tr><td>${dot(k.rssi)}${kname(k.bssid)}</td><td class="n">${k.rssi}</td><td class="n">${k.kanal}</td><td>${k.bssid===m.bssid?'<b>verbunden</b>':''}</td></tr>`).join('')}
const bester=m=>(m.knoten||[]).reduce((a,k)=>!a||k.rssi>a.rssi?k:a,null);
const fremd=m=>!m.fremd||!m.fremd.length?'keine':m.fremd.map(f=>`Kanal ${f.kanal}: ${f.anzahl}× (stärkstes ${f.max} dBm)`).join(' · ');

async function punkt(){
 await post('/api/punkt',JSON.stringify({punkt:$('p-id').value.trim(),bereich:$('p-ber').value}));
 const id=$('p-id').value.match(/^(.*?)(\d+)$/);if(id)$('p-id').value=id[1]+(+id[2]+1)}
async function ladePunkte(n){
 if(n===punkteZahl)return;punkteZahl=n;
 const p=(await (await fetch('/api/punkte')).json()).reverse();
 $('punkte').innerHTML=p.length?'<tr><th>Punkt</th><th>Bereich</th><th class="n">dBm</th><th>Bester Knoten</th><th class="n">Antwort</th></tr>'+p.map(x=>
  `<tr><td>${esc(x.punkt)}</td><td>${esc(x.bereich)}</td><td class="n">${x.verbunden?dot(x.rssi)+x.rssi:'–'}</td>
  <td>${(b=>b?dot(b.rssi)+b.rssi+' dBm '+(b.bssid===x.bssid?'(verbunden)':'<b>(nicht verbunden!)</b>'):'–')(bester(x))}</td>
  <td class="n">${antw(x)}</td></tr>`).join(''):'<tr><td class="mut">Noch keine Messpunkte.</td></tr>'}
async function ladeZentrale(){
 const [so,kn]=await Promise.all([fetch('/api/sonden').then(r=>r.json()),fetch('/api/knoten').then(r=>r.json())]);
 namen={};kn.forEach(k=>{if(k.name)namen[k.bssid]=k.name});
 const z=[[],[],[]];so.forEach(s=>z[spalte(s.bereich)].push(s));
 $('zonen').innerHTML=z.map((l,i)=>`<div class="zone"><b>${BEREICHE[i]}</b>${l.length?l.map(s=>{const m=s.messung;
  const st=s.veraltet?`<b>keine Meldung seit ${dauer(s.alter)}</b>`:!m?'–':m.verbunden?`${dot(m.rssi)}${m.rssi} dBm · ${antw(m)} · Verlust ${m.verlust} %`:'<b>nicht verbunden</b>';
  return `<div class="sd">${esc(s.name)}${s.eigen?' <span class="tag">Zentrale</span>':''} <span class="mut">${esc(s.ort)}</span><br>${st}
  ${m&&m.verbunden?`<br><span class="mut">an ${kname(m.bssid)} · Abbr. ${m.abbrueche} · Wechsel ${m.wechsel}</span>`:''}</div>`}).join(''):'<div class="sd mut">keine Sonde</div>'}</div>`).join('');
 $('knoten').innerHTML=kn.length?'<tr><th>Name / BSSID</th><th class="n">Kanal</th><th>Am stärksten bei</th><th class="n">Sonden</th></tr>'+kn.map(k=>
  `<tr><td><div class="row"><input value="${esc(k.name)}" placeholder="${k.bssid}" data-b="${k.bssid}" size="16"><button onclick="nm(this)">OK</button></div><span class="mut">${k.bssid}</span></td>
  <td class="n">${k.kanal}</td><td>${dot(k.rssi)}${k.rssi} dBm · ${esc(k.sonde)}</td><td class="n">${k.gesehen} / ${k.verbunden}</td></tr>`).join(''):'<tr><td class="mut">noch keine Messungen</td></tr>';
 $('z-puffer').textContent=`(${S.puffer.anzahl} von max. ${S.puffer.kapazitaet} gespeichert)`}
function nm(b){const i=b.previousElementSibling;post('/api/knoten',JSON.stringify({bssid:i.dataset.b,name:i.value.trim()}))}
function speichern(e){e.preventDefault();
 post('/api/config',new URLSearchParams(new FormData($('f'))),'application/x-www-form-urlencoded');
 alert('Gespeichert – das Gerät startet neu.');return false}

async function load(){
 try{S=await (await fetch('/api/status')).json()}catch(e){$('sub').textContent='keine Verbindung zum Gerät';return}
 const s=S,r=s.rolle;
 $('titel').innerHTML=`${esc(s.name)} <span class="tag">${r}</span>`;
 $('sub').textContent=[s.bereich&&'Bereich '+s.bereich,s.ort,s.ssid,s.ip].filter(Boolean).join(' · ');
 $('rssi').textContent=s.verbunden?s.rssi:'–';
 $('bew').innerHTML=s.verbunden?dot(s.rssi_mittel)+s.bewertung:'<b>nicht verbunden</b>';
 $('mittel').textContent=s.verbunden?`Mittel ${s.rssi_mittel} dBm`:'';
 $('bssid').innerHTML=s.bssid?kname(s.bssid):'–';
 $('kanal').textContent=s.kanal||'–';$('ab').textContent=`${s.abbrueche} / ${s.wechsel}`;$('up').textContent=dauer(s.uptime);
 const m=s.messung;$('k-messung').classList.toggle('hide',!m||r==='mobil');
 if(m&&r!=='mobil'){$('m-zeit').textContent=zeit(m.t,m.uptime);
  $('m-werte').innerHTML=werte(m)+(s.sender?`<tr><td>An Zentrale gesendet</td><td>${s.sender.gesendet}, offen ${s.sender.offen}${s.sender.letzter_fehler?' – <b>'+esc(s.sender.letzter_fehler)+'</b>':''}</td></tr>`:'');
  $('m-knoten').innerHTML=knotenTab(m);$('m-fremd').textContent=fremd(m)}
 $('ev').textContent=s.ereignisse.map(e=>`[${e.zeit?new Date(e.zeit*1000).toLocaleTimeString('de-DE'):dauer(e.uptime)}] ${e.text}`).join('\n');
 for(const [k,an] of [['k-mobil',r==='mobil'],['k-zentrale',r==='zentrale'],['k-knoten',r==='zentrale']])$(k).classList.toggle('hide',!an);
 $('p-bar').style.width=(s.phase!=='ruhe'?s.fortschritt:0)+'%';$('p-start').disabled=s.phase!=='ruhe';
 try{if(r==='mobil')await ladePunkte(s.punkte);if(r==='zentrale')await ladeZentrale()}catch(e){}
 if(!init){init=true;const f=$('f');for(const k of ['name','rolle','bereich','ort','zentrale','intervall'])f[k].value=s[k]??'';
  if(s.bereich)$('p-ber').value=['A','Halle','B'][spalte(s.bereich)]}}
async function schleife(){await load();setTimeout(schleife,S&&S.phase!=='ruhe'?500:3000)}
schleife();
</script></body></html>)rawliteral";

// --------------------------------- Hilfen -----------------------------------
static void senden(JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static bool istRolle(Rolle r) {
  if (cfg.rolle == r) return true;
  server.send(409, "text/plain", String("nur in der Rolle ") + rolleName(r));
  return false;
}

// --------------------------------- Abfragen ---------------------------------
static void handleStatus() {
  JsonDocument doc;
  doc["name"] = cfg.name;
  doc["rolle"] = rolleName(cfg.rolle);
  doc["bereich"] = cfg.bereich;
  doc["ort"] = cfg.ort;
  doc["zentrale"] = cfg.zentrale;
  doc["intervall"] = cfg.intervall;
  doc["ssid"] = WiFi.SSID();
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["uptime"] = millis() / 1000;
  doc["zeit"] = jetztUnix();

  const int32_t mittel = (int32_t)lroundf(verbindung.rssiMittel);
  doc["verbunden"] = verbindung.up;
  if (verbindung.hatBssid) doc["bssid"] = bssidText(verbindung.bssid);
  doc["kanal"] = verbindung.kanal;
  doc["rssi"] = verbindung.rssi;
  doc["rssi_mittel"] = mittel;
  doc["bewertung"] = rssiBewertung(mittel);
  doc["abbrueche"] = verbindung.abbrueche;
  doc["wechsel"] = verbindung.wechsel;

  static const char* PHASEN[] = {"ruhe", "sammeln", "scan", "router"};
  doc["phase"] = PHASEN[(int)messungPhase()];
  doc["fortschritt"] = messungFortschritt();
  if (hatLetzteMessung())
    messungAlsJson(letzteMessung(), cfg.name.c_str(), cfg.bereich.c_str(), cfg.ort.c_str(),
                   cfg.intervall, doc["messung"].to<JsonObject>());

  JsonObject p = doc["puffer"].to<JsonObject>();
  p["anzahl"] = puffer.anzahl();
  p["kapazitaet"] = puffer.kapazitaet();
  p["gesamt"] = puffer.gesamt();
  if (cfg.rolle == Rolle::Sonde) senderStatus(doc["sender"].to<JsonObject>());
  if (cfg.rolle == Rolle::Mobil) doc["punkte"] = punkteAnzahl();
  doc["matrix"] = anzeigeVorhanden();
  ereignisseAlsJson(doc["ereignisse"].to<JsonArray>());
  senden(doc);
}

static void handleSonden() {
  if (!istRolle(Rolle::Zentrale)) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < anzahlSonden; i++) sondeAlsJson(i, arr.add<JsonObject>());
  senden(doc);
}

// Alle Mesh-Knoten aus den letzten Messungen aller aktiven Sonden
static void handleKnoten() {
  if (!istRolle(Rolle::Zentrale)) return;
  struct Sicht {
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t kanal, sonde, gesehen, verbunden;
  };
  static Sicht liste[64];
  int n = 0;
  for (int i = 0; i < anzahlSonden; i++) {
    const SondeInfo& s = sonden[i];
    if (!s.hatMessung || sondeVeraltet(i)) continue;
    for (int k = 0; k < s.letzte.nKnoten; k++) {
      const Knoten& kn = s.letzte.knoten[k];
      int j = 0;
      while (j < n && memcmp(liste[j].bssid, kn.bssid, 6) != 0) j++;
      if (j == n) {
        if (n >= 64) continue;
        memcpy(liste[n].bssid, kn.bssid, 6);
        liste[n].rssi = kn.rssi;
        liste[n].kanal = kn.kanal;
        liste[n].sonde = i;
        liste[n].gesehen = liste[n].verbunden = 0;
        n++;
      } else if (kn.rssi > liste[j].rssi) {
        liste[j].rssi = kn.rssi;
        liste[j].sonde = i;
      }
      liste[j].gesehen++;
      if (s.letzte.verbunden && memcmp(s.letzte.bssid, kn.bssid, 6) == 0) liste[j].verbunden++;
    }
  }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int j = 0; j < n; j++) {
    const String b = bssidText(liste[j].bssid);
    JsonObject o = arr.add<JsonObject>();
    o["bssid"] = b;
    o["name"] = knotenName(b);
    o["kanal"] = liste[j].kanal;
    o["rssi"] = liste[j].rssi;
    o["sonde"] = sonden[liste[j].sonde].name;
    o["gesehen"] = liste[j].gesehen;
    o["verbunden"] = liste[j].verbunden;
  }
  senden(doc);
}

static void handlePunkte() {
  if (!istRolle(Rolle::Mobil)) return;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  File f = LittleFS.exists(punkteDatei()) ? LittleFS.open(punkteDatei(), "r") : File();
  bool erste = true;
  while (f && f.available()) {
    String zeile = f.readStringUntil('\n');
    zeile.trim();
    if (zeile.length() < 2) continue;
    if (!erste) server.sendContent(",");
    server.sendContent(zeile);
    erste = false;
  }
  if (f) f.close();
  server.sendContent("]");
  server.sendContent("");
}

// Alle Messungen als JSON Lines (eine Messung je Zeile) für scripts/tools
static void handleExport() {
  const String datei = cfg.name + (cfg.rolle == Rolle::Mobil ? "-punkte.jsonl" : "-messungen.jsonl");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + datei + "\"");
  if (cfg.rolle == Rolle::Mobil) {
    if (!LittleFS.exists(punkteDatei())) {
      server.send(200, "application/x-ndjson", "");
      return;
    }
    File f = LittleFS.open(punkteDatei(), "r");
    server.streamFile(f, "application/x-ndjson");
    f.close();
    return;
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/x-ndjson", "");
  String zeile;
  for (size_t i = 0; i < puffer.anzahl(); i++) {
    messungZeile(puffer.alt(i), zeile);
    server.sendContent(zeile);
  }
  server.sendContent("");
}

// -------------------------------- Aktionen ----------------------------------
static void handleSondeMeldung() {                    // POST /api/sonde (Zentrale)
  if (!istRolle(Rolle::Zentrale)) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  const char* name = doc["sonde"] | "";
  if (!*name) {
    server.send(400, "text/plain", "Feld 'sonde' fehlt");
    return;
  }
  const int idx = sondeFinden(name, true);
  if (idx < 0) {
    server.send(507, "text/plain", "Sondentabelle voll");
    return;
  }
  if (idx == 0) {
    server.send(409, "text/plain", "Name ist bereits die Zentrale");
    return;
  }
  Messung m;
  messungAusJson(doc.as<JsonObjectConst>(), m);
  m.sonde = idx;
  if (!m.t) m.t = jetztUnix();

  SondeInfo& s = sonden[idx];
  strlcpy(s.bereich, doc["bereich"] | "", sizeof(s.bereich));
  strlcpy(s.ort, doc["ort"] | "", sizeof(s.ort));
  s.intervall = doc["intervall"] | 30;
  s.zuletzt = millis();
  s.ip = server.client().remoteIP();
  s.letzte = m;
  s.hatMessung = true;
  puffer.add(m);
  server.send(204);
}

static void handlePunkt() {                           // POST /api/punkt (Mobil)
  if (!istRolle(Rolle::Mobil)) return;
  JsonDocument doc;
  deserializeJson(doc, server.arg("plain"));
  String id = doc["punkt"] | "";
  id.trim();
  if (id.isEmpty()) id = "P" + String(punkteAnzahl() + 1);
  const String bereich = doc["bereich"] | cfg.bereich.c_str();
  if (!messungStarten(id.c_str(), bereich.c_str())) {
    server.send(409, "text/plain", "Es läuft bereits eine Messung");
    return;
  }
  server.send(202, "text/plain", id);
}

static void handlePunktLoeschen() {
  if (!istRolle(Rolle::Mobil)) return;
  const bool nurLetzten = server.arg("plain") != "alle";
  punkteLoeschen(nurLetzten);
  ereignis(nurLetzten ? "Letzter Messpunkt gelöscht" : "Alle Messpunkte gelöscht");
  server.send(204);
}

static void handleMessen() {
  if (cfg.rolle == Rolle::Mobil) {
    server.send(409, "text/plain", "In der Rolle mobil bitte Messpunkt erfassen");
    return;
  }
  server.send(messungStarten() ? 202 : 409);
}

static void handleKnotenName() {
  if (!istRolle(Rolle::Zentrale)) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  uint8_t b[6];
  if (!bssidAusText(doc["bssid"] | "", b)) {
    server.send(400, "text/plain", "ungültige BSSID");
    return;
  }
  String name = doc["name"] | "";
  name.trim();
  knotenNameSetzen(bssidText(b), name.substring(0, 32));
  server.send(204);
}

static void handleConfig() {
  if (server.hasArg("name")) {
    const String n = hostnameAus(server.arg("name"));
    if (n.length()) cfg.name = n;
  }
  if (server.hasArg("rolle")) cfg.rolle = rolleAus(server.arg("rolle"));
  if (server.hasArg("bereich")) cfg.bereich = server.arg("bereich").substring(0, 15);
  if (server.hasArg("ort")) cfg.ort = server.arg("ort").substring(0, 39);
  if (server.hasArg("zentrale")) cfg.zentrale = server.arg("zentrale");
  if (server.hasArg("intervall")) cfg.intervall = constrain(server.arg("intervall").toInt(), 10, 3600);
  cfg.bereich.trim();
  cfg.ort.trim();
  cfg.zentrale.trim();
  einstellungenSpeichern();
  server.send(200, "text/plain", "gespeichert, Neustart");
  delay(500);
  ESP.restart();
}

void webStart() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/sonden", HTTP_GET, handleSonden);
  server.on("/api/knoten", HTTP_GET, handleKnoten);
  server.on("/api/knoten", HTTP_POST, handleKnotenName);
  server.on("/api/punkte", HTTP_GET, handlePunkte);
  server.on("/api/export", HTTP_GET, handleExport);
  server.on("/api/sonde", HTTP_POST, handleSondeMeldung);
  server.on("/api/punkt", HTTP_POST, handlePunkt);
  server.on("/api/punkt/loeschen", HTTP_POST, handlePunktLoeschen);
  server.on("/api/messen", HTTP_POST, handleMessen);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/reset-wifi", HTTP_POST, []() {
    server.send(204);
    delay(200);
    wlanZuruecksetzen();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
