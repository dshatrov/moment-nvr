#include <moment-nvr/media_reader.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

mt_mutex (mutex) bool
MediaReader::tryOpenNextFile ()
{
    StRef<String> const filename = file_iter.getNext ();
    if (!filename) {
        logD_ (_func, "filename is null");
        return false;
    }

    session_state = SessionState_FileHeader;

    logD_ (_func, "filename: ", filename);

    {
        StRef<String> const vdat_filename = st_makeString (filename, ".vdat");
        vdat_file = vfs->openFile (vdat_filename->mem(), 0 /* open_flags */, FileAccessMode::ReadOnly);
        if (!vdat_file) {
            logE_ (_func, "vfs->openFile() failed for filename ",
                   vdat_filename, ": ", exc->toString());
            return false;
        }
    }

    return true;
}

mt_mutex (mutex) Result
MediaReader::readFileHeader ()
{
    File * const file = vdat_file->getFile();

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

    if (!sequence_headers_sent && first_frame)
        session_state = SessionState_SequenceHeaders;
    else
        session_state = SessionState_Frame;

    return Result::Success;
}

mt_mutex (mutex) MediaReader::ReadFrameResult
MediaReader::readFrame (ReadFrameCallback   const read_frame_cb,
                        void              * const read_frame_cb_data)
{
    File * const file = vdat_file->getFile();

    // TODO Config parameters for burst limits.
    Size const burst_size_limit = 1 << 23 /* 8 Mb */;
    Time const burst_high_mark = 6000000000 /* 6 sec */;

    Size total_read = 0;
    for (;;) {
        if (send_blocked.get()) {
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

        if (session_state == SessionState_SequenceHeaders) {
            if (!(   (frame_header [12] == 2 && frame_header [13] == AudioRecordFrameType::AacSequenceHeader)
                  || (frame_header [12] == 1 && frame_header [13] == VideoRecordFrameType::AvcSequenceHeader)))
            {
                session_state = SessionState_Frame;
            }
        }

        if (session_state == SessionState_Frame) {
            if (msg_unixtime_ts_nanosec < start_unixtime_sec * 1000000000) {
                if (!file->seek (msg_ext_len - 2, SeekOrigin::Cur)) {
                    logE_ (_func, "seek() failed: ", exc->toString());
                    if (!file->seek (fpos, SeekOrigin::Beg))
                        logE_ (_func, "seek() failed: ", exc->toString());
                    return ReadFrameResult_Failure;
                }
                continue;
            }

            if (first_frame) {
                first_frame = false;
#if 0
// TIMESTAMPS
                session->first_frame_ts = msg_unixtime_ts_nanosec;
                session->first_frame_srv_time = getTimeMicroseconds() * 1000;
#endif
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
                    if (aac_seq_hdr_sent
                        && PagePool::msgEqual (aac_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (aac_seq_hdr_sent)
                            page_pool->msgUnref (aac_seq_hdr.first);

                        aac_seq_hdr_sent = true;
                        aac_seq_hdr = page_list;
                        page_pool->msgRef (aac_seq_hdr.first);
                    }
                }

                if (!skip) {
#if 0
// CALLBACK
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
#endif
                }
            } else
            if (hdr [0] == 1) {
              // Video frame
                VideoStream::VideoFrameType const frame_type = toVideoFrameType (hdr [1]);
                logS_ (_func, "ts ", msg_unixtime_ts_nanosec, " ", frame_type);

                bool skip = false;
                if (frame_type == VideoStream::VideoFrameType::AvcSequenceHeader) {
                    if (avc_seq_hdr_sent
                        && PagePool::msgEqual (avc_seq_hdr.first, page_list.first))
                    {
                        skip = true;
                    } else {
                        if (avc_seq_hdr_sent)
                            page_pool->msgUnref (avc_seq_hdr.first);

                        avc_seq_hdr_sent = true;
                        avc_seq_hdr = page_list;
                        page_pool->msgRef (avc_seq_hdr.first);
                    }
                }

                if (!skip) {
#if 0
// CALLBACK
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
#endif
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

#if 0
// TIMESTAMPS
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
#endif
    } // for (;;)

    return ReadFrameResult_Success;
}

MediaReader::ReadFrameResult
MediaReader::readMoreData (ReadFrameCallback   const read_frame_cb,
                           void              * const read_frame_cb_data)
{
    if (!vdat_file) {
        if (!tryOpenNextFile ())
            // TODO NoDataYet return code.
            return ReadFrameResult_Success;
    }

    for (;;) {
        assert (vdat_file);

        Result res = Result::Failure;
        switch (session_state) {
            case SessionState_FileHeader:
                res = readFileHeader ();
                break;
            case SessionState_SequenceHeaders:
            case SessionState_Frame:
                ReadFrameResult const rf_res = readFrame (read_frame_cb, read_frame_cb_data);
                if (rf_res == ReadFrameResult_BurstLimit) {
                    logD_ (_func, "session 0x", fmt_hex, (UintPtr) this, ": burst limit");
                    return ReadFrameResult_BurstLimit;
                } else
                if (rf_res == ReadFrameResult_Success) {
                    res = Result::Success;
                } else {
                    res = Result::Failure;
                }

                break;
        }

        if (!res) {
            if (!tryOpenNextFile ()) {
                // TODO NoDataYet return code.
                break;
            }
        }
    }

    return ReadFrameResult_Success;
}

mt_const void
MediaReader::init (PagePool * const mt_nonnull page_pool,
                   Vfs      * const mt_nonnull vfs)
{
    this->page_pool = page_pool;
    this->vfs = vfs;
}

}

