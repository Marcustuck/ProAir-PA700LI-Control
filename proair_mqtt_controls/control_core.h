#pragma once
#include <stdint.h>
#include <stddef.h>
#include "gm3_parser.h"
namespace proair {
constexpr uint32_t FRESH_MS=15000, SENSOR_FRESH_MS=30000, READBACK_MS=20000, BUS_WAIT_MS=3000;
enum Field { MODE_A, MODE_B, AUTO_A, AUTO_B, BYPASS, BOOST, BOOST_MIN, PARTY,
 FAN_SUPPLY, FAN_EXTRACT, VOLT_SUPPLY, VOLT_EXTRACT, TEMP_SUPPLY, TEMP_EXTRACT,
 TEMP_FRESH, TEMP_EXHAUST, HUMIDITY, CO2, FILTER_DEPLETION, BYPASS_ACTIVE, BYPASS_POSITION, FIELD_COUNT };
struct Sample {
 bool valid=false; double value=0; uint32_t at=0, generation=0;
 bool fresh(uint32_t now,uint32_t ttl=FRESH_MS) const {return valid && uint32_t(now-at)<=ttl;}
 void put(double v,uint32_t now){valid=true;value=v;at=now;++generation;}
};
struct State {
 Sample f[FIELD_COUNT]; uint32_t valid_frames=0, layout_errors=0;
 bool fresh(Field id,uint32_t now) const {return f[id].fresh(now);}
 bool coherent(uint32_t now) const;
 const char* gate(uint32_t now) const;
 bool decode(const uint8_t*,size_t,uint32_t);
};
enum class Kind : uint8_t { Auto, Mode, Bypass };
struct Request {Kind kind=Kind::Mode;uint32_t value=0;uint32_t received_at=0;uint32_t session=0;};
const char* mode_name(uint32_t);
const char* bypass_name(uint32_t);
const char* kind_name(Kind);
bool valid_request(const Request&);
// Strict, bounded payloads: 16-hex-digit MQTT-connection nonce ':' value. No String/toInt.
bool parse_command(const char* topic,const uint8_t* payload,size_t length,const char* nonce,Request& out);
// Pure wire encoder; NOT an arbitrary MQTT or UART interface.
struct ProtocolCredentials {const char* login;const char* pin;};
constexpr size_t MAX_AUTH_TEXT=24, MAX_WRITE_FRAME=16+2*MAX_AUTH_TEXT+4;
bool protocol_credentials_valid(const ProtocolCredentials&);
size_t pack_write(const ProtocolCredentials&,uint16_t param,const uint8_t* values,size_t width,uint8_t* out,size_t cap);
size_t build_frame(const Request&,const ProtocolCredentials&,uint8_t*,size_t);
struct Event {const char* state;const char* detail;Request request;uint32_t id;};
typedef void (*EventFn)(void*,const Event&);
class Engine {
 State& s;ProtocolCredentials credentials;EventFn emit;void* ctx;Request current;uint32_t next_id=0,current_id=0,start=0,sent_at=0;
 uint32_t baseline[FIELD_COUNT]={};bool active_=false,sent_=false,ack_=false;
 void event(const char*,const char*,const Request&,uint32_t);
 void finish(const char*,const char*);
 bool post(Field,uint32_t) const;
 bool match(uint32_t) const;
 bool any_post(uint32_t) const;
 public:
 Engine(State& state,EventFn fn,void* p,const ProtocolCredentials& c):s(state),credentials(c),emit(fn),ctx(p){}
 bool active() const{return active_;} bool awaiting_bus() const{return active_&&!sent_;}
 void submit(const Request&,uint32_t,bool);
 void tick(uint32_t);
 size_t prepare(uint32_t,uint8_t*,size_t);
 void mark_sent(uint32_t,bool complete);
 uint32_t session() const{return current.session;}
 void cancel_queued(const char* why){if(awaiting_bus())finish("rejected",why);}
 void generic_ack(){if(active_&&sent_)ack_=true;}
};
}
