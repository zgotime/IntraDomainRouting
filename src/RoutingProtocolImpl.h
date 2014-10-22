#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.
	
	
	void handle_ping_alarm();
	
	void handle_ls_alarm();
	
	void handle_dv_alarm();
	
	void handle_update_alarm();
	
	void handle_invalid_alarm();

 private:
    
	/* all kinds of alarms, stored as data via set_alarm */
	static const char* LS_ALARM; // linked state
	static const char* DV_ALARM; // distance vector
	static const char* PING_ALARM; // ping-pong
	static const char* UPDATE_ALARM; // update data
	
	/* the interval for each signal in milliseconds */
	
	/* Ping signal */
	static const unsigned int PING_INTERVAL = 10000;
	static const unsigned int PONG_MAX_TIMEOUT = 15000;
	
	/* DV signal */
	static const unsigned int DV_UPDATE_INTERVAL = 30000;
	static const unsigned int DV_MAX_TIMEOUT = 45000;
	static const unsigned int DV_REFRESH_RATE = 1000;
	
	/* LS signal */
	static const unsigned int LS_UPDATE_INTERVAL = 30000;
	static const unsigned int LS_MAX_TIMEOUT = 45000;
	static const unsigned int LS_REFRESH_RATE =1000;
	
	
	
	
	Node *sys; // To store Node object; used to access GSR9999 interfaces 
	
	unsigned short num_ports;
	unsigned short router_id;
	eProtocolType protocol_type;
	
	
};

#endif

