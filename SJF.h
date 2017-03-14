#ifndef _SJF_H_
#define _SJF_H_

#include "agios_request.h"

void SJF_postprocess(struct request_t *req);
void SJF(void *clnt);
#endif
