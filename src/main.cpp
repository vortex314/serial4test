#include <iostream>

/*
 * DEVICE <-> Gateway
 *
 * -> mqtt.connect,host,port,will.topic,will.message,will.qos,will.retain,clientId,user,password
 * <- mqtt.connack,error
 *
 * -> mqtt.publish,topic,message,qos,retain
 * <- mqtt.pubcomp
 *
 * -> mqtt.subscribe
 * <- mqtt.suback
 *
 * -> mqtt.unsubscribe
 * <- mqtt.unsuback
 *
 * -> mqtt.pingreq
 * <- mqtt.pingresp
 *
 * -> mqtt.disconnect
 */

using namespace std;
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <stdint.h>
#include <string.h>
#include <cstdlib>

#include "Str.h"
#include <time.h>
//#include "Timer.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
//#include "Sequence.h"
#include <unistd.h>
//#include "Thread.h"
// #include "Sequence.h"
#include "Tcp.h"
#include "Serial.h"
#include <time.h>
#include <Actor.h>
#define TIMER_TICK 1000

#include <Log.h>
#include <CborQueue.h>
#include <SlipStream.h>
#include <EventBus.h>

EventBus eb(10240,1024);

struct {
    const char* host;
    uint16_t port;
    uint32_t baudrate;
    const char* device;
    LogManager::LogLevel logLevel;

} context = { "limero.ddns.net", 1883, 115200, "/dev/ttyACM0",
              LogManager::LOG_DEBUG
            };
Cbor mqttConfig(200);

Serial serial("/dev/ttyACM1");

//_______________________________________________________________________________________
//
// simulates RTOS generating events into queue : Timer::TICK,Serial::RXD,Serial::CONNECTED,...
//_______________________________________________________________________________________

void poller(int serialFd, int tcpFd, uint64_t sleepTill) {
    Cbor cbor(1024);
    Bytes bytes(1024);
    uint8_t buffer[1024];
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    struct timeval tv;
    int retval;
    uint64_t start = Sys::millis();
//    uint64_t delta=1000;
    if (serialFd == 0 && tcpFd == 0) {
        usleep(1000);
//       sleep(1);
//        eb.publish(H("sys"),H("tick"));
    } else {

        // Watch serialFd and tcpFd  to see when it has input.
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        if (serialFd)
            FD_SET(serialFd, &rfds);
        if (tcpFd)
            FD_SET(tcpFd, &rfds);
        if (serialFd)
            FD_SET(serialFd, &efds);
        if (tcpFd)
            FD_SET(tcpFd, &efds);

        // Wait up to 1000 msec.
        uint64_t delta = 1000;
        if (sleepTill > Sys::millis()) {
            delta = sleepTill - Sys::millis();
        }

        tv.tv_sec = delta / 1000;
        tv.tv_usec = (delta * 1000) % 1000000;

        int maxFd = serialFd < tcpFd ? tcpFd : serialFd;
        maxFd += 1;

        start = Sys::millis();

        retval = select(maxFd, &rfds, NULL, &efds, &tv);

        if (retval < 0) {
            LOGF(" select() : %d %s", retval, strerror(retval));
            sleep(1);
        } else if (retval > 0) { // one of the fd was set
            if (FD_ISSET(serialFd, &rfds)) {
                int size = ::read(serialFd, buffer, sizeof(buffer));
                if (size > 0) {
                    Str str(size * 3);
                    for (int i = 0; i < size; i++)
                        str.appendHex(buffer[i]);
                    LOGF(" rxd [%d] : %s", size,str.c_str());
//					fprintf(stdout, "%s\n", str.c_str());
                    for (int i = 0; i < size; i++)
                        bytes.write(buffer[i]);
                    eb.event(H("serial"),H("rxd")).addKeyValue(H("data"),bytes);
                    eb.send();
                } else {
                    eb.publish(H("serial"),H("err"));
                    serial.close();
                }
            }
            if (FD_ISSET(tcpFd, &rfds)) {
                eb.publish(H("tcp"),H("rxd"));
            }
            if (FD_ISSET(serialFd, &efds)) {
                eb.publish(H("serial"),H("err"));
            }
            if (FD_ISSET(tcpFd, &efds)) {
                eb.publish(H("tcp"),H("err"));
            }
        } else {
            //TODO publish TIMER_TICK
//           eb.publish(H("sys"),H("tick"));
        }
    }
    uint64_t waitTime = Sys::millis() - start;
    if (waitTime > 1) {
//        LOGF(" waited %d/%d msec.",waitTime,delta);
    }
}

/*_______________________________________________________________________________

 loadOptions  role :
 - parse commandline otions
 h : host of mqtt server
 p : port
 d : the serial device "/dev/ttyACM*"
 b : the baudrate set ( only usefull for a serial2serial box or a real serial port )
 ________________________________________________________________________________*/

#include "Tcp.h"
#include "Log.h"

void loadOptions(int argc, char* argv[]) {
    int c;
    while ((c = getopt(argc, argv, "h:p:d:b:l:")) != -1)
        switch (c) {
        case 'h':
            mqttConfig.addKeyValue(H("host"), optarg);
            break;
        case 'p':
            mqttConfig.addKeyValue(H("host"), atoi(optarg));
            break;
        case 'd':
            context.device = optarg;
            break;
        case 'b':
            context.baudrate = atoi(optarg);
            break;
        case 'l':
            if (strcmp(optarg, "DEBUG") == 0)
                context.logLevel = LogManager::LOG_DEBUG;
            if (strcmp(optarg, "INFO") == 0)
                context.logLevel = LogManager::LOG_INFO;
            if (strcmp(optarg, "WARN") == 0)
                context.logLevel = LogManager::LOG_WARN;
            if (strcmp(optarg, "ERROR") == 0)
                context.logLevel = LogManager::LOG_ERROR;
            if (strcmp(optarg, "FATAL") == 0)
                context.logLevel = LogManager::LOG_FATAL;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            return;
        default:
            abort();
            break;
        }
}

#include <signal.h>
#include <execinfo.h>

void SignalHandler(int signal_number) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:%s \n", signal_number,
            strsignal(signal_number));
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

void interceptAllSignals() {
    signal(SIGFPE, SignalHandler);
    signal(SIGILL, SignalHandler);
    signal(SIGINT, SignalHandler);
    signal(SIGSEGV, SignalHandler);
    signal(SIGTERM, SignalHandler);
}

SlipStream slip(1024, serial);
#include "MqttClient.h"

void onSerialRxd(Cbor& cbor) {
    Bytes data(1000);
    if (cbor.gotoKey(H("data"))) {
        if (cbor.get(data)) {
//           LOGF(" data length : %d ",data.length());
            data.offset(0);
            while (data.hasData()) {
                slip.onRecv(data.read());
            }
        } else LOGF(" no data ");
    } else LOGF(" no key data ");
}

void publishInt(const char* topic,uint32_t value) {

    char sValue[30];
    sprintf(sValue,"%u",value);

    eb.request(H("mqtt"),H("publish"),H("MqttCl")).addKeyValue(H("topic"),topic).addKeyValue(H("message"),sValue);
    eb.send();

}

extern void logCbor(Cbor&);

//_______________________________________________________________________________________________________________________________________
//
#define PREFIX "limero/"
class Counter {
    uint32_t _interval;
    const char* _name;
    uint64_t _start;
    uint32_t _count;
public:

    Counter(const char* name,uint32_t interval) {
        _interval=interval*1000;
        _name=name;
        _start=0;
    }

    void inc() {
        _count++;
        if ( Sys::_upTime- _start > _interval ) flush();
    }

    void flush() {
        char field[30];
        strcpy(field,PREFIX);

        strcat(field,_name);
        strcat(field,"/count");
        publishInt(field,_count);

        strcpy(field,PREFIX);
        strcat(field,_name);
        strcat(field,"/perSec");
        publishInt(field,_count/(_start-Sys::millis()));

        _start=Sys::millis();
        _count=0;
    }


};
Counter requests("requests",10);
Counter responses("responses",10);
Counter mismatchs("mismatch",10);
Counter correct("correct",10);
Counter timeouts("timeout",10);

//_______________________________________________________________________________________________________________________________________
//
class Tester: public Actor {
    uint32_t _counter;
    uint32_t _correct;
    uint32_t _incorrect;
    uint32_t _timeouts;
public:
    Tester() :
        Actor("Tester") {
        _counter=0;
        _correct=0;
        _incorrect=0;
        _timeouts=0;
    }
    void setup() {
        timeout(1000);
        eb.onReply(0,H("ping")).subscribe(this);
    }
    void onEvent(Cbor& msg) {
        PT_BEGIN()
        ;

        while (true) {
//           timeout(5000);
//           PT_YIELD_UNTIL(  timeout());

            eb.request(H("Echo"),H("ping"),H("Tester")).addKeyValue(H("uint32_t"),_counter);
            eb.send();
            requests.inc();

            timeout(5000);
            PT_YIELD_UNTIL( eb.isReply(0,H("ping")) || eb.isEvent(H("sys"),H("timeout")));
            uint32_t counter;
            if ( msg.getKeyValue(H("uint32_t"),counter) ) {
                if ( counter==_counter ) {
                    correct.inc();
                } else {
                    mismatchs.inc();
                }
            } else {
                timeouts.inc();
            }

        }
        PT_END()
    }
};

Tester tester;

class MqttCl: public Actor {
    uint32_t _error;
public:
    MqttCl() :
        Actor("MqttCl") {

    }
    void setup() {
        timeout(1000);
        eb.onDst(H("MqttCl")).subscribe(this);
    }
    void onEvent(Cbor& msg) {

        PT_BEGIN()
        ;

DISCONNECT : {
            eb.request(H("mqtt"),H("disconnect"),H("MqttCl"));
            eb.send();
            requests.inc();
            timeout(2000);

            PT_YIELD_UNTIL(timeout());
            goto CONNECTING;

        }
CONNECTING : {
            while (true) {
                eb.request(H("mqtt"),H("connect"),H("MqttCl")).addKeyValue(H("host"),"localhost").addKeyValue(H("port"),1883);
                eb.send();
                requests.inc();
                timeout(3000);

                PT_YIELD_UNTIL( eb.isReply(H("mqtt"),H("connect")) ||  timeout() );

                if ( eb.isReply(H("mqtt"),H("connect")) && msg.getKeyValue(H("error"),_error) && _error == 0 ) {
                    responses.inc();
                    goto CONNECTED;
                }
            }
        }
CONNECTED : {
            while(true) {
                char sTime[30];
                sprintf(sTime,"%ld",Sys::millis());
                eb.request(H("mqtt"),H("publish"),H("MqttCl")).addKeyValue(H("topic"),"limero/topic").addKeyValue(H("message"),sTime);
                eb.send();
                requests.inc();
                timeout(3000);

                PT_YIELD_UNTIL( eb.isReply(H("mqtt"),H("publish")) || timeout() );
                if ( timeout() ) goto DISCONNECT;

                if ( eb.isReply(H("mqtt"),H("publish")) && msg.getKeyValue(H("error"),_error) && _error == 0 ) {
                    LOGF(" publish succeeded ");
                    responses.inc();
                } else {

                    goto DISCONNECT;
                }
            }
        }

        PT_END()
    }
};


MqttCl mqttCl;


//_______________________________________________________________________________________________________________________________________
//
class SerialConnector: public Actor {
    uint32_t _error;
public:
    SerialConnector() :
        Actor("SerialConnector") {
    }
    void setup() {
        timeout(1000);
        eb.onEvent(H("serial"),0).subscribe(this);
//		eb.subscribe(H("timeout"), this, (MethodHandler) &Tracer::onEvent); // default subscribed to timeouts
    }
    void onEvent(Cbor& msg) {
        PT_BEGIN()
        ;
CONNECTING: {
            while (true) {
                timeout(2000);
                PT_YIELD_UNTIL(eb.isEvent(H("sys"),H("timeout")));
                Erc erc = serial.open();
                LOGF(" serial.open()= %d : %s", erc, strerror(erc));
                if (erc == 0) {
                    eb.publish(H("serial"),H("opened"));
                    goto CONNECTED;
                }
            };
CONNECTED: {
                PT_YIELD_UNTIL(eb.isEvent(H("serial"),H("closed")) );
                goto CONNECTING;
            }
        }
        PT_END()
    }
};

SerialConnector serialConnector;


extern void logCbor(Cbor& cbor);

int main(int argc, char *argv[]) {

    LOGF("Start %s version : %s %s", argv[0], __DATE__, __TIME__);
    LOGF(" H('sys') : %d   H('timeout')=%d", H("sys"),H("timeout"));
    static_assert(H("timeout")==45638," timout hash incorrect");

    loadOptions(argc, argv);

    interceptAllSignals();

    serial.setDevice(context.device);
    serial.setBaudrate(context.baudrate);
    tester.setup();
    serialConnector.setup();
    mqttCl.setup();

//	serial_connect();

    eb.onAny().subscribe([](Cbor& cbor) {
        logCbor(cbor);
    });


    eb.onEvent(H("serial"),H("rxd")).subscribe( [](Cbor& cbor) { // send serial data to slip processing
//				LOGF(" serial.rxd execute ");
        Bytes data(1000);
        if (cbor.getKeyValue(H("data"),data)) {
            data.offset(0);
            while (data.hasData()) {
                slip.onRecv(data.read());
            }
        } else LOGF(" no serial data ");
    });

// OUTGOING MQTT
    eb.onAny().subscribe( [](Cbor& cbor) { // route events to gateway
        if (  eb.isRequest(H("Echo"),0) || eb.isRequest(H("mqtt"),0)) {
            slip.send(cbor);
//            LOGF("send");
        }
    });


//	Actor::setupAll();
    eb.publish(H("serial"),H("closed"));
    while (1) {
        poller(serial.fd(), 0, 1000);
        eb.eventLoop();
    }

};

