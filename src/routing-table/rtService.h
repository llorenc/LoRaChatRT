#pragma once
#include "LoraMesher.h"
#include "config.h"
#include "message/messageManager.h"
#include "message/messageService.h"
#include "rtCommandService.h"
#include "rtServiceMessage.h"
#include <Arduino.h>
#include <cstdint>

#define RT_MQTT_ONE_MESSAGE

class RtService : public MessageService {
public:
  /**
   * @brief Construct a new GPSService object
   *
   */
  static RtService &getInstance() {
    static RtService instance;
    return instance;
  }
  void init();
  rtCommandService *rtCommandService_ = new rtCommandService();
  String getJSON(DataMessage *message);
  DataMessage *getDataMessage(JsonObject data);
  void processReceivedMessage(messagePort port, DataMessage *message);

private:
  RtService() : MessageService(RtApp, "Rt") {
    commandService = rtCommandService_;
  };
  void createSendingTask();
#if defined(RT_MQTT_ONE_MESSAGE)
  static void sendingLoopOneMessage(void *);
  rtOneMessage *createRTPayloadMessage(int number_of_neighbors) ;
#else
  static void sendingLoop(void *);
  void createAndSendMessage(uint16_t mcount, RouteNode *);
#endif
  TaskHandle_t sending_TaskHandle = NULL;
  bool running = false;
  bool isCreated = false;
  size_t rtMessageId = 0;
};
