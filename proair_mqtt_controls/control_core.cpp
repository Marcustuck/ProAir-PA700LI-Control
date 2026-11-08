#include "control_core.h"
#include <string.h>
#include <math.h>
#include <initializer_list>
namespace proair {
static uint16_t le16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
static uint32_t le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
const char* mode_name(uint32_t v){switch(v){case 3:return "Compliance Level 1";case 4:return "Compliance Level 2";case 5:return "Compliance Level 3";case 6:return "Pause";default:return "unknown";}}
const char* bypass_name(uint32_t v){switch(v){case 0x00100121:return "Auto";case 0x00100109:return "Closed";case 0x00100111:return "Open";default:return "unknown";}}
const char* kind_name(Kind k){switch(k){case Kind::Mode:return "mode";case Kind::Auto:return "auto";case Kind::Bypass:return "bypass";}return "unknown";}
bool valid_request(const Request& r){switch(r.kind){case Kind::Mode:return r.value>=3&&r.value<=6;case Kind::Auto:return r.value<=1;case Kind::Bypass:return strcmp(bypass_name(r.value),"unknown")!=0;}return false;}
bool parse_command(const char* topic,const uint8_t* p,size_t n,const char* nonce,Request& out){
 if(!topic||!p||!nonce||strlen(nonce)!=16||n<18||n>64||memcmp(p,nonce,16)||p[16]!=':')return false;
 char msg[48];size_t m=n-17;if(m>=sizeof(msg))return false;
 for(size_t i=0;i<m;++i){if(p[17+i]<32||p[17+i]>126)return false;msg[i]=char(p[17+i]);}msg[m]=0;
 Request r;
 if(!strcmp(topic,"pa700li/set/auto")){
  r.kind=Kind::Auto;if(!strcmp(msg,"ON")||!strcmp(msg,"on")||!strcmp(msg,"1"))r.value=1;
  else if(!strcmp(msg,"OFF")||!strcmp(msg,"off")||!strcmp(msg,"0"))r.value=0;else return false;
 }else if(!strcmp(topic,"pa700li/set/mode")){
  r.kind=Kind::Mode;for(uint32_t i=3;i<=6;++i)if(!strcmp(msg,mode_name(i)))r.value=i;if(!r.value)return false;
 }else if(!strcmp(topic,"pa700li/set/bypass")){
  r.kind=Kind::Bypass;for(uint32_t v:{0x00100121u,0x00100109u,0x00100111u})if(!strcmp(msg,bypass_name(v)))r.value=v;if(!r.value)return false;
 }else return false;
 out=r;return true;
}
// Credential strings are supplied by user-local configuration, never built-in.
static bool auth_length(const char* text,size_t& n){
 if(!text)return false;
 n=0;
 while(n<MAX_AUTH_TEXT&&text[n]){
  const unsigned char c=static_cast<unsigned char>(text[n]);
  if(c<32||c>126)return false;
  ++n;
 }
 return n>0&&text[n]==0;
}
bool protocol_credentials_valid(const ProtocolCredentials& c){
 size_t login=0,pin=0;return auth_length(c.login,login)&&auth_length(c.pin,pin);
}
size_t pack_write(const ProtocolCredentials& credentials,uint16_t param,const uint8_t* values,size_t width,uint8_t* out,size_t cap){
 if(!out||!values||(width!=1&&width!=4))return 0;
 size_t login=0,pin=0;
 if(!auth_length(credentials.login,login)||!auth_length(credentials.pin,pin))return 0;
 const size_t total=16+login+pin+width;if(cap<total)return 0;
 uint8_t b[MAX_WRITE_FRAME]={0x68,0,0,1,0,0x64,0,0x29};size_t index=8;
 memcpy(b+index,credentials.login,login);index+=login;b[index++]=0;
 memcpy(b+index,credentials.pin,pin);index+=pin;b[index++]=0;
 b[index++]=1;b[index++]=param&255;b[index++]=param>>8;
 memcpy(b+index,values,width);index+=width;
 b[1]=uint8_t(total-6);b[2]=0;
 uint16_t c=gm3_crc16_xmodem(b+1,total-4);b[index++]=c>>8;b[index++]=c&255;b[index++]=0x16;
 if(index!=total)return 0;
 memcpy(out,b,total);return total;
}
size_t build_frame(const Request& r,const ProtocolCredentials& credentials,uint8_t* out,size_t cap){
 if(!valid_request(r))return 0;
 const uint16_t param=r.kind==Kind::Auto?241:r.kind==Kind::Mode?1123:816;
 uint8_t values[4];for(size_t i=0;i<4;++i)values[i]=uint8_t(r.value>>(8*i));
 return pack_write(credentials,param,values,r.kind==Kind::Auto?1:4,out,cap);
}
// Layout/status fingerprints derived from controlled observation of one installation. Status semantics are UNKNOWN:
// 0xC0 on fan-voltage fields is not an error by assumption; each field requires its observed byte.
struct Def{Field id;uint16_t block,offset;uint8_t width,status;bool floating;double low,high;};
static const Def defs[]={
 {MODE_A,1100,46,4,1,false,3,6},{MODE_B,200,109,1,1,false,3,6},
 {AUTO_A,229,10,1,0,false,0,1},{AUTO_B,1100,97,1,0,false,0,1},
 {BYPASS,800,35,4,1,false,0,4294967295.0},{BOOST,229,16,1,0,false,0,2},
 {BOOST_MIN,550,117,2,0,false,0,65535},{PARTY,1100,61,4,1,false,0,2},
 {FAN_SUPPLY,150,147,1,0,false,0,100},{FAN_EXTRACT,150,149,1,0,false,0,100},
 {VOLT_SUPPLY,150,27,4,0xC0,true,0,15},{VOLT_EXTRACT,150,32,4,0xC0,true,0,15},
 {TEMP_SUPPLY,250,30,4,0,true,-60,100},{TEMP_EXTRACT,250,40,4,0,true,-60,100},
 {TEMP_FRESH,250,55,4,0,true,-60,100},{TEMP_EXHAUST,250,60,4,0,true,-60,100},
 {HUMIDITY,550,73,4,0,true,0,100},{CO2,550,78,4,0,true,0,20000},
 {FILTER_DEPLETION,900,29,4,0,true,0,100},{BYPASS_ACTIVE,800,45,1,0,false,0,1},
 {BYPASS_POSITION,800,87,4,0,true,0,100}
};
static size_t block_size(uint16_t b){switch(b){case 150:return 155;case 200:return 138;case 229:return 20;case 250:return 148;case 550:return 144;case 800:return 153;case 900:return 153;case 1100:return 146;default:return 0;}}
bool State::decode(const uint8_t* b,size_t n,uint32_t now){
 if(!b||n>GM3_MAX_FRAME||gm3_validate_frame(b,uint16_t(n))!=GM3_OK)return false;
 ++valid_frames;if(n<14||le16(b+5)!=1||b[7]!=0xC0)return false;
 const uint16_t block=le16(b+9);const size_t expected=block_size(block);if(!expected)return false;
 const size_t len=n-14;const uint8_t* p=b+11;
 if(len!=expected||b[8]!=(block==229?10:50)){
  ++layout_errors;for(const auto& d:defs)if(d.block==block)f[d.id].valid=false;return false;
 }
 for(const auto& d:defs){
  if(d.block!=block)continue;
  Sample& sample=f[d.id];sample.valid=false;
  if(d.offset>=len||d.width>len-d.offset-1||p[d.offset]!=d.status)continue;
  const uint8_t* v=p+d.offset+1;double x;
  if(d.floating){uint32_t bits=le32(v);float fv;static_assert(sizeof(float)==4,"32-bit float required");memcpy(&fv,&bits,4);x=fv;}
  else x=d.width==1?v[0]:d.width==2?le16(v):le32(v);
  if(!isfinite(x)||x<d.low||x>d.high)continue;
  // Unknown packed layouts are retained as raw values but gate all control below.
  sample.put(x,now);
 }
 return true;
}
bool State::coherent(uint32_t now) const {
 return fresh(MODE_A,now)&&fresh(MODE_B,now)&&f[MODE_A].value==f[MODE_B].value&&
 fresh(AUTO_A,now)&&fresh(AUTO_B,now)&&f[AUTO_A].value==f[AUTO_B].value&&fresh(BYPASS,now);
}
const char* State::gate(uint32_t now) const {
 if(!coherent(now))return "stale_uninitialized_or_mismatched_copies";
 if(!strcmp(bypass_name(uint32_t(f[BYPASS].value)),"unknown"))return "unknown_bypass_layout";
 if(!fresh(BOOST,now)||!fresh(PARTY,now))return "stale_override_state";
 if(f[BOOST].value==2||f[PARTY].value!=0)return "boost_or_party_active";
 return nullptr;
}
void Engine::event(const char* state,const char* detail,const Request& r,uint32_t id){if(emit){Event e{state,detail,r,id};emit(ctx,e);}}
void Engine::finish(const char* state,const char* detail){event(state,detail,current,current_id);active_=false;sent_=false;}
void Engine::submit(const Request& r,uint32_t now,bool enabled){
 const uint32_t id=++next_id;
 if(active_){event("rejected","busy",r,id);return;}
 if(!enabled){event("rejected","control_disabled",r,id);return;}
 if(!protocol_credentials_valid(credentials)){event("rejected","protocol_credentials_unconfigured",r,id);return;}
 if(!valid_request(r)){event("rejected","invalid_payload",r,id);return;}
 const char* why=s.gate(now);if(why){event("rejected",why,r,id);return;}
 if(r.kind==Kind::Auto&&s.f[MODE_A].value==6){event("rejected","auto_requires_compliance_mode",r,id);return;}
 Field f=r.kind==Kind::Mode?MODE_A:r.kind==Kind::Auto?AUTO_A:BYPASS;
 if(s.f[f].value==r.value){event("confirmed","already_at_target_no_tx",r,id);return;}
 current=r;current_id=id;start=now;active_=true;sent_=false;ack_=false;
 event("pending","waiting_bus_idle",current,current_id);
}
size_t Engine::prepare(uint32_t now,uint8_t* b,size_t cap){
 if(!awaiting_bus())return 0;
 if(uint32_t(now-start)>=BUS_WAIT_MS){finish("timeout","not_sent_bus_busy");return 0;}
 const char* why=s.gate(now);if(why){finish("rejected",why);return 0;}
 if(current.kind==Kind::Auto&&s.f[MODE_A].value==6){finish("rejected","auto_requires_compliance_mode");return 0;}
 Field f=current.kind==Kind::Mode?MODE_A:current.kind==Kind::Auto?AUTO_A:BYPASS;
 if(s.f[f].value==current.value){finish("confirmed","target_reached_before_tx_no_tx");return 0;}
 size_t n=build_frame(current,credentials,b,cap);if(!n)finish("rejected","frame_construction_failed");return n;
}
void Engine::mark_sent(uint32_t now,bool complete){
 if(!awaiting_bus())return;
 sent_=true;sent_at=now;
 for(size_t i=0;i<FIELD_COUNT;++i)baseline[i]=s.f[i].generation;
 if(!complete){finish("uncertain","partial_uart_write_no_retry");return;}
 event("pending","sent_waiting_independent_readback",current,current_id);
}
bool Engine::post(Field f,uint32_t now) const{return s.f[f].generation!=baseline[f]&&s.f[f].fresh(now)&&uint32_t(s.f[f].at-sent_at)<0x80000000u;}
bool Engine::match(uint32_t now) const {
 if(current.kind==Kind::Bypass)return post(BYPASS,now)&&s.f[BYPASS].value==current.value;
 Field a=current.kind==Kind::Mode?MODE_A:AUTO_A,b=current.kind==Kind::Mode?MODE_B:AUTO_B;
 if(current.kind==Kind::Auto&&(!s.f[MODE_A].fresh(now)||!s.f[MODE_B].fresh(now)||s.f[MODE_A].value!=s.f[MODE_B].value||s.f[MODE_A].value==6))return false;
 return post(a,now)&&post(b,now)&&s.f[a].value==current.value&&s.f[b].value==current.value;
}
bool Engine::any_post(uint32_t) const {
 auto seen=[this](Field f){return s.f[f].generation!=baseline[f]&&uint32_t(s.f[f].at-sent_at)<0x80000000u;};
 if(current.kind==Kind::Bypass)return seen(BYPASS);
 return current.kind==Kind::Mode?(seen(MODE_A)||seen(MODE_B)):(seen(AUTO_A)||seen(AUTO_B));
}
void Engine::tick(uint32_t now){
 if(!active_)return;
 if(!sent_){if(uint32_t(now-start)>=BUS_WAIT_MS)finish("timeout","not_sent_bus_busy");return;}
 if(match(now)){finish("confirmed","fresh_post_tx_readback");return;}
 if(uint32_t(now-sent_at)>=READBACK_MS){
  if(any_post(now)||ack_)finish("uncertain","unconfirmed_readback_no_retry");
  else finish("timeout","no_post_tx_readback_no_retry");
 }
}
}
