/*
 * Copyright (C) 2022-2025 - 2024 IoT.bzh Company
 *
 * Author: Valentin Lefebvre <valentin.lefebvre@iot.bzh
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/////////////////////////////////////////////////////////////////////////////
//                          DEFINE                                        //
/////////////////////////////////////////////////////////////////////////////

// -- Macro for TCP connection
#define TCP_ADDRESS_DEFAULt         "127.0.0.1"
#define TCP_PORT_DEFAULT            2000
#define TCP_MAX_CONN                5
#define TCP_PERIOD_MS               500

// -- Macro for data
#define NUMBER_DATA                 10
#define MAX_EVENTS                  5
#define BUFFER_DATA_LENGTH          24
#define RESPONSE_DATA_LENGTH        128

// -- Useful macro
#define SLEEP_MS_TO_US              1000

// -- Getter modbus command buffer
#define GET_TRANS_ID_H(x)           (x[0])
#define GET_TRANS_ID_L(x)           (x[1])
#define GET_MODBUS_PROTOCOL_H(x)    (x[2])
#define GET_MODBUS_PROTOCOL_L(x)    (x[3])
#define GET_DATA_LENGTH_H(x)        (x[4])
#define GET_DATA_LENGTH_L(x)        (x[5])
#define GET_SLAVE_ID(x)             (x[6])
#define GET_COMMAND(x)              (x[7])
#define GET_REGISTER_H(x)           (x[8])
#define GET_REGISTER_L(x)           (x[9])
#define GET_COUNT_H(x)              (x[10])
#define GET_COUNT_L(x)              (x[11])

// -- Command ID
#define MODBUS_CMD_READ             0x03

/////////////////////////////////////////////////////////////////////////////
//                          INCLUDE                                        //
/////////////////////////////////////////////////////////////////////////////

// -- Standard includes
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

// -- Network includes
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>

// --Thread includes
#include <pthread.h>

/////////////////////////////////////////////////////////////////////////////
//                          ENUMERATIONS                                   //
/////////////////////////////////////////////////////////////////////////////

enum {
    CONNECTION_NONE = 0,        // Connection not established yet
    CONNECTION_ESTABLISHED      // Connection in progress
};

/////////////////////////////////////////////////////////////////////////////
//                          STRUCTURES                                     //
/////////////////////////////////////////////////////////////////////////////

/**
 * @brief Hold information about connection device
 *
 * @param idx       Index of the connection - the n°
 * @param state     Connection state, could be  established or not
 * @param thread_id ID of the connection thread
 * @param conn_fd   File descriptor of the connection
 * @param epoll_fd  File descriptor to catch epoll event
 */
typedef struct {
    int idx;
    int state;
    pthread_t thread_id;
    int conn_fd;
    int epoll_fd;
}connection_t;

/////////////////////////////////////////////////////////////////////////////
//                          GLOBAL VARIABLES                               //
/////////////////////////////////////////////////////////////////////////////

/**
 * @brief Store an array of connection data
 *
 */
connection_t _global_connection_data_arr[TCP_MAX_CONN] = {0};

/**
 * @brief Store the global tcp socket fd
 *
 */
int _global_sockfd = 0;

/////////////////////////////////////////////////////////////////////////////
//                          EXTERNAL VARIABLES                             //
/////////////////////////////////////////////////////////////////////////////

extern uint16_t modbus_simu_data_static[115];

/////////////////////////////////////////////////////////////////////////////
//                          UTILS FUNCTIONS                                //
/////////////////////////////////////////////////////////////////////////////

/**
 * @brief print the error message
 *
 * @param msg char *
 */
static void error(char *msg) {
    perror(msg);
    exit(0);
}

/**
 * @brief Signal handler to close correctly the socket
 *
 * @param signum Id of the signal caught
 */
static void _signal_handler(const int signum) {
    switch (signum)
    {
    case SIGINT:
        printf("/!\\ SIGINT caught\n");
        break;

    case SIGSEGV:
        printf("/!\\ SIGSEGV caught\n");
        break;

    default:
        printf("/!\\ UNKNOWN signal caught\n");
        break;
    }
    for (int idx = 0; idx < TCP_MAX_CONN; idx ++) {
        if (_global_connection_data_arr[idx].state == CONNECTION_ESTABLISHED) {
            connection_t *connection = &_global_connection_data_arr[idx];
            epoll_ctl(connection->epoll_fd, EPOLL_CTL_DEL, connection->conn_fd, NULL);
            close(connection->epoll_fd);
            close(connection->conn_fd);
        }
    }
    close(_global_sockfd);
    exit(signum);
}

/////////////////////////////////////////////////////////////////////////////
//                          PRIVATES FUNCTIONS                             //
/////////////////////////////////////////////////////////////////////////////

/**
 * @brief Calculate the modbus rtu CRC16
 *
 * @param buf   buffer to calculate the CRC
 * @param len   buffer length
 * @return      CRC16 calculated
 */
uint16_t modbus_rtu_crc(uint8_t* buf, int len) {
    uint16_t crc = 0xFFFF;

    for (int pos = 0; pos < len; pos++) {
        crc = (uint16_t) (crc ^ (uint16_t)buf[pos]); // XOR byte into least sig. byte of crc

        for (int i = 8; i != 0; i--) {      // Loop over each bit
            if ((crc & 0x0001) != 0) {      // If the LSB is set
                crc >>= 1;                  // Shift right and XOR 0xA001
                crc ^= 0xA001;
            } else {                        // Else LSB is not set
                crc >>= 1;                  // Just shift right
            }
        }
    }
    // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
    return crc;
}

/**
 * @brief Copy the header of the modbus command to the response
 *
 * @param response_buff     Response buffer where header will be copy
 * @param command_buff      Command buffer where header comes from
 * @return                  0 in success negative otherwise
 */
static int _copy_mbap_header(uint8_t *response_buff, uint8_t *command_buff) {
    (response_buff)[0] = GET_TRANS_ID_H(command_buff);
    (response_buff)[1] = GET_TRANS_ID_L(command_buff);

    // Copy protocol identifier (0x00 0x00 for modbus)
    response_buff[2] = GET_MODBUS_PROTOCOL_H(command_buff);
    response_buff[3] = GET_MODBUS_PROTOCOL_L(command_buff);

    // Determine response length
    uint16_t count = (uint16_t) (GET_COUNT_H(command_buff) << 8 | GET_COUNT_L(command_buff));
    uint16_t data_length = (uint16_t) (1 + 2 * count + 1 + 1)/* + 2*/; // response will hold SlaveID + Function ID + count data in 16 bites (2 x 8 bites) + CRC
    response_buff[4] = (uint8_t) (data_length >> 8);
    response_buff[5] = (uint8_t) data_length;

    // Copy the n° of the slave ID
    response_buff[6] = GET_SLAVE_ID(command_buff);

    // Copy ID function
    response_buff[7] = GET_COMMAND(command_buff);

    return 0;
}

/**
 * @brief Send the correct response following the modbus command received
 *
 * @param connection    Connection struct hold
 * @param command_buff  Command buffer received
 * @param command_len   Length of the command buffer
 * @return 0 in success negative otherwise
 */
static int _response_modbus(connection_t *connection, uint8_t *command_buff, ssize_t command_len) {
    uint16_t data_length = (uint16_t) (GET_DATA_LENGTH_H(command_buff) << 8 | GET_DATA_LENGTH_L(command_buff));

    // Check if we received the entire command data to avoid segfault
    if ((6+data_length) != command_len) {
        fprintf(stderr, "something wrong with the buffer length: %ld, command length: %u\n", command_len, data_length);
        return -1;
    }

    uint8_t response_buffer[RESPONSE_DATA_LENGTH] = {0};
    ssize_t response_length = 0;

    // Create the fake response
    if (GET_COMMAND(command_buff) == MODBUS_CMD_READ) {
        uint16_t count = (uint16_t) (GET_COUNT_H(command_buff) << 8 | GET_COUNT_L(command_buff));
        uint16_t register_start = (uint16_t) (GET_REGISTER_H(command_buff) << 8 | GET_REGISTER_L(command_buff));

        response_buffer[8] = (uint8_t) (2*count); // Byte count: It will have count*uint16_t, so count*2*bytes

        // Set data following count of register( 1 register = INT16)
        for (uint16_t idx = 0; idx < count; idx ++) {
            response_buffer[9+(2*idx)] = (uint8_t) ((modbus_simu_data_static[idx + register_start - 1] >> 8)&0xff);
            response_buffer[9+(2*idx+1)] = (uint8_t) (modbus_simu_data_static[idx + register_start - 1] & 0xff) + (uint8_t) rand() % 3;
        }

        // Copy the header
        if (_copy_mbap_header(response_buffer, command_buff) < 0) {
            fprintf(stderr, "Failed to copy the command header\n");
            return -1;
        }
        response_length = 2*count + 1 + 2 + 6;

        //Calculate CRC
        /*
        uint16_t crc = modbus_rtu_crc(response_buffer, response_length);
        response_buffer[response_length+1] = (uint8_t) crc;
        response_buffer[response_length] = (uint8_t) (crc >> 8);
        response_length+=2;
        */

        // Write data on the socket
        fprintf(stdout, "connection n° %i - Send ", connection->idx);
        for (int index = 0; index < response_length; index++) {
            fprintf(stdout, "0x%02x ", response_buffer[index]);
        }
        fprintf(stdout, "\n");
        ssize_t size = write(connection->conn_fd, response_buffer, response_length);
        if (size <= 0) {
            fprintf(stderr, "connection n°%i - Failed to send response buffer", connection->idx);
        }
    } else {
        fprintf(stderr, "Unrecognized command !! (0x%02x)\n", GET_COMMAND(command_buff));
        return -1;
    }

    return 0;
}


/**
 * @brief Take care about the device connection.
 *  Then wait at epoll event
 *
 * @param   ctx connection data structure
 * @return  void*
 */
static void *_monitor_connection_entry(void *ctx) {
    connection_t *connection = (connection_t *)ctx;
    int event_count = 0;
    struct epoll_event events[MAX_EVENTS];
    ssize_t size = 0;
    uint8_t buffer[BUFFER_DATA_LENGTH];

    // Create the epoll
    connection->epoll_fd = epoll_create1(0);
    if (connection->epoll_fd < 0) goto OnError;

    // Config epoll event
    struct epoll_event epoll_evt = {0};
    epoll_evt.events = EPOLLIN|EPOLLRDHUP|EPOLLWAKEUP|EPOLLET|EPOLLERR;
    epoll_evt.data.fd = connection->conn_fd;
    if (epoll_ctl(connection->epoll_fd, EPOLL_CTL_ADD, connection->conn_fd, &epoll_evt) < 0) {
        close(connection->epoll_fd);
        goto OnError;
    }

    // Wait for each event of epoll
    while(1) {
        event_count = epoll_wait(connection->epoll_fd, events, MAX_EVENTS, -1);
        for (int idx = 0; idx < event_count; idx++) {
            if (events[idx].events & EPOLLRDHUP) {
                fprintf(stdout, "connection n° %i - EPOLLRDHUP \n", connection->idx);
                epoll_ctl(connection->epoll_fd, EPOLL_CTL_DEL, connection->conn_fd, NULL);
                close(connection->epoll_fd);
                goto OnError;
            }
            if (events[idx].events & EPOLLWAKEUP) {
                fprintf(stdout, "connection n° %i - EPOLLWAKEUP \n", connection->idx);
            }
            if (events[idx].events & EPOLLERR) {
                fprintf(stdout, "connection n° %i - EPOLLERR \n", connection->idx);
            }
            if (events[idx].events & EPOLLET) {
                fprintf(stdout, "connection n° %i - EPOLLET \n", connection->idx);
            }
            if (events[idx].events & EPOLLOUT) {
                // fprintf(stdout, "EPOLLOUT \n");
            }
            if (events[idx].events & EPOLLIN) {
                size = recv(connection->conn_fd, buffer, sizeof(buffer), 0);
                if (size < 0) {
                    fprintf(stderr, "connection n° %i - Error in Received! (errno: %s)\n"
                                , connection->idx, strerror(errno));
                } else if (size == 0) {
                    fprintf(stderr, "connection n° %i - Received nothing!\n", connection->idx);
                } else {
                    fprintf(stdout, "connection n° %i - Receive ", connection->idx);
                    for (int index = 0; index < size; index++) {
                        fprintf(stdout, "0x%02X ", buffer[index]);
                    }
                    fprintf(stdout, "(size :%ld)\n", size);

                    if (_response_modbus(connection, buffer, size) < 0) {
                        fprintf(stderr, "Failed to create response for the connection n°%i\n", connection->idx);
                    }
                }
                memset(&buffer, '\0', sizeof(buffer));
            }
        }
    }

OnError:
    close(connection->conn_fd);
    connection->state = CONNECTION_NONE;
    pthread_exit(NULL);
}

/**
 * @brief Print to th stdout, the usage of the generated executable
 *
 */
static void _print_usage() {
    fprintf(stdout,
"This is the usage for the modbus-simulation binary:\n\
    -a : TCP address of the emulated modbus device\n\
    -p : TCP port of the emulated modbus device\n\
    -h : Helper (print this)\n\
    \n\
example: modbus-simulation -a 127.0.0.1 -p 2000\n"
    );
    exit(0);
}

/**
 * @brief Parsed option arguments passed to the executable
 *
 * @param argc          Count og the arguments
 * @param argv          List of arguments
 * @param tcp_address   Address tcp caught from option args
 * @param tcp_port      Port tcp caught from option args
 * @return              exit the program if args aren't suitable
 */
static void _parsed_arguments(int argc, char **argv, char **tcp_address, uint16_t *tcp_port) {
    char *opt_address = NULL;
    char *opt_port = NULL;
    int option = 0;

    while((option = getopt(argc, argv, "a:p:h")) != -1) {
        switch (option) {
        case 'a':
            opt_address = optarg;
            break;
        case 'p':
            opt_port = optarg;
            break;
        case 'h':
            _print_usage();
            break;

        default:
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            _print_usage();
            break;
        }
    }

    // Check if address and port are given and parsed them
    if (opt_address == NULL) {
        fprintf(stderr, "ERROR - Missing address tcp in args\n");
        _print_usage();
    } else if (opt_port == NULL) {
        fprintf(stderr, "ERROR - Missing port tcp in args\n");
        _print_usage();
    } else {
        asprintf(tcp_address, "%s", opt_address);
        *tcp_port = (uint16_t ) atoi(opt_port);
    }
}

/////////////////////////////////////////////////////////////////////////////
//                          MAIN FUNCTION                                  //
/////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a TCP server and emulate a modbus device through protocol
 *  modbus TCP
 *
 * ==========================================================================
 * Command:
 * --------
 *  MBAP Header:
 *      Byte[0:1]   -> Transaction Identifier (2 octets)
 *      Byte[2:3]   -> 0 = MODBUS protocol (2 octet)
 *      Bytes[4:5]  -> length command data (n octets) (Take care Slave Id, function code and data)
 *      Bytes[6]    -> Slave identifier (1 octet)
 *  Function code:
 *      Byte[7]     -> Code of the function (1 octet) (read = 0x03, write = ?)
 *  Data:
 *      Byte[8:9]   -> Register
 *      Byte[10:11] -> Count
 *
 * Response:
 * ---------
 *  MBAP Header:
 *      Byte[0:1]   -> Transaction Identifier (2 octets) (same as command)
 *      Byte[2:3]   -> 0 = MODBUS protocol (2 octet)
 *      Bytes[4:5]  -> length response data (n octets) (Take care Slave Id, function code and data)
 *      Bytes[6]    -> Slave identifier (1 octet) (same as command)
 *  Function code:
 *      Byte[7]     -> Code of the function (1 octet) (same as command)
 *  Data:
 *      Byte[8]     -> Byte count (1 octet)
 *      Byte[n]     -> Data coils
 *  ==========================================================================
 *
 * @param argc    the count of parameter
 * @param argv    the list of parameter data
 */
int main(int argc, char **argv) {
    char *tcp_address = NULL;
    uint16_t tcp_port = 0;

    // Parsed arguments
    _parsed_arguments(argc, argv, &tcp_address, &tcp_port);
    fprintf(stdout, "\n\t--- Starting modbus tcp server at %s:%u ---\n\n", tcp_address, tcp_port);

    //int flags = 0;
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    struct sockaddr_in servaddr = {0};
    struct sockaddr_in client = {0};

    // Initiate seed for random value
    srand((uint) time(NULL));

    // Create socket
    _global_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_global_sockfd < 0)
        error("Failed to create socket !\n");

    // Init servaddr
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(tcp_address);
    servaddr.sin_port = htons(tcp_port);
    free(tcp_address);

    // Set the socket in no-blocking
    // flags = fcntl(_global_sockfd, F_GETFD);
    // if (fcntl(_global_sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
    //     close (_global_sockfd);
    //     error("Failed to set socket in no-blocking mode !\n");
    // }

    // Set Keepalive option
    if (setsockopt(_global_sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        close (_global_sockfd);
        error("Failed to set socket keepalive option !\n");
    }

    // Set Keepidle option
    optval = 10;
    if (setsockopt(_global_sockfd, SOL_TCP, TCP_KEEPIDLE, &optval, optlen) < 0) {
        close (_global_sockfd);
        error("Failed to set socket keepidle option !\n");
    }

    // Bind the socket
    if (bind(_global_sockfd, (const struct sockaddr *)(&servaddr), sizeof(servaddr)) < 0) {
        close(_global_sockfd);
        error("Failed to bind socket !\n");
    }

    // Listen to the socket
    if (listen(_global_sockfd, TCP_MAX_CONN) < 0) {
        error("Failed to listen to the created socket !\n");
        close(_global_sockfd);
    }

    // Create a signal handler
    signal(SIGSEGV, _signal_handler);
    signal(SIGINT, _signal_handler);

    while(1) {
        connection_t *connection = NULL;
        for (int idx = 0; idx < TCP_MAX_CONN; idx ++) {
            if (_global_connection_data_arr[idx].state == CONNECTION_NONE) {
                connection = &_global_connection_data_arr[idx];
                connection->idx = idx;
                break;
            }
        }
        if (!connection) {
            fprintf(stderr, "The Maximum of client allowed has been reached, client has to wait!");
            sleep(10);
            continue;
        }
        fprintf(stdout, "TCP server ready for incoming client ...\n");
        socklen_t clientlen = {0};

        connection->conn_fd = accept(_global_sockfd, (struct sockaddr *)&client, &clientlen);
        if (connection->conn_fd < 0)
            perror("Failed to accept client ...\n");

        connection->state = CONNECTION_ESTABLISHED;
        fprintf(stdout, "connection n°%i established\n", connection->idx);
        // Create thread to take care about the connection with the client
        if (pthread_create(&connection->thread_id, NULL, _monitor_connection_entry, connection) < 0) {
            fprintf(stderr, "Failed to cerate thread for client n° %i\n", connection->idx);
        }
    }
    return 0;
}
