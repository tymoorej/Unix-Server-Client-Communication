#ifndef CONTROLLER_INCLUDE
#define CONTROLLER_INCLUDE

#include "includes.h"

struct controller_signal_count{
    int open;
    int ack;
    int query;
    int add;

    controller_signal_count();

    void print_receiving();
    void print_transmitting();
};

struct controller_total_signals{
    struct controller_signal_count recieved;
    struct controller_signal_count transmitted;

    void print();
};

struct controller_known_switch_data{
    int switch_number;
    int fd;
    int left;
    int right;
    int ip_lo;
    int ip_hi;
    bool active;

    controller_known_switch_data(int switch_number);
    void print();
};

class Controller{
    struct sigaction oldAct, newAct;
    struct controller_total_signals signals;
    vector<struct controller_known_switch_data> switches;
    int num_of_switches, port_number, socket_fd;

    void setup(struct pollfd fdarray[]);
    void main(struct pollfd fdarray[]);
    void shutdown();
    void exit_program();
    
    void setup_socket();
    
    void initialize_poll_array(struct pollfd fdarray[]);

    void send_message(struct message m, int target);
    void send_ack_message(int switch_number);
    void send_add_message(struct Query_Data rule, int switch_number);
    void send_add_drop_message(int dest_ip_lo, int dest_ip_hi, int switch_number);
    void send_add_forward_message(int dest_ip_lo, int dest_ip_hi, int switch_number, int port);
    void set_open_data(struct Open_Data data, int fd);

    void recieve_message(message m, int fd);
    int get_switch_number_from_fd(int fd);
    
    struct controller_known_switch_data get_switch_data(int switch_number);
    bool set_switch_data(int switch_number, struct controller_known_switch_data data);
    void print_switch_info();

    void dest_ip_to_ip_range(int dest_ip, int dest_ip_range[]);
    bool ip_not_known(int dest_ip);
    
    void path_to_target(int switch_number, int dest_ip);
    bool possible_to_reach_right(int switch_number, int dest_ip);
    bool possible_to_reach_left(int switch_number, int dest_ip);

    public:
    void start(int num_of_switches, int port_number);
    void list_info();
    Controller();
};

#endif