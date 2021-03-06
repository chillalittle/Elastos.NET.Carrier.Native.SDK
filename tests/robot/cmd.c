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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <socket.h>
#include <vlog.h>

#include "ela_carrier.h"
#include "ela_session.h"

#include "cond.h"
#include "cmd.h"
#include "test_context.h"

const char *stream_state_name(ElaStreamState state);

#define CHK_ARGS(exp) if (!(exp)) { \
        vlogE("Invalid command syntax"); \
        return; \
    }

struct SessionContextExtra {
    int init_flag;

    char remote_sdp[2048];
    size_t sdp_len;
    char test_peer_id[(ELA_MAX_ID_LEN + 1) * 2];
};

static SessionContextExtra session_extra = {
    .init_flag = 0,

    .remote_sdp = {0},
    .sdp_len = 0,
    .test_peer_id = {0}
};

static void session_request_callback(ElaCarrier *w, const char *from, const char *bundle,
                                     const char *sdp, size_t len, void *context)
{
    SessionContextExtra *extra = ((SessionContext *)context)->extra;

    extra->sdp_len = len;
    strncpy(extra->remote_sdp, sdp, len);
    extra->remote_sdp[len] = 0;

    strcpy(extra->test_peer_id, from);

    write_ack("srequest received\n");
}

static void session_request_complete_callback(ElaSession *ws, const char *bundle, int status,
                const char *reason, const char *sdp, size_t len, void *context)
{
    SessionContext *sctxt = ((TestContext *)context)->session;
    StreamContext *stream_ctxt = ((TestContext *)context)->stream;
    int rc;

    vlogD("Session complete, status: %d, reason: %s", status,
          reason ? reason : "null");

    if (status != 0) {
        assert(0 && "test client should confirm session request.\n");
        goto cleanup;
    }

    cond_wait(stream_ctxt->cond);
    if (!(stream_ctxt->state_bits & (1 << ElaStreamState_transport_ready))) {
        vlogE("Stream state not 'transport ready' state");
        goto cleanup;
    }

    if (bundle && bundle[0]) {
        vlogD("Request complete callback invoked with bundle %s", bundle);
        write_ack("bundle %s\n", bundle);
    }

    rc = ela_session_start(ws, sdp, len);
    if (rc < 0) {
        vlogE("Start session for robot failed (0x%x)", ela_get_error());
        goto cleanup;
    } else
        vlogD("Start session for robot success");

    cond_wait(stream_ctxt->cond);
    if (!(stream_ctxt->state_bits & (1 << ElaStreamState_connecting))) {
        vlogE("Stream state not 'connnecting' state");
        goto cleanup;
    }

    cond_wait(stream_ctxt->cond);
    if (!(stream_ctxt->state_bits & (1 << ElaStreamState_connected))) {
        vlogE("Stream state not 'connected' state");
        goto cleanup;
    }

    write_ack("sconnect success\n");
    return ;

cleanup:
    if (stream_ctxt->stream_id > 0) {
        ela_session_remove_stream(sctxt->session, stream_ctxt->stream_id);
        stream_ctxt->stream_id = -1;
    }
    if (sctxt->session) {
        ela_session_close(sctxt->session);
        sctxt->session = NULL;
    }
    write_ack("sconnect failed\n");
}

static SessionContext session_context = {
    .request_cb = session_request_callback,
    .request_received = 0,
    .request_cond = NULL,

    .request_complete_cb = session_request_complete_callback,
    .request_complete_status = -1,
    .request_complete_cond = NULL,

    .session = NULL,
    .extra = &session_extra
};

static void stream_on_data(ElaSession *ws, int stream, const void *data,
                           size_t len, void *context)
{
    vlogD("Stream [%d] received data [%.*s]", stream, (int)len, (char*)data);
}

static void stream_state_changed(ElaSession *ws, int stream,
                                 ElaStreamState state, void *context)
{
    StreamContext *stream_ctxt = (StreamContext *)context;

    stream_ctxt->state = state;
    stream_ctxt->state_bits |= 1 << state;

    vlogD("Stream [%d] state changed to: %s", stream, stream_state_name(state));

    cond_signal(stream_ctxt->cond);
}

struct StreamContextExtra {
    struct {
        int will_open_confirm;
        int channel_id;
        int channel_error_state;
    } channels[MAX_CHANNEL_COUNT];

    int portforwarding_id;
};

static StreamContextExtra stream_extra = {
    .channels = { {0, 0, 0 } },
    .portforwarding_id = -1
};

static bool channel_open(ElaSession *ws, int stream, int channel,
                         const char *cookie, void *context)
{
    StreamContextExtra *extra = ((StreamContext *)context)->extra;

    vlogD("Request open new channel %d on stream %d with cookie: %s.",
          channel, stream, cookie);

    return extra->channels[channel-1].will_open_confirm;
}

static
void channel_opened(ElaSession *ws, int stream, int channel, void *context)
{
    StreamContextExtra *extra = ((StreamContext *)context)->extra;

    vlogD("Channel %d opened.", channel);

    extra->channels[channel-1].channel_id = channel;
}

static void channel_close(ElaSession *ws, int stream, int channel,
                          CloseReason reason, void *context)
{
    StreamContextExtra *extra = ((StreamContext *)context)->extra;

    const char *state_name[] = {
        "Normal",
        "Timeout",
        "Error"
    };

    vlogD("Channel %d closeing with %s.", channel, state_name[reason]);

    if (reason == CloseReason_Error || reason == CloseReason_Timeout)
        extra->channels[channel-1].channel_error_state = 1;
}

static  bool channel_data(ElaSession *ws, int stream, int channel,
                          const void *data, size_t len, void *context)
{
    vlogD("stream [%d] channel [%d] received data [%.*s]",
          stream, channel, (int)len, (char*)data);
    return true;
}

static
void channel_pending(ElaSession *ws, int stream, int channel, void *context)
{
    vlogD("stream [%d] channel [%d] pend data.", stream, channel);
}

static
void channel_resume(ElaSession *ws, int stream, int channel, void *context)
{
    vlogD("stream [%d] channel [%d] resume data.", stream, channel);
}

static ElaStreamCallbacks stream_callbacks = {
    .stream_data = stream_on_data,
    .state_changed = stream_state_changed,
    .channel_open = channel_open,
    .channel_opened = channel_opened,
    .channel_close = channel_close,
    .channel_data = channel_data,
    .channel_pending = channel_pending,
    .channel_resume = channel_resume
};

static Condition DEFINE_COND(stream_cond);

static StreamContext stream_context = {
    .cbs = &stream_callbacks,
    .stream_id = -1,
    .state = 0,
    .state_bits = 0,
    .cond = &stream_cond,
    .extra = &stream_extra
};

TestContext test_context = {
    .carrier = &carrier_context,
    .session = &session_context,
    .stream  = &stream_context
};

/*
 *  command format: ready"
 */
static void wready(TestContext *context, int argc, char *argv[])
{
    CHK_ARGS(argc == 1);

    //ready_to_echo = 1;
}

/*
 * command format: fadd userid useraddr hello
 */
static void fadd(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;

    CHK_ARGS(argc == 4);

    if (ela_is_friend(w, argv[1]))
        ela_remove_friend(w, argv[1]);

    context->carrier->fadd_in_progress = true;
    rc = ela_add_friend(w, argv[2], argv[3]);
    if (rc < 0) {
        vlogE("Add user %s to be friend error (0x%x)", argv[2], ela_get_error());
        if (context->carrier->fadd_in_progress) {
            context->carrier->fadd_in_progress = false;
            write_ack("fadd failed\n");
        }
    } else {
        vlogD("Add user %s to be friend success", argv[2]);
    }
}

/*
 * command format: faccept userid entrusted expire
 */
static void faccept(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;

    CHK_ARGS(argc == 2);
    context->carrier->fadd_in_progress = true;
    rc = ela_accept_friend(w, argv[1]);
    if (rc < 0) {
        if (ela_get_error() == ELA_GENERAL_ERROR(ELAERR_ALREADY_EXIST))
            vlogD("User %s already is friend.", argv[1]);
        else
            vlogE("Accept friend request from user %s error (0x%x)",
                  argv[1], ela_get_error());
        if (context->carrier->fadd_in_progress) {
            context->carrier->fadd_in_progress = false;
            write_ack("fadd failed\n");
        }
    } else {
        vlogD("Accept friend request from user %s success", argv[1]);
    }
}

/*
 * command format: fmsg userid message
 */
static void fmsg(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;

    CHK_ARGS(argc == 3);

    rc = ela_send_friend_message(w, argv[1], argv[2], strlen(argv[2]) + 1);
    if (rc < 0)
        vlogE("Send message to friend %s error (0x%x)",
              argv[1], ela_get_error());
    else
        vlogD("Send message to friend %s success", argv[1]);
}

/*
 * command format: fremove userid
 */
static void fremove(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;

    CHK_ARGS(argc == 2);

    rc = ela_remove_friend(w, argv[1]);
    if (rc < 0) {
        vlogE("Remove friend %s error (0x%x)", argv[1], ela_get_error());
        write_ack("fremove failed\n");
    } else {
        vlogD("Remove friend %s success", argv[1]);
    }
}

static void invite_response_callback(ElaCarrier *w, const char *friendid,
                                     int status, const char *reason,
                                     const void *data, size_t len, void *context)
{
    vlogD("Received invite response from friend %s", friendid);

    if (status == 0) {
        vlogD("Message within response: %.*s", (int)len, (const char *)data);
    } else {
        vlogD("Refused: %s", reason);
    }
}

/*
 * command format: finvite userid data
 */
static void finvite(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;

    CHK_ARGS(argc == 3);

    rc = ela_invite_friend(w, argv[1], argv[2], strlen(argv[2] + 1),
                               invite_response_callback, NULL);
    if (rc < 0)
        vlogE("Send invite request to friend %s error (0x%x)",
              argv[1], ela_get_error());
    else
        vlogD("Send invite request to friend %s success", argv[1]);
}

/*
 * command format: freplyinvite userid [ confirm data ] | [ refuse reason ]
 */
static void freplyinvite(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    int rc;
    int status = 0;
    const char *reason = NULL;
    const char *msg = NULL;
    size_t msg_len = 0;

    CHK_ARGS(argc == 4);

    if (strcmp(argv[2], "confirm") == 0) {
        msg = argv[3];
        msg_len = strlen(argv[3]) + 1;
    } else if (strcmp(argv[2], "refuse") == 0) {
        status = -1; // TODO: fix to correct status code.
        reason = argv[3];
    } else {
        vlogE("Unknown sub command: %s", argv[2]);
        return;
    }

    rc = ela_reply_friend_invite(w, argv[1], status, reason, msg, msg_len);
    if (rc < 0)
        vlogE("Reply invite request from friend %s error (0x%x)",
              argv[1], ela_get_error());
    else
        vlogD("Reply invite request from friend %s success", argv[1]);
}

/*
 * command format: kill
 */
static void wkill(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;

    vlogI("Kill robot instance.");
    ela_kill(w);
}

static void robot_context_reset(TestContext *context)
{
    SessionContext *sctxt = context->session;
    StreamContext *stream_ctxt = context->stream;

    sctxt->request_received = 0;
    sctxt->request_complete_status = -1;
    sctxt->session = NULL;

    sctxt->extra->init_flag = 0;
    sctxt->extra->remote_sdp[0] = 0;
    sctxt->extra->sdp_len = 0;
    sctxt->extra->test_peer_id[0] = 0;

    cond_reset(stream_ctxt->cond);
    stream_ctxt->stream_id = -1;
    stream_ctxt->state = 0;
    stream_ctxt->state_bits = 0;
    stream_ctxt->extra->portforwarding_id = -1;
}

/*
 * command format: sinit
 */
static void sinit(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    SessionContext *sctxt = context->session;
    int rc;

    CHK_ARGS(argc == 1);

    robot_context_reset(context);

    rc = ela_session_init(w);
    if (rc < 0) {
        vlogE("session init failed: 0x%x", ela_get_error());
        write_ack("sinit failed\n");
        return;
    } else {
        ela_session_set_callback(w, NULL, sctxt->request_cb, sctxt);
        vlogD("session init success.");
        sctxt->extra->init_flag = 1;
        write_ack("sinit success\n");
    }
}

/*
 * command format: srequest peerid stream_options
 */
static void srequest(TestContext *context, int argc, char *argv[])
{
    SessionContext *sctxt = context->session;
    StreamContext *stream_ctxt = context->stream;
    const char *bundle;
    int rc;

    CHK_ARGS(argc == 3 || argc == 4);

    sctxt->session = ela_session_new(context->carrier->carrier, argv[1]);
    if (!sctxt->session) {
        vlogE("New session to %s failed: 0x%x", argv[1], ela_get_error());
        goto cleanup;
    }

    stream_ctxt->stream_id = ela_session_add_stream(sctxt->session,
                                        ElaStreamType_text, atoi(argv[2]),
                                        stream_ctxt->cbs, stream_ctxt);
    if (stream_ctxt->stream_id < 0) {
        vlogE("Add text stream failed: 0x%x", ela_get_error());
        goto cleanup;
    }

    cond_wait(stream_ctxt->cond);
    if (!(stream_ctxt->state_bits & (1 << ElaStreamState_initialized))) {
        vlogE("Stream state not 'initialized' state");
        goto cleanup;
    }

    if (argc == 4)
        bundle = argv[3];
    else
        bundle = NULL;

    rc = ela_session_request(sctxt->session, bundle, sctxt->request_complete_cb, context);
    if (rc < 0) {
        vlogE("Session request failed: 0x%x", ela_get_error());
        goto cleanup;
    }

    vlogD("sesion request succeed");
    write_ack("srequest success\n");

    // the request complete callback should be invoked soon later, and do
    // the rest work.
    return;

cleanup:
    if (stream_ctxt->stream_id > 0) {
        ela_session_remove_stream(sctxt->session, stream_ctxt->stream_id);
        stream_ctxt->stream_id = -1;
    }

    if (sctxt->session) {
        ela_session_close(sctxt->session);
        sctxt->session = NULL;
    }

    write_ack("srequest failed\n");
}

/*
 * command format: sreply comfirm stream_type stream_options
 *                 sreply refuse
 */
static void sreply(TestContext *context, int argc, char *argv[])
{
    SessionContext *sctxt = context->session;
    StreamContext *stream_ctxt = context->stream;
    int need_sreply_ack = 1;
    int rc;

    CHK_ARGS(argc == 4 || argc == 2);

    sctxt->session = ela_session_new(context->carrier->carrier, sctxt->extra->test_peer_id);
    if (!sctxt->session) {
        vlogE("New session to %s failed: 0x%x",
              sctxt->extra->test_peer_id, ela_get_error());
        write_ack("sreply failed\n");
        return;
    }

    if (strcmp(argv[1], "confirm") == 0) {
        int stream_type    = atoi(argv[2]);
        int stream_options = atoi(argv[3]);

        stream_ctxt->stream_id = ela_session_add_stream(sctxt->session,
                    stream_type, stream_options, stream_ctxt->cbs, stream_ctxt);
        if (stream_ctxt->stream_id < 0) {
            vlogE("Add stream failed: 0x%x", ela_get_error());
            goto cleanup;
        }

        cond_wait(stream_ctxt->cond);
        if (!(stream_ctxt->state_bits & (1 << ElaStreamState_initialized))) {
            vlogE("Stream is in %d state, not 'initialized'", stream_ctxt->state);
            goto cleanup;
        }

        rc = ela_session_reply_request(sctxt->session, NULL, 0, NULL);
        if (rc < 0) {
            vlogE("Confirm session reply request failed: 0x%x", ela_get_error());
            goto cleanup;
        }

        cond_wait(stream_ctxt->cond);
        if (!(stream_ctxt->state_bits & (1 << ElaStreamState_transport_ready))) {
            vlogE("Stream is in %d state, not 'transport ready'", stream_ctxt->state);
            goto cleanup;
        }

        vlogD("Confirm session request success.");
        write_ack("sreply success\n");

        need_sreply_ack = 0;

        rc = ela_session_start(sctxt->session, sctxt->extra->remote_sdp,
                                   sctxt->extra->sdp_len);
        if (rc < 0) {
            vlogE("Start session failed: 0x%x", ela_get_error());
            goto cleanup;
        }

        cond_wait(stream_ctxt->cond);
        if (!(stream_ctxt->state_bits & (1 << ElaStreamState_connecting))) {
            vlogE("Stream is in %d state, not 'connecting'", stream_ctxt->state);
            goto cleanup;
        }

        cond_wait(stream_ctxt->cond);
        if (!(stream_ctxt->state_bits & (1 << ElaStreamState_connected))) {
            vlogE("Stream is in %d state, not 'connected'", stream_ctxt->state);
            goto cleanup;
        }

        write_ack("sconnect success\n");
        return;
    }
    else if (strcmp(argv[1], "refuse") == 0) {
        rc = ela_session_reply_request(sctxt->session, NULL, 1, "testing");
        if (rc < 0) {
            vlogE("Refuse session request failed: 0x%x", ela_get_error());
            goto cleanup;
        }

        vlogD("Refused session request success");
        write_ack("sreply success\n");
        return;
    }
    else {
        vlogE("Unknown sub command");
        write_ack("sreply failed\n");
        return;
    }
    return;

cleanup:
    if (stream_ctxt->stream_id > 0) {
        ela_session_remove_stream(sctxt->session, stream_ctxt->stream_id);
        stream_ctxt->stream_id = -1;
    }

    if (need_sreply_ack && sctxt->session)
        ela_session_reply_request(sctxt->session, NULL, 2, "Error");

    if (sctxt->session) {
        ela_session_close(sctxt->session);
        sctxt->session = NULL;
    }

    if (need_sreply_ack)
        write_ack("sreply failed\n");
    else
        write_ack("sconnect failed\n");
}

static void sfree(TestContext *context, int argc, char *argv[])
{
    ElaCarrier *w = context->carrier->carrier;
    SessionContext *sctxt = context->session;
    StreamContext *stream_ctxt = context->stream;

    CHK_ARGS(argc == 1);

    if (stream_ctxt->stream_id > 0) {
        ela_session_remove_stream(sctxt->session, stream_ctxt->stream_id);

        cond_wait(stream_ctxt->cond);
        if (stream_ctxt->state != ElaStreamState_closed)
            vlogE("Stream should be closed, but (%d)", stream_ctxt->state);
        stream_ctxt->stream_id = -1;
    }

    if (sctxt->session) {
        ela_session_close(sctxt->session);
        sctxt->session = NULL;
    }

    if (sctxt->extra->init_flag) {
        ela_session_cleanup(w);
        sctxt->extra->init_flag = 0;
    }

    vlogD("Robot session cleanuped");
}

static void spfsvcadd(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;
    int rc;
    PortForwardingProtocol protocol;

    CHK_ARGS(argc == 5);

    if (strcmp(argv[2], "tcp") == 0)
        protocol = PortForwardingProtocol_TCP;
    else {
        vlogE("Invalid portforwarding protocol: %s", argv[2]);
        write_ack("spfsvcadd failed\n");
        return;
    }

    rc = ela_session_add_service(session, argv[1], protocol, argv[3], argv[4]);
    if (rc < 0) {
        vlogE("Add service %s failed (0x%x)", argv[1], ela_get_error());
        write_ack("spfsvcadd failed\n");
        return;
    }

    vlogD("Added service %s to current session", argv[1]);
    write_ack("spfsvcadd success\n");
}

static void spfsvcremove(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;

    CHK_ARGS(argc == 2);

    ela_session_remove_service(session, argv[1]);

    vlogD("Service %s removed", argv[1]);
}

static void spf_open(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;
    StreamContext *stream_ctxt = context->stream;
    PortForwardingProtocol protocol;
    int pfid;

    CHK_ARGS(argc == 5);

    if (strcmp(argv[2], "tcp") == 0)
        protocol = PortForwardingProtocol_TCP;
    else {
        vlogE("Invalid portforwarding protocol %s", argv[2]);
        write_ack("spfopen failed\n");
        return;
    }

    // Double check.
    if (stream_ctxt->extra->portforwarding_id > 0) {
        ela_stream_close_port_forwarding(session, stream_ctxt->stream_id,
                                        stream_ctxt->extra->portforwarding_id);
        stream_ctxt->extra->portforwarding_id = -1;
    }

    pfid = ela_stream_open_port_forwarding(session, stream_ctxt->stream_id,
                                        argv[1], protocol, argv[3], argv[4]);
    if (pfid <= 0) {
        vlogE("Open portforwarding for service %s failed: 0x%x",
              argv[1], ela_get_error());
        write_ack("spfopen failed\n");
        return;
    }

    stream_ctxt->extra->portforwarding_id = pfid;

    vlogD("Open portforwarding for service %s on %s:%s success",
          argv[1], argv[3], argv[4]);
    write_ack("spfopen success\n");
}

static void spf_close(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;
    StreamContext *stream_ctxt = context->stream;
    int rc;

    CHK_ARGS(argc == 1);

    if (stream_ctxt->extra->portforwarding_id > 0) {
        rc = ela_stream_close_port_forwarding(session, stream_ctxt->stream_id,
                                        stream_ctxt->extra->portforwarding_id);
        stream_ctxt->extra->portforwarding_id = -1;

        if (rc < 0) {
            vlogE("Close portforwarding failed: 0x%x", ela_get_error());
            write_ack("spfclose failed\n");
            return;
        }
    }
    write_ack("spfclose success\n");
}

static void cready2open(TestContext *context, int argc, char *argv[])
{
    StreamContextExtra *extra = context->stream->extra;
    int will_open_confirm = 0;
    int i;

    CHK_ARGS(argc == 2);

    if (strcmp(argv[1], "confirm") == 0)
        will_open_confirm = 1;
    else if (strcmp(argv[1], "refuse") == 0)
        will_open_confirm = 0;
    else {
        vlogE("Unknown command option: %s", argv[2]);
        write_ack("cready2open failed\n");
        return;
    }

    for (i = 0; i < MAX_CHANNEL_COUNT; i++)
        extra->channels[i].will_open_confirm = will_open_confirm;

    write_ack("cready2open success\n");
}

static void cpend(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;
    StreamContext *stream_ctxt = context->stream;
    int rc;

    CHK_ARGS(argc == 1);

    rc = ela_stream_pend_channel(session, stream_ctxt->stream_id,
                                 stream_ctxt->extra->channels[0].channel_id);
    if (rc < 0) {
        vlogE("Pending stream %d channel %d failed: 0x%x",
              stream_ctxt->stream_id,
              stream_ctxt->extra->channels[0].channel_id,
              ela_get_error());

        write_ack("cpend failed\n");
    } else {
        write_ack("cpend success\n");
    }
}

static void cresume(TestContext *context, int argc, char *argv[])
{
    ElaSession *session = context->session->session;
    StreamContext *stream_ctxt = context->stream;
    int rc;

    CHK_ARGS(argc == 1);

    rc = ela_stream_resume_channel(session, stream_ctxt->stream_id,
                                   stream_ctxt->extra->channels[0].channel_id);
    if (rc < 0) {
        vlogE("Resume stream %d channel %d failed: 0x%x",
              stream_ctxt->stream_id,
              stream_ctxt->extra->channels[0].channel_id,
              ela_get_error());

        write_ack("cresume failed\n");
    } else {
        write_ack("cresume success\n");
    }
}

static struct command {
    const char* name;
    void (*cmd_cb) (TestContext *context, int argc, char *argv[]);
} commands[] = {
    { "ready",        wready       },
    { "fadd",         fadd         },
    { "faccept",      faccept      },
    { "fmsg",         fmsg         },
    { "fremove",      fremove      },
    { "finvite",      finvite      },
    { "freplyinvite", freplyinvite },
    { "kill",         wkill        },
    { "sinit",        sinit        },
    { "srequest",     srequest     },
    { "sreply",       sreply       },
    { "sfree",        sfree        },
    { "spfsvcadd",    spfsvcadd    },
    { "spfsvcremove", spfsvcremove },
    { "spfopen",      spf_open     },
    { "spfclose",     spf_close    },
    { "cready2open",  cready2open  },
    { "cpend",        cpend        },
    { "cresume",      cresume      },
    { NULL, NULL},
};

SOCKET cmd_sock = INVALID_SOCKET;

int start_cmd_listener(const char *host, const char *port)
{
    int rc;
    SOCKET svr_sock;

    assert(host && *host);
    assert(port && *port);

    svr_sock = socket_create(SOCK_STREAM, host, port);
    if (svr_sock == INVALID_SOCKET) {
        vlogE("Create server socket(%s:%s) error.", host, port);
        return -1;
    }

    rc = listen(svr_sock, 1);
    if (rc < 0) {
        vlogE("Listen server socket(%s:%s) error.", host, port);
        socket_close(svr_sock);
        return -1;
    }

    vlogI("Test robot waiting on: %s:%s", host, port);

    cmd_sock = accept(svr_sock, NULL, NULL);
    socket_close(svr_sock);

    if (cmd_sock == INVALID_SOCKET) {
        vlogE("Accept connection error.");
        return -1;
    }

    vlogI("Test cases connected.");
    return 0;
}

void stop_cmd_listener(void)
{
    if (cmd_sock != INVALID_SOCKET) {
        socket_close(cmd_sock);
        cmd_sock = INVALID_SOCKET;
        vlogI("Close control command socket.");
    }
}

static char cmd_buffer[2048];
static char *cmd_ptr = cmd_buffer;
static int cmd_len = 0;

static char *get_cmd_from_buffer(void)
{
    if (cmd_len > 0) {
        // find 0x0A or 0x0D0A
        char *p = (char *)memchr(cmd_ptr, 0x0A, cmd_len);
        if (p) {
            char *cmd = cmd_ptr;

            cmd_len -= (int)(p - cmd_ptr + 1);
            assert(cmd_len >= 0);
            if (cmd_len < 0) {
                // Should be dead code.
                vlogE("Error parse command buffer. Emit a kill command to shutdown robot.");
                return "kill";
            }

            cmd_ptr = cmd_len ? p + 1 : cmd_buffer;
            *p = 0;
            --p;
            if (*p == 0x0D)
                *p = 0;

            vlogD("@@@@@@@@ Got command: %s", cmd);

            return cmd;
        }

        if (cmd_ptr != cmd_buffer) {
            memmove(cmd_buffer, cmd_ptr, cmd_len);
            cmd_ptr = cmd_buffer;
        }
    }

    return NULL;
}

char* read_cmd(void)
{
    int nfds;
    fd_set rfds;
    char *cmd;

    cmd = get_cmd_from_buffer();
    if (cmd)
        return cmd;

read_cmd:
    FD_ZERO(&rfds);
    FD_SET(cmd_sock, &rfds);

    nfds = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    if (nfds < 0) {
        vlogE("Read command error. Emit a kill command to shutdown robot.");
        return "kill";
    } else if (nfds == 0) {
        goto read_cmd;
    } else {
        ssize_t

        rc = recv(cmd_sock, cmd_buffer + cmd_len, sizeof(cmd_buffer) - cmd_len, 0);
        if (rc < 0) {
            vlogE("Read command error. Emit a kill command to shutdown robot.");
            return "kill";
        } else if (rc == 0) {
            vlogE("Test cases disconnected. shutdown robot.");
            return "kill";
        }

        cmd_len += (int)rc;

        cmd = get_cmd_from_buffer();
        if (!cmd)
            goto read_cmd;
        else
            return cmd;
    }
}

void do_cmd(TestContext *context, char *line)
{
    char *args[64];
    int count = 0;
    char *p;
    int word = 0;

    for (p = line; *p != 0; p++) {
        if (isspace(*p)) {
            *p = 0;
            word = 0;
        } else {
            if (word == 0) {
                args[count] = p;
                count++;
            }

            word = 1;
        }
    }

    if (count > 0) {
        struct command *p;

        for (p = commands; p->name; p++) {
            if (strcmp(args[0], p->name) == 0) {
                vlogD("execute command %s", args[0]);
                p->cmd_cb(context, count, args);
                return;
            }
        }

        vlogE("Unknown command: %s", args[0]);
    }
}

int write_ack(const char *what, ...)
{
    va_list ap;
    char ack[1024];

    assert(cmd_sock != INVALID_SOCKET);
    assert(what);

    va_start(ap, what);
    vsprintf(ack, what, ap);
    va_end(ap);

    assert(ack[strlen(ack) - 1] == '\n');
    vlogD("@@@@@@@@ Acknowledge: %.*s", (int)(strlen(ack)-1), ack);

    return send(cmd_sock, ack, (int)strlen(ack), 0);
}
