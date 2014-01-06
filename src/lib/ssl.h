#ifndef _SSL_H_
#define _SSL_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

#include "typedefs.h"

bool ssl_init();

void*  make_socket_secure(int sk);
uint32 secure_socket_read(int sk, char* buf,
                          uint32 size, void *priv);

uint32 secure_socket_write(int sk, char* buf,
                           uint32 size, void *priv);

// #ifdef __cplusplus
// }
// #endif

#endif /* _SSL_H_ */
