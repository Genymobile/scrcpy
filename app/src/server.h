#include "command.h"

process_t push_server(const char *serial);
process_t enable_tunnel(const char *serial, Uint16 local_port);
process_t disable_tunnel(const char *serial);

process_t start_server(const char *serial);
void stop_server(process_t server);
