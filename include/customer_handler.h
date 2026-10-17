#ifndef CUSTOMER_HANDLER_H
#define CUSTOMER_HANDLER_H

/* Handles one command line from a logged-in customer.
   id comes from the server's session, never from the client.
   Returns 1 if the command was a customer command (reply already sent),
   0 if it is not a customer command. */
int handle_customer(int fd, int id, const char* line);

#endif
