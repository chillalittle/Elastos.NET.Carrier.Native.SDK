/*
 * Copyright (c) 2018 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <ela_carrier.h>
#include "ela_filetransfer.h"

int ela_file_transfer_init(ElaCarrier *carrier,
                           ElaFileTransferRequestCallback *callback,
                           void *context)
{
    //TODO;
    return -1;
}

void ela_file_transfer_cleanup(ElaCarrier *carrier)
{
    //TODO;
}

ElaFileTransfer *ela_file_transfer_open(ElaCarrier *carrier,
                           const char *address,
                           ElaFileTransferInfo *fileinfo_list,
                           size_t count)
{
    //TODO;
    return NULL;
}

int ela_file_transfer_close(ElaFileTransfer *file_transfer)
{
    //TODO;
    return -1;
}

char *ela_file_transfer_get_fileid(ElaFileTransfer *file_transfer,
                           const char *filename,
                           char *fileid_buf,
                           size_t length)
{
    //TODO;
    return NULL;
}

int ela_file_transfer_request(ElaFileTransfer *file_transfer,
                           ElaFileTransferRequestCompleteCallbacks *callbacks,
                           void *context)
{
    //TODO;
    return -1;
}

/* Reply the file transfer request */
int ela_file_transfer_response(ElaFileTransfer *file_transfer,
                           int status,
                           const char *reason)
{
    //TODO;
    return -1;
}

int ela_file_transfer_pull(ElaFileTransfer *file_transfer,
                           const char *fileid,
                           uint64_t offset,
                           size_t length,
                           ElaFileTransferPullCallbacks *callbacks,
                           void *context)
{
    //TODO;
    return -1;
}

/* to send a chunk of file to respone to pull request */
int ela_file_transfer_send(ElaFileTransfer  *file_transfer,
                           const char *fileid,
                           uint64_t offset,
                           const uint8_t *data,
                           size_t length)
{
    //TODO;
    return -1;
}

/* pause transfering */
int ela_file_transfer_pause(ElaFileTransfer *file_transfer,
                           const char *fileid)
{
    //TODO;
    return -1;
}

/* resume transfering */
int ela_file_transfer_resume(ElaFileTransfer *file_transfer,
                           const char *fileid)
{
    //TODO;
    return -1;
}
