#pragma once

#include <Arduino.h>
#include "message/dataMessage.h"

#define MONCOUNT_MONONEMESSAGE UINT16_MAX

#pragma pack(1)
class monMessage: public DataMessageGeneric {
  // see LoRaMesher/src/entities/routingTable/RouteNode.h
public:
  uint16_t RTcount = 0;
  uint16_t address = 0;
  uint16_t via = 0;
  uint8_t metric = 0;
  int8_t receivedSNR = 0;
  int8_t sentSNR = 0;
  unsigned long SRTT = 0;
  unsigned long RTTVAR = 0;
  void serialize(JsonObject &doc) {
    // Call the base class serialize function
    ((DataMessageGeneric *)(this))->serialize(doc);
    // Add the derived class data to the JSON object
    doc["RTcount"] = RTcount ;
    doc["address"] = address;
    doc["via"] = via;
    doc["metric"] = metric;
    doc["receivedSNR"] = receivedSNR;
    doc["sentSNR"] = sentSNR;
    doc["SRTT"] = SRTT;
    doc["RTTVAR"] = RTTVAR;
  }
  void deserialize(JsonObject &doc) {
    // Call the base class deserialize function
    ((DataMessageGeneric *)(this))->deserialize(doc);
    // Add the derived class data to the JSON object
    RTcount = doc["RTcount"];
    address = doc["address"];
    via = doc["via"];
    metric = doc["metric"];
    receivedSNR = doc["receivedSNR"];
    sentSNR = doc["sentSNR"];
    SRTT = doc["SRTT"];
    RTTVAR = doc["RTTVAR"];
  }
};

struct routing_entry {
  uint32_t neighbor ;
  int8_t RxSNR ;
  unsigned long SRTT ;
} ;  

class monOneMessage: public DataMessageGeneric {
  // see LoRaMesher/src/entities/routingTable/RouteNode.h
public:
  uint16_t RTcount = MONCOUNT_MONONEMESSAGE ; // for backward compatibility
  unsigned long uptime ;
  uint32_t number_of_neighbors ;
  routing_entry mon[] ;
  void operator delete(void *ptr) {
    ESP_LOGI("monOneMessage", "delete");
    vPortFree(ptr);
  };
  void serialize(JsonObject &doc) {
    // Call the base class serialize function
    ((DataMessageGeneric *)(this))->serialize(doc);
    // Add the derived class data to the JSON object
    doc["RTcount"] = MONCOUNT_MONONEMESSAGE ;
    doc["uptime"] = uptime ;
    doc["number_of_neighbors"] = number_of_neighbors ;
    JsonArray monArray = doc.createNestedArray("mon");
    for (int i = 0; i < number_of_neighbors ; i++) {
      monArray[i]["neighbor"] = mon[i].neighbor ;
      monArray[i]["RxSNR"] = mon[i].RxSNR ;
      monArray[i]["SRTT"] = mon[i].SRTT ;
    }
  }
  void deserialize(JsonObject &doc) {
    // Call the base class deserialize function
    ((DataMessageGeneric *)(this))->deserialize(doc);
    // Add the derived class data to the JSON object
    RTcount = doc["RTcount"] ;
    uptime = doc["uptime"] ;
    number_of_neighbors = doc["number_of_neighbors"] ;
    for (int i = 0; i < number_of_neighbors ; i++) {
      mon[i].neighbor = doc["mon"][i]["neighbor"] ;
      mon[i].RxSNR = doc["mon"][i]["RxSNR"] ;
      mon[i].SRTT = doc["mon"][i]["SRTT"] ;
    }
  }
};

#pragma pack()
