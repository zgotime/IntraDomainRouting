#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include <map>
#include <stack>

struct Port_Status{
	int neighbor_router_id; // int for router id since -1 means no router
	unsigned short RTT;
	unsigned int expire_time;
};

struct DV_Info{
	unsigned short dest;
	unsigned short cost;
	unsigned int expire_time;
	unsigned short next_hop;
};


struct LS_Info{
	unsigned int sequence;
	std::map<unsigned short, unsigned short> LSP;
};
	



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
    // a neighbour router.
	
	
	void handle_ping_alarm();
	
	void handle_ls_update_alarm();
	
	void handle_dv_update_alarm();
	
	void handle_dv_refresh_alarm();
	
	void handle_ls_refresh_alarm();
	
	void handle_port_refresh_alarm();
	
	void handle_invalid_alarm();
	
	void handle_invalid_protocol_type();
	
	void handle_data_packet(unsigned short port, void* packet, unsigned short size);
	
	void handle_ping_packet(unsigned short port, void* packet, unsigned short size);
	
	void handle_pong_packet(unsigned short port, void* packet);
	
	void handle_ls_packet();
	
	void handle_dv_packet(unsigned short port, void* packet,unsigned short size);
	
	void handle_dv_stack();
	
	void handle_invalid_packet();

	void handle_send_data(unsigned short port, void* packet, unsigned short size);

 private:
    
	/* all kinds of alarms, stored as data via set_alarm */
	static const char* LS_UPDATE_ALARM; // linked state
	static const char* DV_UPDATE_ALARM; // distance vector
	static const char* PING_ALARM; // ping-pong
	static const char* LS_REFRESH_ALARM; // refresh data
	static const char* DV_REFRESH_ALARM; // refresh data
	static const char* PORT_REFRESH_ALARM; // refresh port data
	
	/* the interval for each signal in milliseconds */
	
	/* Ping signal */
	static const unsigned int PING_INTERVAL = 10000;
	static const unsigned int PONG_MAX_TIMEOUT = 15000;
	static const unsigned int PING_REFRESH_RATE =1000;
	
	/* DV signal */
	static const unsigned int DV_UPDATE_INTERVAL = 30000;
	static const unsigned int DV_MAX_TIMEOUT = 45000;
	static const unsigned int DV_REFRESH_RATE = 1000;
	
	/* LS signal */
	static const unsigned int LS_UPDATE_INTERVAL = 30000;
	static const unsigned int LS_MAX_TIMEOUT = 45000;
	static const unsigned int LS_REFRESH_RATE =1000;
	
	/* LS sequence */
	static unsigned long LS_SEQUENCE;

	/* Port data structure */
	Port_Status*  port_status_list;
	std::map<unsigned short, unsigned short> hop_to_port;

	/* DV data structure */
	std::map<unsigned short,DV_Info> dv_table;
	std::stack<DV_Info> dv_stack;

	/* LS data structure */
	std::map<unsigned short, LS_Info> ls_table;
	std::stack<LS_Info> ls_stack;

	
	Node *sys; // To store Node object; used to access GSR9999 interfaces 
	
	unsigned short num_ports;
	unsigned short router_id;
	eProtocolType protocol_type;
	
	
};

#endif

