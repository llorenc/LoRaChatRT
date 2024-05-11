#include "rtServiceMessage.h"
#include "rtService.h"
#include "loramesh/loraMeshService.h"
#include "LoraMesher.h"

#if defined(RT_MQTT_ONE_MESSAGE)
static const char *RT_TAG = "RtOMService";
#else
static const char *RT_TAG = "RtService";
#endif

void RtService::init() {
  ESP_LOGI(RT_TAG, "Initializing mqtt_rt");
  createSendingTask();
}

String RtService::getJSON(DataMessage *message) {
  rtMessage *bm = (rtMessage *)message;
  StaticJsonDocument<2000> doc;
  JsonObject data = doc.createNestedObject("RT") ;
  if (bm->RTcount == RTCOUNT_RTONEMESSAGE) {
    rtOneMessage *rt = (rtOneMessage *)message ;
    rt->serialize(data);
  } else {
    bm->serialize(data);
  }
  String json ;
  serializeJson(doc, json);
  return json;
}

DataMessage *RtService::getDataMessage(JsonObject data) {
  if(data["RTcount"] == RTCOUNT_RTONEMESSAGE) {
    rtOneMessage *rt = new rtOneMessage() ;
    rt->deserialize(data);
    rt->messageSize = sizeof(rtOneMessage) - sizeof(DataMessageGeneric);
    return ((DataMessage *)rt);
  }
  rtMessage *rt = new rtMessage();
  rt->deserialize(data);
  rt->messageSize = sizeof(rtMessage) - sizeof(DataMessageGeneric);
  return ((DataMessage *)rt);
}

void RtService::processReceivedMessage(messagePort port, DataMessage *message) {
  ESP_LOGI(RT_TAG, "Received rt data");
}

void RtService::createSendingTask() {
  BaseType_t res = xTaskCreatePinnedToCore(
#if defined(RT_MQTT_ONE_MESSAGE)
      sendingLoopOneMessage, /* Function to implement the task */
#else
      sendingLoop, /* Function to implement the task */
#endif      
      "SendingTask",       /* Name of the task */
                              6000,                /* Stack size in words */
                              NULL,                /* Task input parameter */
                              1,                   /* Priority of the task */
                              &sending_TaskHandle, /* Task handle. */
                              0); /* Core where the task should run */
  if (res != pdPASS) {
    ESP_LOGE(RT_TAG, "Sending task creation failed");
    return;
  }
  ESP_LOGI(RT_TAG, "Sending task created");
  running = true;
  isCreated = true;
}

#if defined(RT_MQTT_ONE_MESSAGE)

rtOneMessage* RtService::createRTPayloadMessage(int number_of_neighbors) {
    uint32_t messageSize = sizeof(rtOneMessage) + sizeof(uint32_t)*number_of_neighbors ;
    rtOneMessage* RTMessage = (rtOneMessage*) pvPortMalloc(messageSize);
    RTMessage->messageSize = messageSize - sizeof(DataMessageGeneric);
    RTMessage->RTcount = RTCOUNT_RTONEMESSAGE ;
    RTMessage->number_of_neighbors = number_of_neighbors ;
    RTMessage->appPortDst = appPort::MQTTApp;
    RTMessage->appPortSrc = appPort::RtApp;
    RTMessage->addrSrc = LoraMesher::getInstance().getLocalAddress() ;
    RTMessage->addrDst = 0 ;
    RTMessage->messageId = rtMessageId ;
    return RTMessage ;
}

void RtService::sendingLoopOneMessage(void *parameter) {
  RtService &rtService = RtService::getInstance();
  UBaseType_t uxHighWaterMark;
  ESP_LOGI(RT_TAG, "entering sendingLoop");
  while (true) {
    if (!rtService.running) {
      // Wait until a notification to start the task
      ESP_LOGI(RT_TAG, "Wait notification to start the task");
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      ESP_LOGI(RT_TAG, "received notification to start the task");
    } else {
      uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
      ESP_LOGD(RT_TAG, "Stack space unused after entering the task: %d",
               uxHighWaterMark);
      RoutingTableService::printRoutingTable();  
      LM_LinkedList<RouteNode>* routingTableList = LoRaMeshService::getInstance().radio.routingTableListCopy();
      // count neighbors
      routingTableList->setInUse();
      if (routingTableList->moveToStart()) {
        RtService::getInstance().rtMessageId++;
        uint16_t rtMessagecount = 0 ;
        do {
          RouteNode *rtn = routingTableList->getCurrent() ;
          if(rtn->networkNode.address == rtn->via) {
            ++rtMessagecount ;
          } ;
        } while (routingTableList->next());
        if (rtMessagecount > 0) {
          routingTableList->moveToStart() ;
          rtOneMessage *RTMessage = getInstance().createRTPayloadMessage(rtMessagecount) ;
          int i = 0 ;
          do {
            RouteNode *rtn = routingTableList->getCurrent() ;
            if(rtn->networkNode.address == rtn->via) {
              RTMessage->neighbors[i++] = rtn->networkNode.address;
            }
          } while (routingTableList->next());
          ESP_LOGV(RT_TAG, "sending rtOneMessage") ;
          // Send the message
          MessageManager::getInstance().sendMessage(messagePort::MqttPort,
                                                    (DataMessage *)RTMessage);
          // Delete the message
          vPortFree(RTMessage) ;
        } else {
          ESP_LOGD(RT_TAG, "sendingLoopOneMessage: no neighbors?") ;
        }
      } else {
        ESP_LOGD(RT_TAG, "No routes") ;
      }
      //Release routing table list usage.
      routingTableList->releaseInUse();
      routingTableList->Clear();
      // end send RT
      vTaskDelay(RT_SENDING_EVERY / portTICK_PERIOD_MS);
      // Print the free heap memory
      ESP_LOGD(RT_TAG, "Free heap: %d", esp_get_free_heap_size());
    }
  }
}

#else

void RtService::sendingLoop(void *parameter) {
  RtService &rtService = RtService::getInstance();
  UBaseType_t uxHighWaterMark;
  ESP_LOGI(RT_TAG, "entering sendingLoop");
  while (true) {
    if (!rtService.running) {
      // Wait until a notification to start the task
      ESP_LOGI(RT_TAG, "Wait notification to start the task");
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      ESP_LOGI(RT_TAG, "received notification to start the task");
    } else {
      uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
      ESP_LOGD(RT_TAG, "Stack space unused after entering the task: %d",
               uxHighWaterMark);
      // send RT, one entri per mqtt message
      // from RoutingTableService::printRoutingTable
      // LM_LinkedList<RouteNode> *routingTableList =
      // LoRaMeshService::getInstance().get_routingTableList();
      RoutingTableService::printRoutingTable();  
      LM_LinkedList<RouteNode>* routingTableList = LoRaMeshService::getInstance().radio.routingTableListCopy();
      routingTableList->setInUse();
      if (routingTableList->moveToStart()) {
        RtService::getInstance().rtMessageId++;
        uint16_t rtMessagecount = 0 ;
        do {
          RouteNode *rtn = routingTableList->getCurrent() ;
          getInstance().createAndSendMessage(++rtMessagecount, rtn) ;
        } while (routingTableList->next());
      }
      else {
        ESP_LOGD(RT_TAG, "No routes") ;
      }
      //Release routing table list usage.
      routingTableList->releaseInUse();
      routingTableList->Clear();
      // end send RT
      vTaskDelay(RT_SENDING_EVERY / portTICK_PERIOD_MS);
      // Print the free heap memory
      ESP_LOGD(RT_TAG, "Free heap: %d", esp_get_free_heap_size());
    }
  }
}

void RtService::createAndSendMessage(uint16_t mcount, RouteNode *rtn) {
  ESP_LOGV(RT_TAG, "Sending rt data %d", RtService::getInstance().rtMessageId) ;
  rtMessage *message = new rtMessage();
  message->appPortDst = appPort::MQTTApp;
  message->appPortSrc = appPort::RtApp;
  message->messageId = rtMessageId ;
  message->addrSrc = LoraMesher::getInstance().getLocalAddress();
  message->addrDst = 0;
  //
  message->RTcount = mcount ;
  message->address = rtn->networkNode.address ;
  message->metric = rtn->networkNode.metric ;
  message->via = rtn->via ;
  message->receivedSNR = rtn->receivedSNR ;
  message->sentSNR = rtn->sentSNR ;
  message->SRTT = rtn->SRTT ;
  message->RTTVAR = rtn->RTTVAR ;
  ESP_LOGV(RT_TAG, "routing table");  
  message->messageSize = sizeof(rtMessage) - sizeof(DataMessageGeneric);
  // Send the message
  MessageManager::getInstance().sendMessage(messagePort::MqttPort,
                                            (DataMessage *)message);
  // Delete the message
  delete message;
}

#endif
