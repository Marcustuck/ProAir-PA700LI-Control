#include "../../control_core.h"
#include "../../discovery.h"
#include "fixture_data.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
using namespace proair;
// Deliberately synthetic credentials; these are NOT device credentials.
static const ProtocolCredentials syntheticCredentials{"TESTUSER","DEMO"};
static size_t pack_write_for_test(uint16_t p,const uint8_t* v,size_t w,uint8_t* b,size_t n){return pack_write(syntheticCredentials,p,v,w,b,n);}
static size_t build_frame_for_test(const Request& r,uint8_t* b,size_t n){return build_frame(r,syntheticCredentials,b,n);}
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";abort();}}while(0)
struct Recorded{std::string state,detail;uint32_t id;};
static void record(void* c,const Event& e){static_cast<std::vector<Recorded>*>(c)->push_back({e.state,e.detail,e.id});}
static Request req(Kind k,uint32_t v){Request r;r.kind=k;r.value=v;return r;}
static State fresh(uint32_t now){State s;for(Field f:{MODE_A,MODE_B})s.f[f].put(4,now);for(Field f:{AUTO_A,AUTO_B,BOOST,PARTY})s.f[f].put(0,now);s.f[BYPASS].put(0x00100121,now);return s;}
static std::vector<uint8_t> seal(std::vector<uint8_t> b){b[1]=uint8_t(b.size()-6);b[2]=uint8_t((b.size()-6)>>8);uint16_t crc=gm3_crc16_xmodem(b.data()+1,b.size()-4);b[b.size()-3]=crc>>8;b[b.size()-2]=crc&255;b.back()=0x16;return b;}
static void wire_tests(){
 CHECK(gm3_crc16_xmodem(reinterpret_cast<const uint8_t*>("123456789"),9)==0x31C3);
 uint8_t b[40];
 std::set<uint16_t> params;
 for(const auto& f:controlFixtures){
  params.insert(f.param);memset(b,0xAA,sizeof(b));size_t n=pack_write_for_test(f.param,f.value.data(),f.value.size(),b,sizeof(b));
  CHECK(n==f.bytes.size());CHECK(!memcmp(b,f.bytes.data(),n));CHECK(gm3_validate_frame(b,n)==GM3_OK);CHECK(b[n]==0xAA);
  memset(b,0xAA,sizeof(b));CHECK(pack_write_for_test(f.param,f.value.data(),f.value.size(),b,n-1)==0);CHECK(b[0]==0xAA);
  if(f.param==241||f.param==1123||f.param==816){uint32_t v=0;for(size_t i=0;i<f.value.size();++i)v|=uint32_t(f.value[i])<<(8*i);CHECK(build_frame_for_test(req(f.param==241?Kind::Auto:f.param==1123?Kind::Mode:Kind::Bypass,v),b,sizeof(b))==n);CHECK(!memcmp(b,f.bytes.data(),n));}
 }
 CHECK(params.size()==5);CHECK(controlFixtures.size()==13);
 CHECK(build_frame_for_test(req(Kind::Mode,2),b,sizeof(b))==0);CHECK(build_frame_for_test(req(Kind::Auto,2),b,sizeof(b))==0);CHECK(build_frame_for_test(req(Kind::Bypass,0x21),b,sizeof(b))==0);
 CHECK(pack_write_for_test(1,b,2,b,sizeof(b))==0);CHECK(pack_write_for_test(1,nullptr,4,b,sizeof(b))==0);CHECK(pack_write_for_test(1,b,4,nullptr,32)==0);
 // Missing/invalid protocol configuration must fail closed, not fall back to any shared credentials.
 const ProtocolCredentials blank{"",""},missing{nullptr,"DEMO"},tooLong{"ABCDEFGHIJKLMNOPQRSTUVWXY","DEMO"},badAscii{"BAD\n","DEMO"};
 CHECK(!protocol_credentials_valid(blank));CHECK(!protocol_credentials_valid(missing));CHECK(!protocol_credentials_valid(tooLong));CHECK(!protocol_credentials_valid(badAscii));
 CHECK(build_frame(req(Kind::Mode,3),blank,b,sizeof(b))==0);CHECK(pack_write(blank,1123,b,4,b,sizeof(b))==0);
 const ProtocolCredentials maxLength{"LLLLLLLLLLLLLLLLLLLLLLLL","PPPPPPPPPPPPPPPPPPPPPPPP"};uint8_t maximum[MAX_WRITE_FRAME+1];uint8_t value[4]={3,0,0,0};
 memset(maximum,0xAA,sizeof(maximum));CHECK(protocol_credentials_valid(maxLength));CHECK(pack_write(maxLength,1123,value,4,maximum,MAX_WRITE_FRAME)==MAX_WRITE_FRAME);CHECK(maximum[MAX_WRITE_FRAME]==0xAA);CHECK(gm3_validate_frame(maximum,MAX_WRITE_FRAME)==GM3_OK);CHECK(!pack_write(maxLength,1123,value,4,maximum,MAX_WRITE_FRAME-1));
 std::cout<<"PASS 13 synthetic wire fixtures with non-device credentials (including documentation-only relative/toggle/Party OFF encodings)\n";
}
struct ParseCounts {int good=0,rejected=0,partials=0;};
static void parsed(void* ctx,const gm3_result_t* r,uint32_t){auto& c=*static_cast<ParseCounts*>(ctx);CHECK(r->len<=512);if(r->status==GM3_OK){++c.good;CHECK(gm3_validate_frame(r->data,r->len)==GM3_OK);}else ++c.rejected;if(r->status==GM3_REJECT_PARTIAL_TIMEOUT)++c.partials;}
static void feed(gm3_parser_t& p,const std::vector<uint8_t>& b,uint32_t now,ParseCounts& c){for(uint8_t x:b)gm3_parser_byte(&p,x,now,100,parsed,&c);}
static void parser_tests(){
 auto good=telemetryFixtures[0].bytes;bool internal16=false;
 for(const auto& f:telemetryFixtures){if(std::find(f.bytes.begin()+3,f.bytes.end()-3,0x16)!=f.bytes.end()-3)internal16=true;gm3_parser_t p;gm3_parser_init(&p);ParseCounts c;feed(p,f.bytes,1,c);CHECK(c.good==1);CHECK(c.rejected==0);}
 CHECK(internal16);
 for(size_t n=0;n<good.size();++n){gm3_parser_t p;gm3_parser_init(&p);ParseCounts c;std::vector<uint8_t> trunc(good.begin(),good.begin()+n);feed(p,trunc,1,c);gm3_parser_tick(&p,101,100,parsed,&c);if(n)CHECK(c.partials>0);feed(p,good,102,c);CHECK(c.good==1);CHECK(p.len==0);}
 for(unsigned type=0;type<6;++type){gm3_parser_t p;gm3_parser_init(&p);ParseCounts c;auto bad=good;
  if(type==0)bad[1]=255,bad[2]=255;
  if(type==1)bad[1]=0,bad[2]=0;
  if(type==2)bad.back()=0;
  if(type==3)bad[bad.size()-2]^=1;
  if(type==4)bad[1]=250,bad[2]=0; // plausible corrupt long LEN with a complete nested successor
  if(type==5){bad={0x68,0xF0,0};bad.insert(bad.end(),good.begin(),good.end());}
  feed(p,bad,10,c);feed(p,good,11,c);gm3_parser_tick(&p,111,100,parsed,&c);CHECK(c.good>=1);CHECK(c.rejected>0);CHECK(p.len==0);
 }
 gm3_parser_t p;gm3_parser_init(&p);ParseCounts c;uint32_t r=0x12345678;
 for(int i=0;i<100000;++i){r=r*1664525u+1013904223u;gm3_parser_byte(&p,uint8_t(r>>24),i/10,100,parsed,&c);CHECK(p.len<512);}
 gm3_parser_tick(&p,20000,100,parsed,&c);CHECK(p.len==0);
 std::cout<<"PASS parser: every fixture, all truncation positions, bad CRC/end/length, nested resync, internal 0x16, 100000 noise bytes\n";
}
static void decode_tests(){
 bool observed[FIELD_COUNT]={};std::set<int> modes;
 for(const auto& c:telemetryFixtures){State s;CHECK(s.decode(c.bytes.data(),c.bytes.size(),1234));
  for(const auto& e:c.expected){observed[e.field]=true;CHECK(s.f[e.field].valid);CHECK(std::abs(s.f[e.field].value-e.value)<0.000001);CHECK(s.f[e.field].at==1234);if(e.field==MODE_A||e.field==MODE_B)modes.insert(int(e.value));}
  for(size_t n=0;n<c.bytes.size();++n){State t;CHECK(!t.decode(c.bytes.data(),n,1234));for(const auto& v:t.f)CHECK(!v.valid);}
  auto crc=c.bytes;crc[12]^=1;CHECK(!s.decode(crc.data(),crc.size(),1234));
  auto shortblock=c.bytes;shortblock.erase(shortblock.begin()+12);shortblock=seal(shortblock);State t;CHECK(!t.decode(shortblock.data(),shortblock.size(),1234));CHECK(t.layout_errors==1);
  auto count=c.bytes;count[8]++;count=seal(count);CHECK(!s.decode(count.data(),count.size(),1235));
  for(const auto& e:c.expected)CHECK(!s.f[e.field].valid);
  }
 for(bool o:observed){CHECK(o);}
 CHECK(modes==std::set<int>({3,4,5,6}));
 auto it=std::find_if(telemetryFixtures.begin(),telemetryFixtures.end(),[](const TelemetryFixture& c){return c.bytes[9]==0xE5&&c.bytes[10]==0;});CHECK(it!=telemetryFixtures.end());
 State s;CHECK(s.decode(it->bytes.data(),it->bytes.size(),1));auto bad=it->bytes;bad[11+10]=0xFF;bad=seal(bad);CHECK(s.decode(bad.data(),bad.size(),2));CHECK(!s.f[AUTO_A].valid);
 CHECK(!s.coherent(1));State x=fresh(1000);CHECK(x.coherent(1000));CHECK(x.gate(1000)==nullptr);CHECK(x.gate(17000)!=nullptr);x.f[MODE_B].put(5,1000);CHECK(!x.coherent(1000));
 x=fresh(1000);x.f[BYPASS].put(0x00110121,1000);CHECK(!strcmp(x.gate(1000),"unknown_bypass_layout"));
 std::cout<<"PASS 19 synthetic CRC-valid telemetry fixtures, all 21 fields, documented status fingerprints, all 4 modes, bounds/layout rejection\n";
}
static bool parse(const char* topic,const std::string& value,const char* nonce="0123456789abcdef"){Request r;return parse_command(topic,reinterpret_cast<const uint8_t*>(value.data()),value.size(),nonce,r);}
static void callback_tests(){
 const std::string token="0123456789abcdef:";
 for(const char* x:{"ON","OFF","on","off","1","0"})CHECK(parse("pa700li/set/auto",token+x));
 for(uint32_t v=3;v<=6;++v){Request r;std::string x=token+mode_name(v);CHECK(parse_command("pa700li/set/mode",reinterpret_cast<const uint8_t*>(x.data()),x.size(),"0123456789abcdef",r));CHECK(r.kind==Kind::Mode&&r.value==v);}
 for(const char* x:{"Auto","Closed","Open"})CHECK(parse("pa700li/set/bypass",token+x));
 for(const char* x:{"","garbage","ONNO","01","-1","2","0 "," ON","false","true"})CHECK(!parse("pa700li/set/auto",token+x));
 for(const char* x:{"Base","Boost","Compliance","3","Pause ","pause","0","6oops"})CHECK(!parse("pa700li/set/mode",token+x));
 CHECK(!parse("pa700li/set/party",token+"0"));CHECK(!parse("pa700li/set/boost",token+"2"));CHECK(!parse("pa700li/set/auto","ON"));CHECK(!parse("pa700li/set/auto",token+"ON","fedcba9876543210"));
 std::string nul=token+std::string("ON\0ignored",10);CHECK(!parse("pa700li/set/auto",nul));
 CHECK(!parse("pa700li/set/auto",token+"ON\n"));
 uint8_t bytes[128]={};Request r;
 for(size_t n=0;n<=sizeof(bytes);++n)CHECK(!parse_command("pa700li/set/auto",bytes,n,"0123456789abcdef",r));
 CHECK(!parse_command("pa700li/set/auto",nullptr,64,"0123456789abcdef",r));
 std::cout<<"PASS callback exact topic/payload validation, stale nonce replay rejection, embedded NUL/control chars/oversize rejection\n";
}
static void engine_tests(){
 uint8_t b[32];
 {State s;std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);CHECK(!e.active());CHECK(v.back().state=="rejected");}
 for(Kind k:{Kind::Mode,Kind::Auto,Kind::Bypass}){
  State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);Request r=req(k,k==Kind::Mode?3:k==Kind::Auto?1:0x00100111);
  e.submit(r,1000,false);CHECK(v.back().detail=="control_disabled");CHECK(!e.active());
  e.submit(r,1000,true);CHECK(e.awaiting_bus());e.submit(r,1001,true);CHECK(v.back().detail=="busy");
  CHECK(e.prepare(1002,b,sizeof(b))==(k==Kind::Auto?29:32));e.mark_sent(1003,true);CHECK(e.active());CHECK(!e.awaiting_bus());CHECK(!e.prepare(1004,b,sizeof(b)));
  e.generic_ack();e.tick(1004);CHECK(e.active()); // ACK alone NEVER confirms
  if(k==Kind::Mode){s.f[MODE_A].put(3,1100);e.tick(1100);CHECK(e.active());s.f[MODE_B].put(5,1200);e.tick(1200);CHECK(e.active());s.f[MODE_B].put(3,1300);}
  if(k==Kind::Auto){s.f[AUTO_A].put(1,1100);e.tick(1100);CHECK(e.active());s.f[AUTO_B].put(1,1300);}
  if(k==Kind::Bypass)s.f[BYPASS].put(0x00100111,1300);
  e.tick(1300);CHECK(!e.active());CHECK(v.back().state=="confirmed");CHECK(v.back().detail=="fresh_post_tx_readback");
  e.submit(r,1301,true);CHECK(!e.active());CHECK(v.back().detail=="already_at_target_no_tx");
 }
 {State s=fresh(1000);s.f[MODE_A].put(6,1000);s.f[MODE_B].put(6,1000);s.f[FAN_SUPPLY].put(0,1000);s.f[FAN_EXTRACT].put(0,1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Auto,1),1000,true);CHECK(v.back().detail=="auto_requires_compliance_mode");e.submit(req(Kind::Mode,4),1000,true);CHECK(e.prepare(1001,b,32)==32);e.mark_sent(1002,true);s.f[MODE_A].put(4,1100);s.f[MODE_B].put(4,1200);e.tick(1200);CHECK(v.back().state=="confirmed");CHECK(s.f[FAN_SUPPLY].value==0);}
 for(bool ack:{false,true}){State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);CHECK(e.prepare(1001,b,32)==32);e.mark_sent(1002,true);if(ack)e.generic_ack();e.tick(21002);CHECK(v.back().state==(ack?"uncertain":"timeout"));CHECK(!e.active());for(uint32_t t=21003;t<101000;t+=1000){e.tick(t);CHECK(e.prepare(t,b,32)==0);} }
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);CHECK(e.prepare(1001,b,32)==32);e.mark_sent(1002,true);s.f[MODE_A].put(3,1100);e.tick(21002);CHECK(v.back().state=="uncertain");} // partial readback remains uncertain even after it ages out
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);e.tick(4000);CHECK(v.back().detail=="not_sent_bus_busy");CHECK(!e.prepare(4001,b,32));}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);s.f[MODE_A].valid=false;CHECK(!e.prepare(1001,b,32));CHECK(v.back().state=="rejected");}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);e.cancel_queued("test_session_change");CHECK(v.back().state=="rejected");CHECK(!e.active());}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);e.mark_sent(1001,false);CHECK(v.back().state=="uncertain");CHECK(!e.active());}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);s.f[BOOST].put(2,1000);e.submit(req(Kind::Mode,3),1000,true);CHECK(v.back().detail=="boost_or_party_active");s.f[BOOST].put(0,1000);s.f[PARTY].put(2,1000);e.submit(req(Kind::Mode,3),1000,true);CHECK(v.back().detail=="boost_or_party_active");}
 {uint32_t t=0xFFFFF000u;State s=fresh(t);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),t,true);CHECK(e.prepare(t+1,b,32)==32);e.mark_sent(t+2,true);e.tick(uint32_t(t+2+20000));CHECK(v.back().state=="timeout");CHECK(!e.active());}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);e.mark_sent(1001,true);s.f[MODE_A].put(3,1000);s.f[MODE_B].put(3,1000);e.tick(1002);CHECK(e.active());} // stale timestamps do not become post-TX proof
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),17001,true);CHECK(v.back().state=="rejected");s=fresh(18000);s.f[AUTO_B].put(1,18000);e.submit(req(Kind::Mode,3),18000,true);CHECK(v.back().state=="rejected");}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Auto,1),1000,true);e.mark_sent(1001,true);s.f[AUTO_A].put(1,1100);s.f[AUTO_B].put(1,1100);s.f[MODE_A].put(6,1100);s.f[MODE_B].put(6,1100);e.tick(1100);CHECK(e.active());s.f[MODE_A].put(4,1200);s.f[MODE_B].put(4,1200);e.tick(1200);CHECK(!e.active());CHECK(v.back().state=="confirmed");}
 {State s=fresh(1000);std::vector<Recorded> v;Engine e(s,record,&v,syntheticCredentials);e.submit(req(Kind::Mode,3),1000,true);s.f[MODE_A].put(3,1100);s.f[MODE_B].put(3,1100);CHECK(!e.prepare(1100,b,32));CHECK(v.back().detail=="target_reached_before_tx_no_tx");CHECK(!e.active());}
 {State s=fresh(1000);std::vector<Recorded> v;const ProtocolCredentials blank{"",""};Engine e(s,record,&v,blank);e.submit(req(Kind::Mode,3),1000,true);CHECK(!e.active());CHECK(v.back().detail=="protocol_credentials_unconfigured");CHECK(!e.prepare(1001,b,sizeof(b)));}
 std::cout<<"PASS serialized command engine: fresh independent readbacks, stale/mismatch/busy rejection, ACK insufficient, timeout/uncertain, no automatic retries, no fan-start inference, rollover\n";
}
static void discovery_tests(){
 std::ofstream f("discovery.jsonl");char b[1200],small[8];
 for(const auto& s:sensors){CHECK(sensor_discovery(s,b,sizeof(b)));f<<b<<'\n';CHECK(!sensor_discovery(s,small,sizeof(small)));}
 for(Kind k:{Kind::Mode,Kind::Auto,Kind::Bypass}){CHECK(control_discovery(k,"0123456789abcdef",b,sizeof(b)));f<<b<<'\n';CHECK(!control_discovery(k,"0123456789abcdef",small,sizeof(small)));}
 CHECK(!control_discovery(Kind::Mode,"bad",b,sizeof(b)));
 State state;char payload[40];
 for(const auto& sensor:sensors){CHECK(!sensor_payload(sensor,state,1000,payload,sizeof(payload)));state.f[sensor.field].put(0,1000);CHECK(sensor_payload(sensor,state,1000,payload,sizeof(payload)));CHECK(!sensor_payload(sensor,state,31001,payload,sizeof(payload)));}
 const auto* countdown=std::find_if(std::begin(sensors),std::end(sensors),[](const SensorDef& d){return d.field==BOOST_MIN;});
 CHECK(countdown!=std::end(sensors));state.f[BOOST_MIN].put(65535,1000);CHECK(sensor_payload(*countdown,state,1000,payload,sizeof(payload)));CHECK(!strcmp(payload,"None"));CHECK(!sensor_payload(*countdown,state,1000,small,2));
 std::cout<<"PASS discovery generation and truncation detection\n";
}
int main(){wire_tests();parser_tests();decode_tests();callback_tests();engine_tests();discovery_tests();std::cout<<"PASS "<<checks<<" checks under ASan + UBSan\n";}
