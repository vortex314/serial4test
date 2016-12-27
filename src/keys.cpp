#include <EventBus.h>

const char* hash2string(uint32_t hash)
{
    switch(hash)
    {
	case EB_DST :  return "dst";
    case EB_SRC :  return "src";
    case EB_EVENT :  return "event";
    case EB_REQUEST :  return "request";
    case EB_REPLY :  return "reply";
case H("clean_session") : return "clean_session";
case H("client_id") : return "client_id";
case H("closed") : return "closed";
case H("connect") : return "connect";
case H("data") : return "data";
case H("disconnect") : return "disconnect";
case H("disconnected") : return "disconnected";
case H("Echo") : return "Echo";
case H("err") : return "err";
case H("error") : return "error";
case H("error_detail") : return "error_detail";
case H("error_message") : return "error_message";
case H("error_msg") : return "error_msg";
case H("host") : return "host";
case H("keep_alive") : return "keep_alive";
case H("message") : return "message";
case H("mqtt") : return "mqtt";
case H("MqttCl") : return "MqttCl";
case H("mqtt.puback") : return "mqtt.puback";
case H("opened") : return "opened";
case H("password") : return "password";
case H("ping") : return "ping";
case H("port") : return "port";
case H("publish") : return "publish";
case H("published") : return "published";
case H("qos") : return "qos";
case H("retain") : return "retain";
case H("retained") : return "retained";
case H("rxd") : return "rxd";
case H("serial") : return "serial";
case H("subscribe") : return "subscribe";
case H("sys") : return "sys";
case H("tcp") : return "tcp";
case H("Tester") : return "Tester";
case H("tick") : return "tick";
case H("timeout") : return "timeout";
case H("topic") : return "topic";
case H("uint32_t") : return "uint32_t";
case H("user") : return "user";
case H("value") : return "value";
case H("will_message") : return "will_message";
case H("will_qos") : return "will_qos";
case H("will_retain") : return "will_retain";
case H("will_topic") : return "will_topic";
default :
        return "UNDEFINED";
    }
}
