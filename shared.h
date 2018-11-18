// This file contains the declarations of shared structs and methods

#ifndef SHARED_INCLUDE
#define SHARED_INCLUDE

#include "includes.h"

enum MSG_TYPE {OPEN, ACK, QUERY, ADD, RELAY};

struct Open_Data{
    int switch_number;
    int left;
    int right;
    int ip_range_lo;
    int ip_range_hi;

    void print();
};

struct Ack_Data{
    void print();
};

struct Query_Data{
    int source_ip;
    int dest_ip;

    void print();
};

enum actionType {FORWARD, DROP};

string actionType_to_string(enum actionType type);

struct flow_element{
    int scrIP_lo;
    int scrIP_hi;
    int destIP_lo;
    int destIP_hi;
    enum actionType actionType;
    int actionVal;
    int pri;
    int pktCount;

    void print();
};

struct Add_Data{
    struct flow_element rule;

    void print();
};

struct instruction{
    int source_ip;
    int dest_ip;
};

struct Relay_Data{
   struct instruction ins; 

   void print();
};

union MSG_DATA {
    struct Open_Data open_data; 
    struct Ack_Data ack_data;
    struct Query_Data query_data;
    struct Add_Data add_data;
    struct Relay_Data relay_data;
    };

struct message{
    enum MSG_TYPE type;
    union MSG_DATA data;

    void print();
};

string int_to_string(int i);
string msg_type_to_string(enum MSG_TYPE type);
void get_vector_input(vector<string> *split_input, string input);
void controller_handle_signal_USR1(int sigNo);
void switch_handle_signal_USR1(int sigNo);
#endif