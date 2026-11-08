/* Unofficial community ProAir PA700LI GM3 MQTT controls.
 * OFFLINE-BUILT ONLY: no hardware execution validation.
 * Configuration: config.h. Controls DISABLED in distributed copy.
 */
#include <Arduino.h>
#include "pin_guard.h"
#if !defined(ARDUINO_NANO_ESP32)
#error "Select the official Arduino Nano ESP32 board and By GPIO number (legacy)."
#endif
#include <WiFi.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>
#include <esp_system.h>
#include <initializer_list>
#include "config.h"
#include "control_core.h"
#include "discovery.h"

using namespace proair;
static const ProtocolCredentials protocolCredentials{GM3_LOGIN,GM3_PIN};
HardwareSerial RS485(1);
// All MQTT/WiFi operations are isolated to core 0. UART parser/engine live in loop() on core 1.
class BoundedWiFiClient:public WiFiClient {
 public:
 int connect(IPAddress ip,uint16_t port) override{return WiFiClient::connect(ip,port,1000);}
 int connect(const char* host,uint16_t port) override{return WiFiClient::connect(host,port,1000);}
};
BoundedWiFiClient networkClient;
PubSubClient mqtt(networkClient);
static State liveState;
static gm3_parser_t parser;
static QueueHandle_t commands=nullptr,snapshots=nullptr,events=nullptr;
static bool uartReady=false,resourcesReady=false;
static uint32_t rawBytes=0,parserRejects=0,crcRejects=0,partialRejects=0,lastRxMs=0,lastSnapshot=0,statusDrops=0;
struct Snapshot {State state;uint32_t at,raw,parserRejects,crcRejects,partials,statusDrops;bool busy,uartReady;};
struct OutEvent {char json[256];};
struct Link {bool up=false;uint32_t session=0,connectedAt=0;};
static Link sharedLink;
static portMUX_TYPE linkMux=portMUX_INITIALIZER_UNLOCKED;
static uint32_t networkSession=0;
static char sessionNonce[17]="";
static bool discoveryNeeded=false;
static Snapshot networkSnapshot;
static void onEvent(void*,const Event&);
static Engine engine(liveState,onEvent,nullptr,protocolCredentials);

static Link readLink(){Link l;portENTER_CRITICAL(&linkMux);l=sharedLink;portEXIT_CRITICAL(&linkMux);return l;}
static void updateLink(bool up,uint32_t now){portENTER_CRITICAL(&linkMux);sharedLink.up=up;sharedLink.session=networkSession;sharedLink.connectedAt=now;portEXIT_CRITICAL(&linkMux);}
static void onEvent(void*,const Event& e){
 OutEvent out;
 snprintf(out.json,sizeof(out.json),"{\"state\":\"%s\",\"detail\":\"%s\",\"command\":\"%s\",\"target\":%lu,\"id\":%lu,\"session\":%lu}",e.state,e.detail,kind_name(e.request.kind),(unsigned long)e.request.value,(unsigned long)e.id,(unsigned long)e.request.session);
 if(!events||xQueueSend(events,&out,0)!=pdTRUE)++statusDrops;
 // Never log payloads, SSIDs, passwords, auth strings, or wire frames.
 Serial.printf("Control %s: %s\n",e.state,e.detail);
}
static void parserEvent(void*,const gm3_result_t* r,uint32_t now){
 if(r->status==GM3_OK){
  const uint8_t* f=r->data;
  if(r->len==12&&f[3]==0x64&&f[4]==0&&f[5]==1&&f[6]==0&&f[7]==0xA9&&f[8]==0xE5)engine.generic_ack();
  liveState.decode(f,r->len,now);
 }else{
  ++parserRejects;if(r->status==GM3_REJECT_BAD_CRC)++crcRejects;
  if(r->status==GM3_REJECT_PARTIAL_TIMEOUT)++partialRejects;
 }
}
static bool publish(const char* topic,const char* payload,bool retain=false){return mqtt.publish(topic,payload,retain);}
static void rejectNetwork(const char* reason){
 char b[160];snprintf(b,sizeof(b),"{\"state\":\"rejected\",\"detail\":\"%s\",\"session\":%lu}",reason,(unsigned long)networkSession);publish("pa700li/control/status",b,true);
}
static void mqttCallback(char* topic,byte* payload,unsigned int length){
 if(!strcmp(topic,"homeassistant/status")){
  if(length==6&&!memcmp(payload,"online",6))discoveryNeeded=true;return;
 }
 if(!ENABLE_CONTROL){rejectNetwork("control_disabled");return;}
 Request r;
 if(!parse_command(topic,payload,length,sessionNonce,r)){rejectNetwork("invalid_payload_or_session");return;}
 const uint32_t now=millis();
 if(uint32_t(now-readLink().connectedAt)<10000u){rejectNetwork("startup_guard");return;}
 r.received_at=now;r.session=networkSession;
 if(xQueueSend(commands,&r,0)!=pdTRUE)rejectNetwork("command_inbox_full");
}
static bool sendDiscovery(){
 bool ok=true;char topic[96],json[1200];
 // Intentional documented migration: retire the unsupported legacy Boost switch only.
 ok=publish("homeassistant/switch/pa700li_boost/config","",true)&&ok;
 for(const auto& s:sensors){
  snprintf(topic,sizeof(topic),"homeassistant/sensor/pa700li_%s/config",s.id);
  ok=sensor_discovery(s,json,sizeof(json))&&publish(topic,json,true)&&ok;
 }
 for(Kind k:{Kind::Auto,Kind::Mode,Kind::Bypass}){
  snprintf(topic,sizeof(topic),"homeassistant/%s/pa700li_%s/config",k==Kind::Auto?"switch":"select",kind_name(k));
  ok=control_discovery(k,sessionNonce,json,sizeof(json))&&publish(topic,json,true)&&ok;
 }
 return ok;
}
static void clearLegacyStateRetention(){
 char topic[96];
 for(const auto& s:sensors){
  snprintf(topic,sizeof(topic),"pa700li/sensor/%s",s.field==BOOST?"boost":s.id);
  publish(topic,"",true);publish(topic,"None");
 }
 for(const char* t:{"pa700li/sensor/auto","pa700li/state/mode","pa700li/state/bypass"}){publish(t,"",true);publish(t,"None");}
}
static void publishSnapshot(const Snapshot& snap,uint32_t now){
 const State& s=snap.state;char topic[96],value[40];
 const bool snapshotFresh=uint32_t(now-snap.at)<=2000u;
 for(const auto& def:sensors){
  if(!snapshotFresh||!sensor_payload(def,s,now,value,sizeof(value)))continue;
  snprintf(topic,sizeof(topic),"pa700li/sensor/%s",def.field==BOOST?"boost":def.id);
  publish(topic,value); // NOT retained: expire_after must not resurrect stale broker state.
 }
 const bool modeValid=snapshotFresh&&s.f[MODE_A].fresh(now)&&s.f[MODE_B].fresh(now)&&s.f[MODE_A].value==s.f[MODE_B].value;
 const bool autoValid=snapshotFresh&&s.f[AUTO_A].fresh(now)&&s.f[AUTO_B].fresh(now)&&s.f[AUTO_A].value==s.f[AUTO_B].value;
 const bool bypassValid=snapshotFresh&&s.f[BYPASS].fresh(now);
 publish("pa700li/sensor/auto",autoValid?(s.f[AUTO_A].value==1?"1":"0"):"None");
 publish("pa700li/state/mode",modeValid?mode_name(uint32_t(s.f[MODE_A].value)):"None");
 const char* bp=bypassValid?bypass_name(uint32_t(s.f[BYPASS].value)):"unknown";
 publish("pa700li/state/bypass",strcmp(bp,"unknown")?bp:"None");
 const bool available=ENABLE_CONTROL&&protocol_credentials_valid(protocolCredentials)&&snap.uartReady&&!snap.busy&&snapshotFresh&&s.gate(now)==nullptr&&uint32_t(now-readLink().connectedAt)>=10000u;
 publish("pa700li/availability/control",available?"online":"offline",true);
 char diag[384];snprintf(diag,sizeof(diag),"{\"raw_rx_bytes\":%lu,\"valid_frames\":%lu,\"parser_reject_events\":%lu,\"crc_rejects\":%lu,\"partial_rejects\":%lu,\"layout_errors\":%lu,\"status_queue_drops\":%lu,\"uart_ready\":%s,\"controls_enabled\":%s,\"rx_gpio\":18,\"tx_gpio\":17,\"de_gpio\":4}",(unsigned long)snap.raw,(unsigned long)s.valid_frames,(unsigned long)snap.parserRejects,(unsigned long)snap.crcRejects,(unsigned long)snap.partials,(unsigned long)s.layout_errors,(unsigned long)snap.statusDrops,snap.uartReady?"true":"false",ENABLE_CONTROL?"true":"false");
 publish("pa700li/diagnostics",diag);
}
static void networkTask(void*){
 mqtt.setServer(MQTT_SERVER,MQTT_PORT);mqtt.setCallback(mqttCallback);
 if(!mqtt.setBufferSize(2048)){Serial.println("MQTT buffer allocation failed; controls inhibited");vTaskDelete(nullptr);return;}
 mqtt.setSocketTimeout(1);mqtt.setKeepAlive(15);networkClient.setTimeout(1000);
 WiFi.mode(WIFI_STA);
 uint32_t lastWifi=0,lastConnect=0,lastPublish=0,lastDiscovery=0;bool wifiStarted=false;
 for(;;){
  uint32_t now=millis();
  if(*WIFI_SSID&&WiFi.status()!=WL_CONNECTED&&(!wifiStarted||uint32_t(now-lastWifi)>=15000u)){
   WiFi.begin(WIFI_SSID,WIFI_PASSWORD);wifiStarted=true;lastWifi=now;
  }
  if(xQueueReceive(snapshots,&networkSnapshot,0)==pdTRUE){} // newest snapshot only, no shared UART/state locks
  if(WiFi.status()!=WL_CONNECTED){if(readLink().up)updateLink(false,now);if(mqtt.connected())mqtt.disconnect();}
  else if(*MQTT_SERVER&&*MQTT_CLIENT&&!mqtt.connected()&&uint32_t(now-lastConnect)>=5000u){
   updateLink(false,now);lastConnect=now;
   if(mqtt.connect(MQTT_CLIENT,*MQTT_USER?MQTT_USER:nullptr,*MQTT_PASS?MQTT_PASS:nullptr,"pa700li/availability",0,true,"offline")){
    ++networkSession;snprintf(sessionNonce,sizeof(sessionNonce),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
    updateLink(true,millis());
    publish("pa700li/availability/control","offline",true);clearLegacyStateRetention();
    publish("pa700li/control/session",sessionNonce,true);
    publish("pa700li/availability","online",true);
    // Only subscribe once per fresh nonce; old retained commands fail nonce validation.
    if(!mqtt.subscribe("pa700li/set/#")||!mqtt.subscribe("homeassistant/status")){mqtt.disconnect();updateLink(false,millis());}
    discoveryNeeded=true;
   }
  }
  if(mqtt.connected()){
   mqtt.loop();
   now=millis();
   if(discoveryNeeded&&uint32_t(now-lastDiscovery)>=1000u){lastDiscovery=now;discoveryNeeded=!sendDiscovery();}
   OutEvent event;
   // Bounded per iteration; preserve an event for retry if publish fails during a broken connection.
   if(xQueuePeek(events,&event,0)==pdTRUE&&publish("pa700li/control/status",event.json,true))xQueueReceive(events,&event,0);
   if(uint32_t(now-lastPublish)>=1000u){lastPublish=now;publishSnapshot(networkSnapshot,now);}
  }
  if(!mqtt.connected()&&readLink().up)updateLink(false,millis());
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
void setup(){
 Serial.begin(115200);
 digitalWrite(DE_PIN,LOW);pinMode(DE_PIN,OUTPUT); // preserve supplied DE4, inactive before UART initialization
 const size_t ring=RS485.setRxBufferSize(4096); // MUST precede begin on the installed official core
 RS485.begin(BAUDRATE,SERIAL_8N1,RX_PIN,TX_PIN);
 uartReady=ring==4096&&bool(RS485);
 gm3_parser_init(&parser);
 commands=xQueueCreate(4,sizeof(Request));snapshots=xQueueCreate(1,sizeof(Snapshot));events=xQueueCreate(8,sizeof(OutEvent));
 resourcesReady=commands&&snapshots&&events;
 if(resourcesReady)resourcesReady=xTaskCreatePinnedToCore(networkTask,"proair_mqtt",8192,nullptr,1,nullptr,0)==pdPASS;
 Serial.printf("ProAir MQTT controls: GPIO RX18 TX17 DE4; controls=%s, UART=%s, network_task=%s\n",ENABLE_CONTROL?"ENABLED":"DISABLED",uartReady?"ready":"failed",resourcesReady?"started":"failed");
}
void loop(){
 uint32_t now=millis();
 // Single UART reader. Bounded batches prevent command/snapshot starvation under noise.
 size_t served=0;
 while(RS485.available()&&served++<4096){int v=RS485.read();if(v<0)break;++rawBytes;lastRxMs=millis();gm3_parser_byte(&parser,uint8_t(v),lastRxMs,100,parserEvent,nullptr);}
 now=millis();gm3_parser_tick(&parser,now,100,parserEvent,nullptr);
 if(resourcesReady){
  const Link l=readLink();
  if(engine.awaiting_bus()&&(!l.up||engine.session()!=l.session))engine.cancel_queued("mqtt_session_changed_before_tx");
  Request r;
  if(xQueueReceive(commands,&r,0)==pdTRUE){
   if(!l.up||r.session!=l.session||uint32_t(now-r.received_at)>2000u){Event e{"rejected","stale_command_session",r,0};onEvent(nullptr,e);}
   else engine.submit(r,now,ENABLE_CONTROL&&uartReady);
  }
 }
 engine.tick(now);
#if ENABLE_CONTROL
 // Listen-before-talk only: not proven multi-master arbitration. One attempt, never a retry.
 static uint32_t idleExtra=0;
 if(!engine.awaiting_bus())idleExtra=0;
 if(engine.awaiting_bus()&&!idleExtra)idleExtra=20u+(esp_random()%11u);
 if(engine.awaiting_bus()&&rawBytes&&uint32_t(now-lastRxMs)>=idleExtra&&!parser.len&&!RS485.available()){
  uint8_t frame[MAX_WRITE_FRAME];const size_t n=engine.prepare(now,frame,sizeof(frame));
  if(n&&!RS485.available()){
   digitalWrite(DE_PIN,HIGH);delayMicroseconds(100);
   const size_t written=RS485.write(frame,n);RS485.flush();
   delayMicroseconds(100);digitalWrite(DE_PIN,LOW);
   engine.mark_sent(millis(),written==n);
  }
 }
#endif
 if(resourcesReady&&uint32_t(now-lastSnapshot)>=250u){
  lastSnapshot=now;Snapshot snap;snap.state=liveState;snap.at=now;snap.raw=rawBytes;snap.parserRejects=parserRejects;snap.crcRejects=crcRejects;snap.partials=partialRejects;snap.statusDrops=statusDrops;snap.busy=engine.active();snap.uartReady=uartReady;
  xQueueOverwrite(snapshots,&snap);
 }
 delay(1);
}
