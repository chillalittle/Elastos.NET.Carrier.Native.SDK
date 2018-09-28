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
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <rc_mem.h>
#include <vlog.h>

#include <ela_carrier.h>
#include <ela_session.h>

#include "ela_filetransfer.h"
#include "filetransfer.h"

#define ELA_MAX_TRANSFER_FILE_COUNT             10

static void filetransfer_extension_destroy(void *p)
{
    //TODO;
}

int ela_filetransfer_init(ElaCarrier *w,
                          ElaFileTransferRequestCallback *callback,
                          void *context)
{
    FileTransferExtension *ext;

    if (!w) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    pthread_mutex_lock(&w->ext_mutex);
    if (w->filetransfer_extension) {
        pthread_mutex_unlock(&w->ext_mutex);
        vlogD("FileTransfer: filetransfer initialized already, ref counter(%d).",
              nrefs(w->filetransfer_extension));
        return 0;
    }

    ext = (FileTransferExtension *)rc_zalloc(sizeof(FileTransferExtension),
                                             filetransfer_extension_destroy);
    if (!ext) {
        pthread_mutex_unlock(&w->ext_mutex);
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_OUT_OF_MEMORY));
        return -1;
    }

    ext->callback = *callback;
    ext->context = context;
    w->filetransfer_extension = ext;

    /*ft->stream_callbacks.stream_data = NULL;
    ft->stream_callbacks.state_changed = stream_state_changed;
    ft->stream_callbacks.channel_open = stream_channel_open;
    ft->stream_callbacks.channel_opened = stream_channel_opened;
    ft->stream_callbacks.channel_data = stream_channel_data;
    ft->stream_callbacks.channel_close = stream_channel_close;
    ft->stream_callbacks.channel_pending = stream_channel_pending;
    ft->stream_callbacks.channel_resume = stream_channel_resume;
    */

    pthread_mutex_unlock(&w->ext_mutex);

    vlogD("FileTransfer: Initialize filetransfer extension success.");

    return 0;
}

void ela_filetransfer_cleanup(ElaCarrier *w)
{
    if (!w)
        return;

    pthread_mutex_lock(&w->ext_mutex);

    if (!w->filetransfer_extension) {
        pthread_mutex_unlock(&w->ext_mutex);
        return;
    }

    vlogD("FileTransfer: filetransfer cleanup, ref counter(%d).",
           nrefs(w->filetransfer_extension));

    deref(w->filetransfer_extension);
    w->filetransfer_extension = NULL;

    pthread_mutex_unlock(&w->ext_mutex);
}


static void session_request_complete_callback(ElaSession *session,
                const char *bundle, int status, const char *reason,
                const char *sdp, size_t len, void *context)
{
    ElaFileTransfer *ft = (ElaFileTransfer *)context;
    int rc;

    //TODO: transport ready.

    rc = ela_session_start(session, sdp, len);
    if (rc < 0) {
        vlogW("FileTransfer: Start session negotiate error");
        ft->state = ElaFileTransferState_failed;
        return;
    }
}

static
void stream_state_changed(ElaSession *session, int stream,
                          ElaStreamState state, void *context)
{
    ElaFileTransfer *ft = (ElaFileTransfer *)context;
    const char *bundle = NULL;
    int rc;

    assert(session);
    assert(stream > 0);

    switch(state) {
    case ElaStreamState_initialized:
        rc = ela_session_request(session, bundle,
                        session_request_complete_callback, context);
        if (rc < 0) {
            vlogE("FileTransfer: Send filetransfer %s request error.", ft->to);
            ft->state = ElaFileTransferState_failed;
            return;
        }

        //TODO;
        break;

    case ElaStreamState_transport_ready:
        //TODO;
        break;

    case ElaStreamState_connecting:
        //Do nothing;
        break;

    case ElaStreamState_connected:


        //TODO;
        break;

    case ElaStreamState_deactivated:
    case ElaStreamState_closed:
    case ElaStreamState_failed:
        //TODO;
        break;

    default:
        //TODO;
        break;
    }
}

static
bool stream_channel_open(ElaSession *session, int stream, int channel,
                         const char *cookie, void *context)
{
    //TODO;
    return false;
}

static
void stream_channel_opened(ElaSession *session, int stream, int channel,
                            void *context)
{
    //TODO;
}

static
void stream_channel_close(ElaSession *session, int stream, int channel,
                          CloseReason reason, void *context)
{
    //TODO;
}

static
bool stream_channel_data(ElaSession *session, int stream, int channel,
                         const void *data, size_t len, void *context)
{
    //TODO;
    return true;
}

static
void stream_channel_pending(ElaSession *session, int stream, int channel,
                            void *context)
{
    //TODO;
}

static
void stream_channel_resume(ElaSession *session, int stream, int channel,
                           void *context)
{
    //TODO;
}

static void filetransfer_destroy(void *p)
{
    ElaFileTransfer *ft = (ElaFileTransfer *)p;

    if (ft) {
        vlogD("FileTransfer: Filetransfer to %s destroyed.", ft->to);

        if (ft->to)
            free(ft->to);
    }
}

ElaFileTransfer *ela_filetransfer_new(ElaCarrier *w, const char *address,
                           ElaFileTransferInfo *fileinfo_list, size_t count,
                           ElaFileTransferCallbacks *callbacks,
                           void *context)
{
    FileTransferExtension *ext;
    ElaFileTransfer *ft;
    ElaSession *ws;
    int rc;
    int i;

    if (!w || !address || !*address || !fileinfo_list || !count ||
        count > ELA_MAX_TRANSFER_FILE_COUNT || !callbacks) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return NULL;
    }

    for (i = 0; i < (int)count; i++) {
        char zero[ELA_MAX_FILE_ID_LEN] = {0};
        ElaFileTransferInfo *fi = &fileinfo_list[i];

        if (!*fi->filename || fi->file_size) {
            ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
            return NULL;
        }

        if (memcmp(zero, fi->fileid, sizeof(zero)) == 0) {
            //TODO: generate random fileid.
        }
    }

    ext = w->filetransfer_extension;
    if (!ext) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_NOT_EXIST));
        return NULL;
    }

    ws = ela_session_new(w, address);
    if (!ws) {
        vlogE("FileTransfer: Create session to %s for filetransfer error",
              address);
        return NULL;
    }

    ft = (ElaFileTransfer *)rc_zalloc(sizeof(ElaFileTransfer) +
                sizeof(ElaFileTransferInfo) * count, filetransfer_destroy);
    if (!ft) {
        ela_session_close(ws);
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_OUT_OF_MEMORY));
        return NULL;
    }

    ft->ext = ext;
    ft->session = ws;
    ft->streamid = -1;
    ft->to = strdup(address);
    ft->stream_callbacks = &ext->stream_callbacks;
    ft->fileinfo_list = (ElaFileTransferInfo *)(ft + 1);
    ft->count = count;
    memcpy(ft->fileinfo_list, fileinfo_list, sizeof(ElaFileTransferInfo) * count);

    ft->callbacks = *callbacks;
    ft->context = context;

    ft->state = ElaFileTransferState_opened;

    vlogD("FileTransfer: Filetransfer instance to %s created.", ft->to);
    return ft;
}

void ela_filetransfer_close(ElaFileTransfer *ft)
{
    if (!ft)
        return;

    vlogD("FileTransfer: Closing filetransfer to %s.", ft->to);

    //TODO: close channel.

    if (ft->streamid > 0) {
        ela_session_remove_stream(ft->session, ft->streamid);
        ft->streamid = -1;
    }

    if (ft->session) {
        ela_session_close(ft->session);
        ft->session = NULL;
    }

    deref(ft);

    vlogD("FileTransfer: Filetransfer to %s closed.", ft->to);
}

char *ela_filetransfer_get_fileid(ElaFileTransfer *ft, const char *filename,
                                  char *fileid, size_t length)
{
    int i;

    if (!ft || !filename || !*filename || !fileid ||
        length <= ELA_MAX_FILE_ID_LEN) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return NULL;
    }

    for (i = 0; i < (int)ft->count; i++) {
        ElaFileTransferInfo *fi = &ft->fileinfo_list[i];

        if (strcmp(fi->filename, filename) == 0) {
            strcpy(fileid, fi->fileid);
            break;
        }
    }

    if (i == (int)ft->count) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_NOT_EXIST));
        return NULL;
    }

    return fileid;
}


int ela_filetransfer_request(ElaFileTransfer *ft,
                             ElaFileTransferRequestCompleteCallback *callback,
                             void *context)
{
    int rc;

    if (!ft || !callback) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    assert(ft->session);

    ft->request_complete_callback = *callback;
    ft->request_complete_context = context;

    rc = ela_session_add_stream(ft->session, ElaStreamType_application,
                                ELA_STREAM_RELIABLE | ELA_STREAM_MULTIPLEXING,
                                ft->stream_callbacks, ft);
    if (rc < 0) {
        ft->state = ElaFileTransferState_failed;
        vlogE("FileTransfer: Add relialbe multiplexing stream error");
        return rc;
    }

    vlogD("FileTransfer: File transfer request to %s send success", ft->to);
    return 0;
}

int ela_filetransfer_response(ElaFileTransfer *ft, int status,
                              const char *reason)
{
    int rc;

    if (!ft || (status != 0 && !reason)) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    rc = ela_session_add_stream(ft->session, ElaStreamType_application,
                                ELA_STREAM_RELIABLE | ELA_STREAM_MULTIPLEXING,
                                ft->stream_callbacks, ft);
    if (rc < 0) {
        ft->state = ElaFileTransferState_failed;
        vlogE("FileTransfer: Add reliable multiplexing stream error");
        return rc;
    }

    vlogD("FileTransfer: File transfer reply to %s send success", ft->to);
    return 0;
}


int ela_filetransfer_pull(ElaFileTransfer *ft, const char *fileid,
                          uint64_t offset, size_t length)
{
    uint8_t *data;
    size_t data_len;
    int rc;
    int i;

    if (!ft || !fileid || !*fileid) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (ft->is_sender) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    for (i = 0; i < (int)ft->count; i++) {
        if (strcmp(ft->fileinfo_list[i].fileid, fileid) == 0)
            break;
    }

    if (i == (int)ft->count) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_NOT_EXIST));
        return -1;
    }

    //TODO: what about if length == 0.

    if (offset >= ft->fileinfo_list[i].file_size ||
        length >= ft->fileinfo_list[i].file_size ||
        (offset + length) >= ft->fileinfo_list[i].file_size) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (ft->state != ElaFileTransferState_connected) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    rc = ela_stream_write_channel(ft->session, -1, 1, data, data_len);
    if (rc < 0)  {
        vlogE("FileTransfer: Request to pull file %s send error", fileid);
        return -1;
    }

    vlogD("FileTransfer: Request to pull file %s send success", fileid);
    return 0;
}

int ela_filetransfer_send(ElaFileTransfer  *ft, const char *fileid,
                          uint64_t offset, const uint8_t *data, size_t length)
{
    int rc;
    int i;

    if (!ft || !fileid || !*fileid) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    // Allow to transfer zero length data.
    if ((!data && length > 0) || (data && !length)) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (!ft->is_sender) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    for (i = 0; i < (int)ft->count; i++) {
        if (strcmp(ft->fileinfo_list[i].fileid, fileid) == 0)
            break;
    }

    if (i == (int)ft->count) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_NOT_EXIST));
        return -1;
    }

    if (offset >= ft->fileinfo_list[i].file_size ||
        length >= ft->fileinfo_list[i].file_size ||
        (offset + length) >= ft->fileinfo_list[i].file_size) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (ft->state != ElaFileTransferState_connected) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    rc = ela_stream_write_channel(ft->session, -1, 1, data, length);
    if (rc < 0) {
        vlogE("FileTransfer: File %s block data send error", fileid);
        return -1;
    }

    return 0;
}

int ela_filetransfer_pend(ElaFileTransfer *ft, const char *fileid)
{
    int rc;

    if (!ft || !fileid || !*fileid)  {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (!ft->is_sender) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    if (ft->state != ElaFileTransferState_connected) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    rc = ela_stream_pend_channel(ft->session,  ft->streamid, 1);
    if (rc < 0) {
        vlogE("FileTransfer: Request to pend transfer file %s error", fileid);
        return -1;
    }

    vlogD("FileTransfer: Request to pend transfer file %s success", fileid);

    return 0;
}

int ela_filetransfer_resume(ElaFileTransfer *ft, const char *fileid)
{
    int rc;

    if (!ft || !fileid || !*fileid)  {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_INVALID_ARGS));
        return -1;
    }

    if (!ft->is_sender) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    if (ft->state != ElaFileTransferState_connected) {
        ela_set_error(ELA_GENERAL_ERROR(ELAERR_WRONG_STATE));
        return -1;
    }

    rc = ela_stream_resume_channel(ft->session,  ft->streamid, 1);
    if (rc < 0) {
        vlogE("FileTransfer: Request to resume transfer file %s error", fileid);
        return -1;
    }

    vlogD("FileTransfer: Request to resume transfer file %s success", fileid);

    return 0;
}
