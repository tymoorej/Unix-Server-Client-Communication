// the controller file has all of the methods and data needed for the controller

#include "controller.h"
#include "shared.h"

// the controller constructor
Controller::Controller(){
}

// The controller initialization function, setup the controller, run the main loop
// when done shutdown the controller.
void Controller::start(int num_of_switches, int port_number){
    this->num_of_switches = num_of_switches;
    this->port_number = port_number;
    struct pollfd fdarray[this->num_of_switches + 2]; // master socket, keyboard, and data sockets
    this->setup(fdarray);
    this->main(fdarray);
    this->shutdown();
}

// Setup the controller, do this by setting the signal handler,
// setting up the master socket,
// and then creating the poll array
void Controller::setup(struct pollfd fdarray[]){
    this->newAct.sa_handler = controller_handle_signal_USR1;
    sigaction(SIGUSR1, &this->newAct, &this->oldAct);

    cout << "Starting Controller. Controller pid: " << getpid() << endl;
    this->setup_socket();
    this->initialize_poll_array(fdarray);

    // Initialize switch vector
    for (int i = 1; i<= num_of_switches; i++){
        struct controller_known_switch_data switch_data = controller_known_switch_data(i);
        switches.push_back(switch_data);
    }
}

// The main controller loop. Polls all file descriptors to detect any incomming
// messages and handles them accordingly
void Controller::main(struct pollfd fdarray[]){
    int rval;
    string line;
    struct sockaddr_in client;
    int N = 2; // Initially we are just polling master socket and the keyboard
    while (true){
        rval = poll(fdarray, N, 10);
        if (rval > 0){
            if (fdarray[0].revents & POLLIN){ // a new client is connecting

                if (N >= this-> num_of_switches + 2){
                    cout << "Error: Too many clients attempting to connect to controller.\nController Shutting Down." << endl;
                    exit(0);
                }

                int client_size = sizeof(client);
                int newfd = accept(this->socket_fd, (struct sockaddr *) &client, (socklen_t*) &client_size);

                if (newfd < 0){
                    perror("Failed Accepting New Connection");
                    exit(1);
                }

                fdarray[N].fd = newfd;
                fdarray[N].events = POLLIN;
                fdarray[N].revents = 0;
                N++;
            }
            for (int i=1; i < N; i++){
                if (fdarray[i].revents & POLLIN){
                    // stdin (keyboard)
                    if (i == 1){
                        cin >> line;
                        cout << "Recieved " << line << " From Keyboard" << endl;
                        if (line == "exit"){
                            this->exit_program();
                        }
                        if (line == "list"){
                            this->list_info();
                        }
                    }
                    // switch
                    else{
                        struct message m;
                        memset( (char *) &m, 0, sizeof(m) );
                        int read_val = read(fdarray[i].fd, (char*)&m, sizeof(m));

                        if(read_val < 0){
                            perror("Read Failed");
                            exit(1);
                        }
                        else if(read_val==0){
                            int switch_number = get_switch_number_from_fd(fdarray[i].fd);
                            printf("Lost connection to sw%d\n", switch_number);
                            close(fdarray[i].fd);
                            fdarray[i].events = 0;
                            struct controller_known_switch_data data = get_switch_data(switch_number);
                            data.active = false;
                            data.fd = -1;
                            set_switch_data(switch_number, data);
                        }
                        else{
                            this->recieve_message(m, fdarray[i].fd);
                        }
                    }
                }
            }
        }
    }
}

// Shutdown the controller, restores signal behaviour
void Controller::shutdown(){
    cout << "Controller Shutting Down" << endl;
    sigaction(SIGUSR1,&this->oldAct,&this->newAct);
    close(this->socket_fd);
    for (int i = 0; i < num_of_switches; i++){
        close(i+3);
    }
}

// Exit the controller, first call list to do all needed printing
// then shutdown the controller and exit the process
void Controller::exit_program(){
    this->list_info();
    this->shutdown();
    exit(0);
}

// Print switch data and all signals revcieved by or transmitted by the controller
void Controller::list_info(){
    this->print_switch_info();
    this->signals.print();
}


// This function relates to setting up the master socket,
// it first creates the socket, then it binds the socket,
// to make it a managerial socket, then it sets how many
// clients to listen for
void Controller::setup_socket(){
    this->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->socket_fd <= 0){
        perror("Creation of socket failed"); 
        exit(1); 
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY); 
    serv.sin_port = htons(this->port_number);

    int options = 1; 
    if(setsockopt(this->socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options))){
        perror("Failed Configuring Socket");
        exit(1);
    }

    if(bind(this->socket_fd, (struct sockaddr *) &serv, sizeof(serv)) < 0){
        perror("Failed binding socket"); 
        exit(1);
    }

    if(listen(this->socket_fd, this->num_of_switches) < 0){
        perror("Listen Failed");
        exit(1);
    }
}


// This function relates to initializing the poll array
// which we will use later to poll over file descriptors
void Controller::initialize_poll_array(struct pollfd fdarray[]){
    // Master Socket
    fdarray[0].fd = this->socket_fd;
    fdarray[0].events = POLLIN;
    fdarray[0].revents = 0;

    // Keyboard
    fdarray[1].fd = STDIN_FILENO;
    fdarray[1].events = POLLIN;
    fdarray[1].revents = 0;
}


// Send a message to a switch from the controller, sends the message
// through a socket
void Controller::send_message(struct message m, int target){
    printf("Transmitted (src= cont, dest= sw%d)", target);
    m.print();
    struct controller_known_switch_data sw_data = get_switch_data(target);

    write(sw_data.fd, (char*)&m, sizeof(m));
}


// Construct and send the ACK message also incriment the 
// ack transmitted signal counter
void Controller::send_ack_message(int switch_number){
    struct message m;
    struct Ack_Data data;
    memset((char *) &m, 0, sizeof(m));

    m.type = ACK;
    m.data.ack_data = data;
    this->send_message(m, switch_number);
    this->signals.transmitted.ack++;
}

// Given a rule and a switch number, sends the correct rule to the switch
// which sent the query
void Controller::send_add_message(struct Query_Data rule, int switch_number){
    if (this->ip_not_known(rule.dest_ip)){
        this->send_add_drop_message(rule.dest_ip, rule.dest_ip, switch_number);
        return;
    }
    this->path_to_target(switch_number, rule.dest_ip);
}

// Constructs and sends an add-drop message from the controller to a specific switch
void Controller::send_add_drop_message(int dest_ip_lo, int dest_ip_hi, int switch_number){
    struct message m;
    struct Add_Data data;
    struct flow_element element;
    memset((char *) &m, 0, sizeof(m));


    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = dest_ip_lo;
    element.destIP_hi = dest_ip_hi;
    element.actionType = DROP;
    element.actionVal = 0;
    element.pri = 4;
    element.pktCount = 0;

    m.type = ADD;
    data.rule = element;
    m.data.add_data = data;
    this->send_message(m, switch_number);
    this->signals.transmitted.add++;
}

// Constructs and sends an add-forward message from the controller to a specific switch
void Controller::send_add_forward_message(int dest_ip_lo, int dest_ip_hi, int switch_number, int port){
    struct message m;
    struct Add_Data data;
    struct flow_element element;
    memset((char *) &m, 0, sizeof(m));


    element.scrIP_lo = 0;
    element.scrIP_hi = MAXIP;
    element.destIP_lo = dest_ip_lo;
    element.destIP_hi = dest_ip_hi;
    element.actionType = FORWARD;
    element.actionVal = port;
    element.pri = 4;
    element.pktCount = 0;

    m.type = ADD;
    data.rule = element;
    m.data.add_data = data;
    this->send_message(m, switch_number);
    this->signals.transmitted.add++;
}

// Set all the known data of a switch after receiving an open signal
void Controller::set_open_data(struct Open_Data data, int fd){
    int target = data.switch_number;
    for (int i=0; i < this->switches.size(); i++){
        struct controller_known_switch_data element = switches[i];
        if(element.switch_number == target){
            element.left = data.left;
            element.right = data.right;
            element.ip_lo = data.ip_range_lo;
            element.ip_hi = data.ip_range_hi;
            element.active = true;
            element.fd = fd;
            switches[i] = element;
        }
    }
}



// Handles the messages a controller can receive from a switch
void Controller::recieve_message(message m, int fd){
    int switch_number;
    if (m.type == OPEN){
        this->signals.recieved.open++;
        this->set_open_data(m.data.open_data, fd);
        switch_number = m.data.open_data.switch_number;
        printf("Received (src= sw%d, dest=cont)", switch_number);
        m.print();
        this->send_ack_message(switch_number);
    }
    if (m.type == QUERY){
        this->signals.recieved.query++;

        switch_number = get_switch_number_from_fd(fd);
        if (switch_number == -1){
            cout << "Error: QUERY message arrived before open" << endl;
            exit(0);
        }
        printf("Received (src= sw%d, dest=cont)", switch_number);
        m.print();
        this->send_add_message(m.data.query_data, switch_number);
    }
}

// gets which switch number a file descriptor belongs to
int Controller::get_switch_number_from_fd(int fd){
    for (int i=0; i < switches.size(); i++){
        if (switches[i].fd == fd){
            return switches[i].switch_number;
        }
    }
    return -1;
}

// for a specific switch, get all of its known data
struct controller_known_switch_data Controller::get_switch_data(int switch_number){
    
    for (int i=0; i < this->switches.size(); i++){
        if (switches[i].switch_number == switch_number){
            return switches[i];
        }
    }
}

// for a specific switch, set its known data
bool Controller::set_switch_data(int switch_number, struct controller_known_switch_data data){
    for (int i=0; i < this->switches.size(); i++){
        if (switches[i].switch_number == switch_number){
            switches[i] = data;
            return true;
        }
    }
    return false;
}

// Print all of the known data for each switch
void Controller::print_switch_info(){
    printf("Switch Information:\n");
    for (int i=0; i < this->switches.size(); i++){
        switches[i].print();
    }
}


// Given a destination ip, get the ip range it belongs to
void Controller::dest_ip_to_ip_range(int dest_ip, int dest_ip_range[]){
    for (vector<struct controller_known_switch_data>::iterator it = this->switches.begin(); it != this->switches.end(); ++it){
        if (dest_ip >= (*it).ip_lo && dest_ip <= (*it).ip_hi){
            dest_ip_range[0] = (*it).ip_lo;
            dest_ip_range[1] = (*it).ip_hi;
            return;
        }
    }
} 

// Determines if the dest ip is an ip address that the controller does not know about
bool Controller::ip_not_known(int dest_ip){
    for (vector<struct controller_known_switch_data>::iterator it = this->switches.begin(); it != this->switches.end(); ++it){
        if ((dest_ip >= (*it).ip_lo) && (dest_ip <= (*it).ip_hi)){
            return false;
        }
    }
    return true;
}


// Given a switch number and destination ip, this function either sends
// an add-forward message to the proper switch and specifies which port to use
// or sends an add-drop message if the destination ip is not reachable from the source
void Controller::path_to_target(int switch_number, int dest_ip){
    struct controller_known_switch_data current_switch = this->get_switch_data(switch_number);
    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        this->send_add_forward_message(current_switch.ip_lo, current_switch.ip_hi, switch_number, 3);
        return;
    }

    int dest_ip_range[2];
    this->dest_ip_to_ip_range(dest_ip, dest_ip_range);

    if (this->possible_to_reach_right(current_switch.right, dest_ip)){
        this->send_add_forward_message(dest_ip_range[0], dest_ip_range[1], switch_number, 2);
        return;
    }
    if (this->possible_to_reach_left(current_switch.left, dest_ip)){
        this->send_add_forward_message(dest_ip_range[0], dest_ip_range[1], switch_number, 1);
        return;
    }
    else{
        this->send_add_drop_message(dest_ip_range[0], dest_ip_range[1], switch_number);
        return;
    }
    
}

// Recursive function which determines if a switch is able to relay a message to the right
bool Controller::possible_to_reach_right(int switch_number, int dest_ip){
    if (switch_number == -1){
        return false;
    }
    struct controller_known_switch_data current_switch = this->get_switch_data(switch_number);

    if (!current_switch.active){
        return false;
    }

    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        return true;
    }
    if (current_switch.right == -1){
        return false;
    }
    return this->possible_to_reach_right(current_switch.right, dest_ip);
}

// Recursive function which determines if a switch is able to relay a message to the left
bool Controller::possible_to_reach_left(int switch_number, int dest_ip){
    if (switch_number == -1){
        return false;
    }
    struct controller_known_switch_data current_switch = this->get_switch_data(switch_number);

    if (!current_switch.active){
        return false;
    }

    if (dest_ip >= current_switch.ip_lo && dest_ip <= current_switch.ip_hi){
        return true;
    }
    if (current_switch.left == -1){
        return false;
    }
    return this->possible_to_reach_left(current_switch.left, dest_ip);
}


/* This group of functions relate to controller signal counts
*/
controller_signal_count::controller_signal_count(){
    this->open=0;
    this->ack=0;
    this->query=0;
    this->add=0;
}

void controller_signal_count::print_receiving(){
    cout << "OPEN: " << this->open << ",   ";
    cout << "QUERY: " << this->query << endl;
}

void controller_signal_count::print_transmitting(){
    cout << "ACK: " << this->ack << ",   ";
    cout << "ADD: " << this->add << endl;
}



controller_known_switch_data::controller_known_switch_data(int switch_number){
    this->switch_number = switch_number;
    this->active = false;
    this->fd = -1;
}

void controller_total_signals::print(){
    cout << "Packet Stats:" << endl;
    cout << "\tRecieved: ";
    this->recieved.print_receiving();
    cout << "\tTransmitted: ";
    this->transmitted.print_transmitting();
}

void controller_known_switch_data::print(){
    if (this->fd!=-1){
        printf("[sw%d] port1= %d, port2= %d, port3= %d-%d\n", \
        this->switch_number, this->left, this->right, this->ip_lo, this->ip_hi);
    }
}