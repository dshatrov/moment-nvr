#include <moment-nvr/types.h>

#include <moment-nvr/media_viewer.h>


/* Задачи:
     1. Получение файла;
     2. Воспроизведение с произвольного места внутри одного большого файла (по индексу).
 */


using namespace M;
using namespace Moment;

namespace MomentNvr {

mt_mutex (Session::session_mutex) bool
MediaViewer::tryOpenNextFile (Session * const session)
{
//    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session);

    StRef<String> const filename = session->file_iter.getNext ();
    if (!filename) {
        logD_ (_func, "filename is null");
        return false;
    }

    session->session_state = SessionState_FileHeader;

    logD_ (_func, "filename: ", filename);

    {
        StRef<String> const vdat_filename = st_makeString (filename, ".vdat");
        session->vdat_file = vfs->openFile (vdat_filename->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly);
        if (!session->vdat_file) {
            logE_ (_func, "vfs->openFile() failed for filename ",
                   vdat_filename, ": ", exc->toString());
            return false;
        }
    }

    return true;
}

mt_mutex (Session::session_mutex) Result
MediaViewer::readFileHeader (Session * const session)
{
    File * const file = session->vdat_file->getFile();

    FileSize fpos = 0;
    if (!file->tell (&fpos)) {
        logE_ (_func, "tell() failed: ", exc->toString());
        return Result::Failure;
    }

    Byte header [20];
    Size bytes_read = 0;
    IoResult const res = file->readFull (Memory::forObject (header), &bytes_read);
    if (res == IoResult::Error) {
        logE_ (_func, "vdat file read error: ", exc->toString());
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    } else
    if (res != IoResult::Normal) {
        logE_ (_func, "Could not read vdat header");
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    }

    if (bytes_read < sizeof (header)) {
        if (!file->seek (fpos, SeekOrigin::Beg))
            logE_ (_func, "seek() failed: ", exc->toString());
        return Result::Failure;
    }

    if (!equal (ConstMemory (header, 4), "MMNT")) {
        logE_ (_func, "Invalid vdat header: no magic bytes");
        return Result::Failure;
    }

    // TODO Parse format version and header length.

    if (!session->sequence_headers_sent && session->first_frame)
        session->session_state = SessionState_SequenceHeaders;
    else
        session->session_state = SessionState_Frame;

    return Result::Success;
}

mt_mutex (Session::session_mutex) MediaViewer::ReadFrameResult
MediaViewer::readFrame (Session * const mt_nonnull session)
{
    File * const file = session->vdat_file->getFile();

    // TODO Config parameters for burst limits.
    Size const burst_size_limit = 1 << 23 /* 8 Mb */;
    Time const burst_high_mark = 6000000000 /* 6 sec */;

#if 0
    // DEBUG
    if (!session->first_frame) {
        logD_ (_func, "--- DEBUG BREAK");
        return ReadFrameResult_BurstLimit;
    }
#endif

    Size total_read = 0;
    for (;;) {
        if (session->send_blocked.get()) {
            logD_ (_func, "send_blocked");
            return ReadFrameResult_BurstLimit;
        }

        FileSize fpos = 0;
        if (!file->tell (&fpos)) {
            logE_ (_func, "tell() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        Byte frame_header [14];
        Size bytes_read = 0;
        IoResult const res = file->readFull (Memory::forObject (frame_header), &bytes_read);
        if (res == IoResult::Error) {
            logE_ (_func, "vdat file read error: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        if (res != IoResult::Normal) {
            logE_ (_func, "Could not read media header");
            if (!file->seek (fpos, SeekOrigin::Beg))
                logE_ (_func, "seek() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        if (bytes_read < sizeof (frame_header)) {
            if (!file->seek (fpos, SeekOrigin::Beg))
                logE_ (_func, "seek() failed: ", exc->toString());
            return ReadFrameResult_Failure;
        }

        Time const msg_unixtime_ts_nanosec = ((Time) frame_header [0] << 56) |
                                             ((Time) frame_header [1] << 48) |
                                             ((Time) frame_header [2] << 40) |
                                             ((Time) frame_header [3] << 32) |
                                             ((Time) frame_header [4] << 24) |
                                             ((Time) frame_header [5] << 16) |
                                             ((Time) frame_header [6] <<  8) |
                                             ((Time) frame_header [7] <<  0);
//        logD_ (_func, "msg ts: ", msg_unixtime_ts_nanosec);

        Size const msg_ext_len = ((Size) frame_header [ 8] << 24) |
                                 ((Size) frame_header [ 9] << 16) |
                                 ((Size) frame_header [10] <<  8) |
                                 ((Size) frame_header [11] <<  0);
//        logD_ (_func, "msg_ext_len: ", msg_ext_len);

        if (session->session_state == SessionState_SequenceHeaders) {
            if (!(   (frame_header [12] == 2 && frame_header [13] == AudioRecordFrameType::AacSequenceHeader)
                  || (frame_header [12] == 1 && frame_header [13] == VideoRecordFrameType::AvcSequenceHeader)))
            {
                session->session_state = SessionState_Frame;
            }
        }

        if (session->session_state == SessionState_Frame) {
            if (msg_unixtime_ts_nanosec < session->start_unixtime_sec * 1000000000) {
                if (!file->seek (msg_ext_len - 2, SeekOrigin::Cur)) {
                    logE_ (_func, "seek() failed: ", exc->toString());
                    if (!file->seek (fpos, SeekOrigin::Beg))
                        logE_ (_func, "seek() failed: ", exc->toString());
                    return ReadFrameResult_Failure;
                }
                continue;
            }

            if (session->first_frame) {
                session->first_frame = false;
                session->first_frame_ts = msg_unixtime_ts_nanosec;
                session->first_frame_srv_time = getTimeMicroseconds() * 1000;
            }
        }

        if (msg_ext_len <= (1 << 25 /* 32 Mb */)
            && msg_ext_len >= 2)
        {
            PagePool::PageListHead page_list;
            Size const page_size = page_pool->getPageSize ();
            page_pool->getPages (&page_list, msg_ext_len - 2);

            PagePool::Page *cur_page = page_list.first;
            Size msg_read = 0;
            Size page_read = 0;
            for (;;) {
                if (msg_read >= msg_ext_len - 2)
                    break;

                Size nread = 0;
                Size toread = page_size - page_read;
                if (toread > msg_ext_len - 2 - msg_read)
                    toread = msg_ext_len - 2 - msg_read;
                // TODO prechunking
                IoResult const res = file->readFull (Memory (cur_page->getData() + page_read,
                                                             toread),
                                                     &nread);
                if (res == IoResult::Error) {
                    logE_ (_func, "vdat file read error: ", exc->toString());
                    page_pool->msgUnref (page_list.first);
                    return ReadFrameResult_Failure;
                }

                if (res != IoResult::Normal) {
                    logE_ (_func, "Could not read frame");
                    if (!file->seek (fpos, SeekOrigin::Beg))
                        logE_ (_func, "seek() failed: ", exc->toString());
                    page_pool->msgUnref (page_list.first);
                    return ReadFrameResult_Failure;
                }

                page_read += nread;
                msg_read  += nread;

                cur_page->data_len = page_read;

                if (page_read >= page_size) {
                    cur_page = cur_page->getNextMsgPage();
                    page_read = 0;
                }
            }

//            logD_ (_func, "msg_read: ", msg_read);

//            Byte * const hdr = page_list.first->getData();
            Byte * const hdr = frame_header + 12;
            if (hdr [0] == 2) {
              // Audio frame
                VideoStream::AudioFrameType const frame_type = toAudioFrameType (hdr [1]);
                logS_ (_func, "ts ", msg_unixtime_ts_nanosec, " ", frame_type);

                bool skip = false;
                if (frame_type == VideoStream::AudioFrameType::AacSequenceHeader) {
                    if (session->aac_seq_hdr_sent
                        && PagePool::msgEqual (session->aac_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (session->aac_seq_hdr_sent)
                            page_pool->msgUnref (session->aac_seq_hdr.first);

                        session->aac_seq_hdr_sent = true;
                        session->aac_seq_hdr = page_list;
                        page_pool->msgRef (session->aac_seq_hdr.first);
                    }
                }

                if (!skip) {
                    VideoStream::AudioMessage msg;
                    msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                    msg.page_pool = page_pool;
                    msg.page_list = page_list;
                    msg.msg_len = msg_ext_len - 2;
                    msg.msg_offset = 0;
                    // TODO prechunking
                    msg.prechunk_size = 0;

                    msg.codec_id = VideoStream::AudioCodecId::AAC;
                    msg.frame_type = frame_type;

                    session->stream->fireAudioMessage (&msg);
                }
            } else
            if (hdr [0] == 1) {
              // Video frame
                VideoStream::VideoFrameType const frame_type = toVideoFrameType (hdr [1]);
                logS_ (_func, "ts ", msg_unixtime_ts_nanosec, " ", frame_type);

                bool skip = false;
                if (frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
                    if (session->avc_seq_hdr_sent
                        && PagePool::msgEqual (session->avc_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (session->avc_seq_hdr_sent)
                            page_pool->msgUnref (session->avc_seq_hdr.first);

                        session->avc_seq_hdr_sent = true;
                        session->avc_seq_hdr = page_list;
                        page_pool->msgRef (session->avc_seq_hdr.first);
                    }
                }

                if (!skip) {
                    VideoStream::VideoMessage msg;
                    msg.timestamp_nanosec = msg_unixtime_ts_nanosec;

                    msg.page_pool = page_pool;
                    msg.page_list = page_list;
                    msg.msg_len = msg_ext_len - 2;
                    msg.msg_offset = 0;
                    // TODO prechunking
                    msg.prechunk_size = 0;

                    msg.codec_id = VideoStream::VideoCodecId::AVC;
                    msg.frame_type = frame_type;
                    msg.is_saved_frame = false;

                    session->stream->fireVideoMessage (&msg);
                }
            } else {
                logE_ (_func, "unknown frame type: ", hdr [0]);
            }

            page_pool->msgUnref (page_list.first);

            // TODO Handle AVC/AAC codec data
        } else {
            logE_ (_func, "frame is too large (", msg_ext_len, " bytes), skipping");
            if (!file->seek (msg_ext_len - 2, SeekOrigin::Cur)) {
                logE_ (_func, "seek() failed: ", exc->toString());
                if (!file->seek (fpos, SeekOrigin::Beg))
                    logE_ (_func, "seek() failed: ", exc->toString());
                return ReadFrameResult_Failure;
            }
        }

        total_read += msg_ext_len - 2;
        if (total_read >= burst_size_limit)
            return ReadFrameResult_BurstLimit;

        if (session->session_state == SessionState_Frame) {
            if (msg_unixtime_ts_nanosec >= session->first_frame_ts) {
                Time const srv_time = getTimeMicroseconds() * 1000;
                if (srv_time >= session->first_frame_srv_time) {
                    Time const ts_delta = msg_unixtime_ts_nanosec - session->first_frame_ts;
                    Time const srv_delta = srv_time - session->first_frame_srv_time;

                    if (ts_delta >= srv_delta
                        && ts_delta - srv_delta >= burst_high_mark)
                    {
                        return ReadFrameResult_BurstLimit;
                    }
                }
            }
        }
    } // for (;;)

    return ReadFrameResult_Success;
}

mt_mutex (Session::session_mutex) void
MediaViewer::sendMoreData (Session * const session)
{
//    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session);

    if (!session->vdat_file) {
        if (!tryOpenNextFile (session))
            return;
    }

    for (;;) {
        assert (session->vdat_file);

        Result res = Result::Failure;
        switch (session->session_state) {
            case SessionState_FileHeader:
                res = readFileHeader (session);
                break;
            case SessionState_SequenceHeaders:
            case SessionState_Frame:
                ReadFrameResult const rf_res = readFrame (session);
                if (rf_res == ReadFrameResult_BurstLimit) {
                    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session, ": burst limit");
                    return;
                } else
                if (rf_res == ReadFrameResult_Success) {
                    res = Result::Success;
                } else {
                    res = Result::Failure;
                }

                break;
        }

        if (!res) {
            if (!tryOpenNextFile (session))
                break;
        }
    }

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session, ": done");
}

void
MediaViewer::sendTimerTick (void * const _session)
{
//    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session);

    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

    self->sendMoreData (session);
}

VideoStream::EventHandler const MediaViewer::stream_handler = {
    NULL /* audioMessage */,
    NULL /* videoMessage */,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    streamNumWatchersChanged
};

void
MediaViewer::streamNumWatchersChanged (Count   const num_watchers,
                                       void  * const _session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ", "
           "num_watchers: ", fmt_def, num_watchers);

    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

  // TODO unsubscribe and start streaming

    session->session_mutex.lock ();
    if (session->started) {
        return;
    }
    session->started = true;
    assert (session->stream_sbn);
    session->stream->getEventInformer()->unsubscribe (session->stream_sbn);
    session->stream_sbn = NULL;

    self->sendMoreData (session);
    session->send_timer =
            self->timers->addTimer (
                    CbDesc<Timers::TimerCallback> (sendTimerTick, session, session),
                    1    /* time_seconds */,
                    true /* periodical */,
                    true /* auto_delete */);

    session->session_mutex.unlock ();
}

Sender::Frontend const MediaViewer::sender_frontend = {
    senderStateChanged,
    NULL /* closed */
};

mt_mutex (Session::session_mutex) void
MediaViewer::setSendState (Session           * const mt_nonnull session,
                           Sender::SendState   const send_state)
{
    session->send_blocked.set (send_state != Sender::SendState::ConnectionReady ? 1 : 0);
}

void
MediaViewer::senderStateChanged (Sender::SendState   const send_state,
                                 void              * const _session)
{
    Session * const session = static_cast <Session*> (_session);

    session->session_mutex.lock ();
    setSendState (session, send_state);
    session->session_mutex.unlock ();
}

MomentServer::ClientHandler const MediaViewer::client_handler = {
    rtmpClientConnected
};

void
MediaViewer::rtmpClientConnected (MomentServer::ClientSession * const client_session,
                                  ConstMemory   const app_name,
                                  ConstMemory   const full_app_name,
                                  void        * const _self)
{
    MediaViewer * const self = static_cast <MediaViewer*> (_self);

    Ref<Session> const session = grab (new (std::nothrow) Session);
    session->weak_media_viewer = self;
    session->watching = false;
    session->started = false;

    session->sequence_headers_sent = false;

    session->first_frame = true;
    session->first_frame_ts = 0;
    session->first_frame_srv_time = 0;

    session->send_blocked.set (0);

    session->start_unixtime_sec = 0;
    session->session_state = SessionState_FileHeader;
    // TODO Unused
    session->file_opened = false;

    session->aac_seq_hdr_sent = false;
    session->avc_seq_hdr_sent = false;

    logD_ (_func, "session 0x", fmt_hex, (UintPtr) session.ptr(), ", "
           "app_name: ", app_name, ", full_app_name: ", full_app_name);

    self->mutex.lock ();

    CodeDepRef<RtmpConnection> const rtmp_conn = client_session->getRtmpConnection();
    if (rtmp_conn) {
        Sender * const sender = rtmp_conn->getSender();
        sender->lock ();
        setSendState (session, sender->getSendState_unlocked());
        sender->getEventInformer()->subscribe_unlocked (
                CbDesc<Sender::Frontend> (&sender_frontend, session, session));
        sender->unlock ();
    }

    client_session->isConnected_subscribe (
            // IMPORTANT: Note that we use 'session' as ref_data here.
            CbDesc<MomentServer::ClientSession::Events> (&client_session_events, session, session, session));
    client_session->setBackend (
            CbDesc<MomentServer::ClientSession::Backend> (&client_session_backend, session, session));
    self->mutex.unlock ();
}

MomentServer::ClientSession::Events const MediaViewer::client_session_events = {
    rtmpCommandMessage,
    rtmpClientDisconnected
};

void
MediaViewer::rtmpCommandMessage (RtmpConnection       * const mt_nonnull rtmp_conn,
                                 VideoStream::Message * const mt_nonnull msg,
                                 ConstMemory            const method_name,
                                 AmfDecoder           * const mt_nonnull amf_decoder,
                                 void                 * const _session)
{
    logD_ (_func, "session: 0x", fmt_hex, (UintPtr) _session);
}

void
MediaViewer::rtmpClientDisconnected (void * const _session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session);

    Session * const session = static_cast <Session*> (_session);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self)
        return;

    session->session_mutex.lock ();

    if (session->send_timer) {
        self->timers->deleteTimer (session->send_timer);
        session->send_timer = NULL;
    }

    if (session->aac_seq_hdr_sent) {
        session->aac_seq_hdr_sent = false;
        self->page_pool->msgUnref (session->aac_seq_hdr.first);
    }
    if (session->avc_seq_hdr_sent) {
        session->avc_seq_hdr_sent = false;
        self->page_pool->msgUnref (session->avc_seq_hdr.first);
    }

    session->session_mutex.unlock ();

    self->mutex.lock ();
    // TODO Release the session
    self->mutex.unlock ();
}

MomentServer::ClientSession::Backend const MediaViewer::client_session_backend = {
    rtmpStartWatching,
    rtmpStartStreaming
};

void
MediaViewer::parseStreamParams_paramCallback (ConstMemory   const name,
                                              ConstMemory   const value,
                                              void        * const _stream_params)
{
    StreamParams * const stream_params = static_cast <StreamParams*> (_stream_params);

    logD_ (_func, "name: ", name, ", value: ", value);

    if (equal (name, "start")) {
        Time start_unixtime_sec = 0;
        if (strToUint64_safe (value, &start_unixtime_sec, 10 /* base */)) {
            stream_params->start_unixtime_sec = start_unixtime_sec;
        } else {
            logE_ (_func, "bad \"start\" stream param: ", value);
        }
    }
}

void
MediaViewer::parseStreamParams (ConstMemory    const stream_name_with_params,
                                StreamParams * const mt_nonnull stream_params)
{
    ConstMemory const stream_name = stream_name_with_params;

    Byte const * const name_sep = (Byte const *) memchr (stream_name.mem(), '?', stream_name.len());
    if (name_sep) {
        ConstMemory const params_mem = stream_name.region (name_sep + 1 - stream_name.mem());
        logD_ (_func, "parameters: ", params_mem);
        parseHttpParameters (params_mem,
                             parseStreamParams_paramCallback,
                             stream_params);
    }
}

bool
MediaViewer::rtmpStartWatching (ConstMemory        const stream_name,
                                ConstMemory        const stream_name_with_params,
                                IpAddress          const /* client_addr */,
                                CbDesc<MomentServer::StartWatchingCallback> const & /* cb */,
                                Ref<VideoStream> * const mt_nonnull ret_stream,
                                void             * const _session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ", stream_name: ", stream_name_with_params);

    Session * const session = static_cast <Session*> (_session);

    Ref<VideoStream> const stream = grab (new (std::nothrow) VideoStream);
    Ref<MediaViewer> const self = session->weak_media_viewer.getRef ();
    if (!self) {
        *ret_stream = NULL;
        return true;
    }

    StreamParams stream_params;
    parseStreamParams (stream_name_with_params, &stream_params);
    logD_ (_func, "start_unixtime_sec: ", stream_params.start_unixtime_sec, ", getUnixtime(): ", getUnixtime());

    session->session_mutex.lock ();
    if (session->watching) {
        session->session_mutex.unlock ();
        logE_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ": already watching");
        *ret_stream = NULL;
        return true;
    }
    session->watching = true;

    session->stream = stream;
    session->stream_name = st_grab (new (std::nothrow) String (stream_name));

    session->start_unixtime_sec = stream_params.start_unixtime_sec;
    session->stream_sbn = stream->getEventInformer()->subscribe (
            CbDesc<VideoStream::EventHandler> (&stream_handler, session, session));

    session->file_iter.init (self->vfs, session->stream_name->mem(), session->start_unixtime_sec);

    session->session_mutex.unlock ();

    *ret_stream = stream;
    return true;
}

bool
MediaViewer::rtmpStartStreaming (ConstMemory     const stream_name,
                                 IpAddress       const client_addr,
                                 VideoStream   * const mt_nonnull stream,
                                 RecordingMode   const rec_mode,
                                 CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                 Result        * const mt_nonnull ret_res,
                                 void          * const _session)
{
    logD_ (_func, "session 0x", fmt_hex, (UintPtr) _session, ", stream_name: ", stream_name);
    *ret_res = Result::Failure;
    return true;
}

void
MediaViewer::init (MomentServer * const mt_nonnull moment,
                   Vfs          * const mt_nonnull vfs)
{
    this->vfs = vfs;

    page_pool = moment->getPagePool();

    // TODO Use timers from multiple threads for sendDataTimerTick().
    timers = moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers();

    moment->addClientHandler (CbDesc<MomentServer::ClientHandler> (&client_handler, this, this),
                              "nvr");
}

MediaViewer::MediaViewer ()
    : page_pool (this /* coderef_container */),
      timers    (this /* coderef_container */)
{
}

}

