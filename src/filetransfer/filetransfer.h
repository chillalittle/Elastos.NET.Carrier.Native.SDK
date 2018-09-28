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

#ifndef __FILETRANSFER_H__
#define __FILETRANSFER_H__

#include <pthread.h>

#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#endif

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

#include <stdint.h>
#include <ela_session.h>
#include "ela_filetransfer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FileTransferExtension FileTransferExtension;

struct ElaCarrier       {
    pthread_mutex_t         ext_mutex;
    void                    *session_extension;
    void                    *filetransfer_extension;
    uint8_t                 padding[1]; // the rest fields belong to Carrier self.
};

struct FileTransferExtension {
    ElaCarrier              *carrier;

    ElaFileTransferRequestCallback callback;
    void                    *context;

    ElaStreamCallbacks      stream_callbacks;
};

struct ElaFileTransfer {
    FileTransferExtension   *ext;

    char                    *to;
    ElaFileTransferInfo     *fileinfo_list;
    size_t                  count;

    ElaFileTransferState    state;

    ElaSession              *session;
    int                     streamid;

    bool                    is_sender;

    ElaFileTransferCallbacks    callbacks;
    void *                  context;

    ElaFileTransferRequestCompleteCallback request_complete_callback;
    void *                  request_complete_context;

    ElaStreamCallbacks      *stream_callbacks;
};

void ela_set_error(int error);

#ifdef __cplusplus
}
#endif

#endif /* __FILETRANSFER_H__ */
