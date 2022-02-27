#!/usr/bin/python3
"""
Class: CS 372 - Intro To Networking
Program: Project 2 FTP with sockets - ftclient.py
Author: KC
Date: August 10 2018
Description: Establish FTP protocol with server through control socket. Send
file or directory through a data socket.
Sources Used: CS 372 - Lectures on python client and server socket code
"""
import socket
import sys
import os.path

MAX_SIZE = 1000


def main():
    """FTP client.

    Description: Initiate contact with the server, make a request on the
    control socket and receive data on the data socket.
    """
    # Validate command line input from user
    val_input()

    # Initiate contact with server through the control socket
    control_socket = initiate_contact()
    if control_socket:
        # Send information for request on control socket
        valid_command = make_request(control_socket)

        # If command was valid set up data socket and receive file or directory
        if valid_command == 0:
            data_socket = set_up_data_socket()
            receive_file(control_socket, data_socket)

            # Close data socket after receiving data
            data_socket.close()

    control_socket.close()


def val_input():
    """Validate Input.

    Description: Ensure user input contains 5 inputs: server hostname, server
    control port number, client data port, command, file_name
    """
    min_inputs = 5
    max_inputs = 6
    if len(sys.argv) < min_inputs or len(sys.argv) > max_inputs:
        text1 = "USAGE: hostname, command port, data port, command, "
        text2 = "[if command=-g: file_name]"
        full_text = text1 + text2
        print(full_text)
        sys.exit(1)


def initiate_contact():
    """Initiate Contact.

    Description: Send and receive "hello" from the server on the port
    number specified in the second command line argument
    """
    # Build socket and connect to server
    control_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_host = sys.argv[1]
    server_port = int(sys.argv[2])
    print('server_host: {} server_port: {}'.format(server_host, server_port))
    control_socket.connect((server_host, server_port))

    # Send message to server
    str_msg = 'hello'
    byte_msg = str_msg.encode('ASCII')
    control_socket.send(byte_msg)

    # Receive message from server
    recv_byte_msg = control_socket.recv(2048)
    recv_str_msg = recv_byte_msg.decode('ASCII')
    print('Control socket to server established.')

    return control_socket


def make_request(control_socket):
    """Make request.

    Description: Send command, data port, and possible file name over control
    socket
    """
    # Send "l" for list or "g" for get file to control_socket
    command = sys.argv[4]
    if command == "-l":
        msg = "l"
    elif command == "-g":
        msg = "g"
    else:
        msg = "x"

    # Send command msg to data socket
    byte_msg = msg.encode('ASCII')
    control_socket.send(byte_msg)
    recv_byte_msg = control_socket.recv(2048)
    recv_str_msg = recv_byte_msg.decode('ASCII')

    # If command comes back as invalid print error message
    if recv_str_msg == "x":
        print('Error, command invalid.')
        return -1

    # Send data port to data socket
    str_msg = sys.argv[3]
    byte_msg = str_msg.encode('ASCII')
    control_socket.send(byte_msg)
    recv_byte_msg = control_socket.recv(2048)

    # If getting a file, send file_name
    if command == "-g":
        str_msg = sys.argv[5]
        print("file_name: {}".format(str_msg))
        byte_msg = str_msg.encode('ASCII')
        control_socket.send(byte_msg)
    return 0


def set_up_data_socket():
    """Set Up Data Socket.

    Description: Build and return the data socket
    """
    # Set up data socket on data_port to receive file/directory
    incoming_ip = '0.0.0.0'  # Allows server to bind to any incoming IP
    data_port = int(sys.argv[3])
    data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    data_socket.bind((incoming_ip, data_port))
    data_socket.listen(1)
    return data_socket


def receive_file(control_socket, data_socket):
    """Receive File.

    Description: Receive the file or the directory contents on the data socket.
    Receive any other control/error message on the control socket.
    """
    command = sys.argv[4]
    # Open the data socket
    cxn_socket, addr = data_socket.accept()
    print("Data socket to server established.")
    status = 0

    # If using get command, confirm file_name or error on control socket
    if command == "-g":
        databyte_msg = control_socket.recv(MAX_SIZE)
        datastr_msg = databyte_msg.decode('ASCII')

        # Server confirms the file_name is ok
        if datastr_msg == "ok":
            # If file_name already exists locally, append '_copy' to filename
            file_name = sys.argv[5]
            if os.path.exists(file_name):
                file_name = file_name + "_copy"

            # Open file and receive data into it until reach termination signal
            # file_handle = open(file_name, "w+")

            with open(file_name, "w+") as file_handle:

                # Make control socket nonblocking while sending file
                control_socket.setblocking(0)

                while status == 0:
                    databyte_msg = cxn_socket.recv(MAX_SIZE)
                    datastr_msg = databyte_msg.decode('ASCII')
                    file_handle.write(datastr_msg)

                    # Check control socket for termination msg
                    try:
                        databyte_msg_2 = control_socket.recv(MAX_SIZE)
                        datastr_msg_2 = databyte_msg_2.decode('ASCII')
                        if '#$!*' in datastr_msg_2:
                            status = 1
                    # BlockingIOError occurs when nothing read from socket
                    except BlockingIOError:
                        continue

            # Make control socket blocking again
            control_socket.setblocking(1)
            print("Transfer complete")

        else:
            print("File name not found")

    # For list command, read from data socket until the termination signal
    if command == "-l":
        print("\nDirectory contents:")

        # Make control socket nonblocking while sending file
        control_socket.setblocking(0)

        while status == 0:
            databyte_msg = cxn_socket.recv(MAX_SIZE)
            datastr_msg = databyte_msg.decode('ASCII')
            print(datastr_msg, end="")

            # Check control socket for termination msg
            try:
                databyte_msg_2 = control_socket.recv(MAX_SIZE)
                datastr_msg_2 = databyte_msg_2.decode('ASCII')
                if '#$!*' in datastr_msg_2:
                    status = 1
            # BlockingIOError occurs when nothing read from socket
            except BlockingIOError:
                continue

        # Make control socket blocking again
        control_socket.setblocking(1)
        print("\nTransfer complete")

    cxn_socket.close()


if __name__ == "__main__":
    main()
