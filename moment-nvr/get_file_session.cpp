#include <moment-nvr/get_file_session.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

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

    if (self->session_state == SessionState_Header) {
        self->last_audio_ts_nanosec = msg->timestamp_nanosec;
        self->got_last_audio_ts = true;

        self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Audio,
                                     msg->timestamp_nanosec,
                                     msg->msg_len,
                                     false /* is_sync_sample */);
    } else {
        if (!self->got_last_audio_ts
            || msg->timestamp_nanosec <= self->last_audio_ts_nanosec)
        {
            logD_ (_func, "Finish");
            return MediaReader::ReadFrameResult_Finish;
        }

        // TODO Send audio data
#if 0
        self->mp4_muxer.pass2_frame (Mp4Muxer::FrameType_Audio,
                                     msg->timestamp_nanosec,
                                     msg->page_list.first,
                                     msg->msg_offset,
                                     msg->msg_len);
#endif
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) MediaReader::ReadFrameResult
GetFileSession::videoFrame (VideoStream::VideoMessage * const mt_nonnull msg,
                            void                      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    assert (msg->prechunk_size == 0);

//    logD_ (_func, (self->session_state == SessionState_Header ? "HEADER" : "DATA"),
//           " ts ", msg->timestamp_nanosec, " ", msg->frame_type, " len ", msg->msg_len);

    assert (self->session_state != SessionState_Complete);

    if (self->session_state == SessionState_Header) {
        if (msg->timestamp_nanosec > (self->start_unixtime_sec + self->duration_sec) * 1000000000) {
            logD_ (_func, "Finish");
            return MediaReader::ReadFrameResult_Finish;
        }

        if (msg->frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
            self->mp4_muxer.pass1_avcSequenceHeader (msg->page_pool,
                                                     msg->page_list.first,
                                                     msg->msg_offset,
                                                     msg->msg_len);
        } else
        if (msg->frame_type.isVideoData()) {
            self->last_video_ts_nanosec = msg->timestamp_nanosec;
            self->got_last_video_ts = true;

            self->mp4_muxer.pass1_frame (Mp4Muxer::FrameType_Video,
                                         msg->timestamp_nanosec,
                                         msg->msg_len,
                                         msg->frame_type.isKeyFrame());
        }
    } else {
        if (msg->frame_type.isVideoData()) {
            if (!self->got_last_video_ts) {
                logD_ (_func, "Finish");
                return MediaReader::ReadFrameResult_Finish;
            }

            Size const max_header_len = RtmpConnection::MaxHeaderLen; /* FIXME Ugly */
            Sender::MessageEntry_Pages * const msg_pages =
                    Sender::MessageEntry_Pages::createNew (max_header_len);
            msg->page_pool->msgRef (msg->page_list.first);

            msg_pages->page_pool = msg->page_pool;
            msg_pages->first_page = msg->page_list.first;
            msg_pages->msg_offset = msg->msg_offset;
            msg_pages->header_len = 0;

            bool burst_limit = false;
            bool sender_limit = false;

            self->sender->lock ();
            self->sender->sendMessage_unlocked (msg_pages, true /* do_flush */);
            {
                Sender::SendState const send_state = self->sender->getSendState_unlocked();
                if (send_state != Sender::SendState::ConnectionReady) {
                    logD_ (_func, "Connection overloaded");

                    if (self->bps_limit_timer) {
                        self->thread_ctx->getTimers()->deleteTimer (self->bps_limit_timer);
                        self->bps_limit_timer = NULL;
                    }

                    burst_limit = true;
                    sender_limit = true;
                }
            }
            self->sender->unlock ();

            self->bytes_transferred += msg->msg_len;

            if (msg->timestamp_nanosec >= self->last_video_ts_nanosec) {
                logD_ (_func, "Finish");
                return MediaReader::ReadFrameResult_Finish;
            }

            if (!burst_limit) {
                if (self->first_data_read) {
                    if (self->first_burst_size
                        && self->bytes_transferred >= self->first_burst_size)
                    {
                        logD_ (_func, "first_burst_size limit");
                        burst_limit = true;
                    }
                } else
                if (self->bps_limit) {
                    Time const cur_time_millisec = getTimeMilliseconds();
                    if (cur_time_millisec <= self->transfer_start_time_millisec
                        || (double) self->bytes_transferred /
                                   ((double) (cur_time_millisec - self->transfer_start_time_millisec) / 1000.0)
                               >= (double) self->bps_limit)
                    {
                        logD_ (_func, "bps_limit");
                        burst_limit = true;
                    }
                }
            }

            if (burst_limit) {
                if (!sender_limit) {
                    assert (!self->bps_limit_timer);
                    self->bps_limit_timer =
                            self->thread_ctx->getTimers()->addTimer (
                                    CbDesc<Timers::TimerCallback> (bpsLimitTimerTick, self, self),
                                    1     /* time_seconds */,
                                    false /* periodical */,
                                    true  /* auto_delete */);
                }
                return MediaReader::ReadFrameResult_BurstLimit;
            }
        }
    }

    return MediaReader::ReadFrameResult_Success;
}

mt_sync_domain (readTask) bool
GetFileSession::senderClosedTask (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    logD_ (_func_);
    self->session_state = SessionState_Complete;
    return false /* do not reschedule */;
}

mt_sync_domain (readTask) bool
GetFileSession::readTask (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    logD_ (_func_);

    if (self->session_state == SessionState_Header) {
        MOMENT_SERVER__HEADERS_DATE

        for (;;) {
            MediaReader::ReadFrameResult const res = self->media_reader.readMoreData (&read_frame_backend, self);
            if (res == MediaReader::ReadFrameResult_Failure) {
                logE_ (_func, "ReadFrameResult_Failure");

                ConstMemory msg = "Data retrieval error";
                self->sender->send (self->page_pool,
                                    true /* do_flush */,
                                    // TODO No cache
                                    MOMENT_SERVER__500_HEADERS (msg.len()),
                                    "\r\n",
                                    msg);

                if (!self->req_is_keepalive)
                    self->sender->closeAfterFlush ();

                logA_ ("mod_nvr 500 ", self->req_client_addr, " ", self->req_request_line);
                return false /* do not reschedule */;
            }

            bool header_done = false;
            if (res == MediaReader::ReadFrameResult_NoData) {
                logD_ (_func, "ReadFrameResult_NoData");

                if (!self->got_last_audio_ts &&
                    !self->got_last_video_ts)
                {
                    ConstMemory msg = "Requested video data not found";
                    self->sender->send (self->page_pool,
                                        true /* do_flush */,
                                        // TODO No cache
                                        MOMENT_SERVER__404_HEADERS (msg.len()),
                                        "\r\n",
                                        msg);

                    if (!self->req_is_keepalive)
                        self->sender->closeAfterFlush ();

                    logA_ ("mod_nvr 404 ", self->req_client_addr, " ", self->req_request_line);
                    return false /* do not reschedule */;
                }

                header_done = true;
            } else
            if (res == MediaReader::ReadFrameResult_Finish) {
                logD_ (_func, "ReadFrameResult_Finish");
                header_done = true;
            }

            if (header_done) {
                self->session_state = SessionState_Data;
                self->media_reader.reset ();
                break;
            }

            assert (res != MediaReader::ReadFrameResult_BurstLimit);
            assert (res == MediaReader::ReadFrameResult_Success);
        }

        PagePool::PageListHead const mp4_header = self->mp4_muxer.pass1_complete ();

        Size const mp4_header_len = PagePool::countPageListDataLen (mp4_header.first, 0 /* msg_offset */);
        {
            ConstMemory msg = "Requested video data not found";
            self->sender->send (self->page_pool,
                                true /* do_flush */,
                                // TODO No cache
                                MOMENT_SERVER__OK_HEADERS (
                                        "video/mp4",
                                        mp4_header_len + self->mp4_muxer.getTotalDataSize()),
                                "\r\n");
            logD_ (_func, "CONTENT-LENGTH: ", mp4_header_len + self->mp4_muxer.getTotalDataSize());

            if (!self->req_is_keepalive)
                self->sender->closeAfterFlush ();

            logA_ ("mod_nvr 200 ", self->req_client_addr, " ", self->req_request_line);
        }

        {
            Size const max_header_len = RtmpConnection::MaxHeaderLen; /* FIXME Ugly */
            Sender::MessageEntry_Pages * const msg_pages =
                    Sender::MessageEntry_Pages::createNew (max_header_len);

            msg_pages->page_pool = self->page_pool;
            msg_pages->first_page = mp4_header.first;
            msg_pages->msg_offset = 0;
            msg_pages->header_len = 0;

            self->sender->sendMessage (msg_pages, true /* do_flush */);
        }

        self->transfer_start_time_millisec = getTimeMilliseconds();
        self->bytes_transferred += mp4_header_len;

        self->sender->getEventInformer()->subscribe (
                CbDesc<Sender::Frontend> (&sender_frontend, self, self));
    }

    if (self->session_state == SessionState_Data) {
        self->doReadData ();
        self->first_data_read = false;
    }

    return false /* do not reschedule */;
}

mt_sync_domain (readTask) void
GetFileSession::bpsLimitTimerTick (void * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);

    self->thread_ctx->getTimers()->deleteTimer (self->bps_limit_timer);
    self->bps_limit_timer = NULL;

    self->doReadData ();
}

mt_sync_domain (readTask) void
GetFileSession::doReadData ()
{
    if (session_state == SessionState_Complete) {
        logD_ (_func, "SessionState_Complete");
        return;
    }

    for (;;) {
        MediaReader::ReadFrameResult const res = media_reader.readMoreData (&read_frame_backend, this);
        if (res == MediaReader::ReadFrameResult_Failure) {
            logE_ (_func, "ReadFrameResult_Failure");
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader.reset ();
            return;
        }

        if (res == MediaReader::ReadFrameResult_BurstLimit) {
            logD_ (_func, "ReadFrameResult_BurstLimit");
            return;
        }

        bool data_done = false;
        if (res == MediaReader::ReadFrameResult_NoData) {
            logD_ (_func, "ReadFrameResult_NoData");
            data_done = true;
        } else
        if (res == MediaReader::ReadFrameResult_Finish) {
            logD_ (_func, "ReadFrameResult_Finish");
            data_done = true;
        }

        if (data_done) {
            session_state = SessionState_Complete;
            if (!req_is_keepalive) {
                sender->closeAfterFlush ();
            }
            media_reader.reset ();
            break;
        }

        assert (res == MediaReader::ReadFrameResult_Success);
    }
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

    if (self->bps_limit_timer)
        return;

    if (send_state == Sender::SendState::ConnectionReady)
        self->deferred_reg.scheduleTask (&self->read_task, false /* permanent */);
}

void
GetFileSession::senderClosed (Exception * const exc,
                              void      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
    self->deferred_reg.scheduleTask (&self->sender_closed_task, false /* permanent */);
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
                      Time           /* TEST const */ start_unixtime_sec,
                      Time           const duration_sec,
                      CbDesc<Frontend> const &frontend)
{
    // TEST
    start_unixtime_sec = getUnixtime() - 31;

    this->moment = moment;
    this->page_pool = page_pool;
    this->sender = sender;
    this->frontend = frontend;

    this->start_unixtime_sec = start_unixtime_sec;
    this->duration_sec = duration_sec;

    this->req_is_keepalive = req->getKeepalive();
    this->req_client_addr = req->getClientAddress();
    this->req_request_line = st_grab (new (std::nothrow) String (req->getRequestLine()));

    media_reader.init (page_pool, vfs, stream_name, start_unixtime_sec, 0 /* burst_size_limit */);
    mp4_muxer.init (page_pool);

    thread_ctx = moment->getReaderThreadPool()->grabThreadContext (stream_name);
    if (thread_ctx) {
        reader_thread_ctx = thread_ctx;
    } else {
        logE_ (_func, "Could not get reader thread context: ", exc->toString());
        reader_thread_ctx = NULL;
        thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    deferred_reg.setDeferredProcessor (thread_ctx->getDeferredProcessor());
}

GetFileSession::GetFileSession ()
    : page_pool          (this /* coderef_container */),
      sender             (this /* coderef_container */),
      start_unixtime_sec (0),
      duration_sec       (0),
      req_is_keepalive   (false),
      first_burst_size   (1 << 20 /* 1 MB */),
      bps_limit          (0),
      transfer_start_time_millisec (0),
      bytes_transferred  (0),
      thread_ctx         (this /* coderef_container */),
      reader_thread_ctx  (NULL),
      media_reader       (this /* coderef_container */),
      session_state      (SessionState_Header),
      first_data_read    (false),
      got_last_audio_ts  (false),
      last_audio_ts_nanosec (0),
      got_last_video_ts  (false),
      last_video_ts_nanosec (0),
      started            (false)
{
    read_task.cb  = CbDesc<DeferredProcessor::TaskCallback> (readTask,  this, this);
    sender_closed_task.cb = CbDesc<DeferredProcessor::TaskCallback> (senderClosedTask, this, this);
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

