#include <moment-nvr/get_file_session.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

Mp4Muxer::Frontend const GetFileSession::mp4_muxer_frontend = {
    muxerStateChanged
};

void
GetFileSession::muxerStateChanged (Mp4Muxer::MuxState   const mux_state,
                                   void               * const _self)
{
    logD_ (_func_);
}

MediaReader::ReadFrameBackend const GetFileSession::read_frame_backend = {
    audioFrame,
    videoFrame
};

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::audioFrame (VideoStream::AudioMessage * const mt_nonnull msg,
                            void                      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    assert (msg->prechunk_size == 0);

    logD_ (_func, " ts ", msg->timestamp_nanosec, " ", msg->frame_type);

    if (self->session_state == SessionState_PassOne) {
        self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Audio,
                                     msg->timestamp_nanosec,
                                     msg->msg_len,
                                     false /* is_sync_sample */);
    } else {
        self->mp4_muxer.pass2_frame (Mp4Muxer::FrameType_Audio,
                                     msg->timestamp_nanosec,
                                     msg->page_list.first,
                                     msg->msg_offset,
                                     msg->msg_len);
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::videoFrame (VideoStream::VideoMessage * const mt_nonnull msg,
                            void                      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    assert (msg->prechunk_size == 0);

    logD_ (_func, (self->session_state == SessionState_PassOne ? "ONE" : "TWO"), " ts ", msg->timestamp_nanosec, " ", msg->frame_type, " len ", msg->msg_len);

    if (self->session_state == SessionState_PassOne) {
        if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
            self->mp4_muxer.pass1_avcSequenceHeader (msg->page_list.first,
                                                     msg->msg_offset,
                                                     msg->msg_len);
        } else
        if (msg->frame_type.isVideoData()) {
            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Video,
                                         msg->timestamp_nanosec,
                                         msg->msg_len,
                                         msg->frame_type.isKeyFrame());
        }
    } else {
        if (msg->frame_type.isVideoData()) {
            self->mp4_muxer.pass2_frame (Mp4Muxer::FrameType_Video,
                                         msg->timestamp_nanosec,
                                         msg->page_list.first,
                                         msg->msg_offset,
                                         msg->msg_len);
        }
    }

    return MediaReader::ReadFrameResult_Success;
}

bool
GetFileSession::readTask (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    logD_ (_func_);

    // TEST
    for (unsigned i = 0; i < 2; ++i) {
        for (;;) {
            logD_ (_func, "calling readMoreData()");
            MediaReader::ReadFrameResult const res = self->media_reader.readMoreData (&read_frame_backend, self);

            if (res == MediaReader::ReadFrameResult_Failure) {
                logE_ (_func, "ReadFrameResult_Failure");
                return false /* do not reschedule */;
            }

            if (res == MediaReader::ReadFrameResult_BurstLimit) {
                logD_ (_func, "ReadFrameResult_BurstLimit");
                return false /* do not reschedule */;
            }

            if (res == MediaReader::ReadFrameResult_NoData) {
                // TEST
                if (i == 0)
                    self->mp4_muxer.pass1_complete ();

                logD_ (_func, "ReadFrameResult_NoData");
                break;
            }
        }

        if (self->session_state == SessionState_PassOne) {
            self->session_state = SessionState_PassTwo;
            self->media_reader.reset ();
        } else {
            break;
        }
    }

    return false /* do not reschedule */;
}

Sender::Frontend const GetFileSession::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
GetFileSession::senderStateChanged (Sender::SendState   const send_state,
                                    void              * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    // TODO Check 'started'
}

void
GetFileSession::senderClosed (Exception * const exc,
                              void      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    // TODO Check 'started'
}

void
GetFileSession::start ()
{
    mutex.lock ();
    started = true;
    mutex.unlock ();

    deferred_reg.scheduleTask (&read_task, false /* permanent */);
}

mt_const void
GetFileSession::init (MomentServer * const mt_nonnull moment,
                      HttpRequest  * const mt_nonnull req,
                      Sender       * const mt_nonnull sender,
                      PagePool     * const page_pool,
                      Vfs          * const vfs,
                      ConstMemory    const stream_name,
                      Time           const start_unixtime_sec,
                      Time           const duration_sec,
                      CbDesc<Frontend> const &frontend)
{
    this->moment = moment;
    this->sender = sender;
    this->frontend = frontend;
    this->duration_sec = duration_sec;

    media_reader.init (page_pool, vfs, stream_name, /* TEST start_unixtime_sec */ getUnixtime() - 10);
    mp4_muxer.init (page_pool, sender, CbDesc<Mp4Muxer::Frontend> (&mp4_muxer_frontend, this, this));

    thread_ctx = moment->getReaderThreadPool()->grabThreadContext (stream_name);
    if (thread_ctx) {
        reader_thread_ctx = thread_ctx;
    } else {
        logE_ (_func, "Could not get reader thread context: ", exc->toString());
        reader_thread_ctx = NULL;
        thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());

    sender->getEventInformer()->subscribe (
            CbDesc<Sender::Frontend> (&sender_frontend, this, this));
}

GetFileSession::GetFileSession ()
    : sender            (this /* coderef_container */),
      thread_ctx        (this /* coderef_container */),
      reader_thread_ctx (NULL),
      media_reader      (this /* coderef_container */),
      mp4_muxer         (this /* coderef_container */),
      session_state     (SessionState_PassOne),
      started           (false)
{
    read_task.cb = CbDesc<DeferredProcessor::TaskCallback> (readTask, this, this);
}

GetFileSession::~GetFileSession ()
{
    deferred_reg.release ();

    if (reader_thread_ctx) {
        moment->getReaderThreadPool()->releaseThreadContext (reader_thread_ctx);
        reader_thread_ctx = NULL;
    }
}

}

