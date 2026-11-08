#pragma once
#include "control_core.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
namespace proair {
struct SensorDef{Field field;const char* id;const char* name;const char* unit;const char* device_class;bool integer;};
static const SensorDef sensors[]={
 {TEMP_EXTRACT,"extract_temp","Extract Temperature","\xC2\xB0" "C","temperature",false},
 {TEMP_FRESH,"fresh_temp","Fresh Temperature","\xC2\xB0" "C","temperature",false},
 {TEMP_SUPPLY,"supply_temp","Supply Temperature","\xC2\xB0" "C","temperature",false},
 {TEMP_EXHAUST,"exhaust_temp","Exhaust Temperature","\xC2\xB0" "C","temperature",false},
 {HUMIDITY,"humidity","Humidity","%","humidity",false},{CO2,"co2","CO2","ppm","carbon_dioxide",false},
 {FILTER_DEPLETION,"filter","Filter Depletion","%",nullptr,false},
 {FAN_SUPPLY,"supply_fan","Supply Fan","%",nullptr,true},{FAN_EXTRACT,"extract_fan","Extract Fan","%",nullptr,true},
 {VOLT_SUPPLY,"supply_v","Supply Fan Voltage","V","voltage",false},{VOLT_EXTRACT,"extract_v","Extract Fan Voltage","V","voltage",false},
 {BOOST_MIN,"boost_min","Short Boost Remaining","min",nullptr,true},
 {BOOST,"boost_status","p237 Status (2 = Short Boost)",nullptr,nullptr,true},
 {PARTY,"party_status","Party State (read only)",nullptr,nullptr,true},
 {BYPASS_ACTIVE,"bypass_active","Bypass Active",nullptr,nullptr,true},
 {BYPASS_POSITION,"bypass_position","Bypass Position (candidate)","%",nullptr,false}
};
static constexpr size_t SENSOR_COUNT=sizeof(sensors)/sizeof(sensors[0]);
inline bool sensor_payload(const SensorDef& def,const State& state,uint32_t now,char* out,size_t cap){
 const Sample& v=state.f[def.field];
 if(!v.fresh(now,SENSOR_FRESH_MS)||!isfinite(v.value))return false;
 int n;
 if(def.field==BOOST_MIN&&v.value==65535)n=snprintf(out,cap,"None");
 else if(def.integer)n=snprintf(out,cap,"%lu",(unsigned long)v.value);
 else n=snprintf(out,cap,"%.2f",v.value);
 return n>=0&&size_t(n)<cap;
}
static const char* device_json="\"device\":{\"identifiers\":[\"pa700li\"],\"name\":\"PA700LI\"}";
inline bool sensor_discovery(const SensorDef& s,char* b,size_t cap){
 char unit[96]="",dc[96]="";
 if(s.unit)snprintf(unit,sizeof(unit),",\"unit_of_measurement\":\"%s\"",s.unit);
 if(s.device_class)snprintf(dc,sizeof(dc),",\"device_class\":\"%s\"",s.device_class);
 const char* topicid=s.field==BOOST?"boost":s.id;
 int n=snprintf(b,cap,"{\"name\":\"PA700LI %s\",\"unique_id\":\"pa700li_%s\",\"state_topic\":\"pa700li/sensor/%s\",\"availability_topic\":\"pa700li/availability\",\"expire_after\":30%s%s,%s}",s.name,s.id,topicid,unit,dc,device_json);
 return n>=0&&size_t(n)<cap;
}
inline bool control_discovery(Kind k,const char* nonce,char* b,size_t cap){
 if(!nonce||strlen(nonce)!=16)return false;
 const char* id=kind_name(k);char specific[512];
 if(k==Kind::Auto)snprintf(specific,sizeof(specific),"\"payload_on\":\"%s:ON\",\"payload_off\":\"%s:OFF\",\"state_on\":\"1\",\"state_off\":\"0\",\"state_topic\":\"pa700li/sensor/auto\"",nonce,nonce);
 else snprintf(specific,sizeof(specific),"\"state_topic\":\"pa700li/state/%s\",\"command_template\":\"%s:{{ value }}\",\"options\":%s",id,nonce,k==Kind::Mode?"[\"Compliance Level 1\",\"Compliance Level 2\",\"Compliance Level 3\",\"Pause\"]":"[\"Auto\",\"Closed\",\"Open\"]");
 int n=snprintf(b,cap,"{\"name\":\"PA700LI %s\",\"unique_id\":\"pa700li_%s\",\"command_topic\":\"pa700li/set/%s\",\"optimistic\":false,\"retain\":false,\"qos\":0,\"availability_mode\":\"all\",\"availability\":[{\"topic\":\"pa700li/availability\"},{\"topic\":\"pa700li/availability/control\"}],%s,%s}",id,id,id,specific,device_json);
 return n>=0&&size_t(n)<cap;
}
}
