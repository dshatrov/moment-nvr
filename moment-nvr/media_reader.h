#ifndef MOMENT_NVR__MEDIA_READER__H__
#define MOMENT_NVR__MEDIA_READER__H__


#include <moment-nvr/types.h>
#include <moment-nvr/nvr_file_iterator.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MediaReader : public DependentCodeReferenced
{
private:
    StateMutex mutex;

public:
    enum ReadFrameResult
    {
        ReadFrameResult_Success,
        ReadFrameResult_Finish,
        ReadFrameResult_BurstLimit,
        ReadFrameResult_NoData,
        ReadFrameResult_Failure
    };

    struct ReadFrameBackend
    {
        ReadFrameResult (*audioFrame) (VideoStream::AudioMessage * mt_nonnull msg,
                                       void                      *cb_data);

        ReadFrameResult (*videoFrame) (VideoStream::VideoMessage * mt_nonnull msg,
                                       void                      *cb_data);
    };

private:
    enum SessionState {
        SessionState_FileHeader,
        SessionState_SequenceHeaders,
        SessionState_Frame
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const Ref<Vfs> vfs;

    mt_const Time start_unixtime_sec;
    // 0 means no limit
    mt_const Size burst_size_limit;

    mt_mutex (mutex) SessionState session_state;
    mt_mutex (mutex) NvrFileIterator file_iter;

    mt_mutex (mutex) Ref<Vfs::VfsFile> vdat_file;

    mt_mutex (mutex) bool sequence_headers_sent;
    mt_mutex (mutex) bool first_frame;

    mt_mutex (mutex) bool aac_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead aac_seq_hdr;

    mt_mutex (mutex) bool avc_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead avc_seq_hdr;

    mt_mutex (mutex) void releaseSequenceHeaders_unlocked ();

    mt_mutex (mutex) void reset_unlocked ()
    {
        session_state = SessionState_FileHeader;
        releaseSequenceHeaders_unlocked ();
        sequence_headers_sent = false;
        first_frame = true;
        file_iter.reset (start_unixtime_sec);
        vdat_file = NULL;
    }

    mt_mutex (mutex) bool tryOpenNextFile  ();
    mt_mutex (mutex) Result readFileHeader ();

    mt_mutex (mutex) ReadFrameResult readFrame (ReadFrameBackend const *read_frame_cb,
                                                void                   *read_frame_cb_data);

public:
    ReadFrameResult readMoreData (ReadFrameBackend const *read_frame_cb,
                                  void                   *read_frame_cb_data);

    void reset ()
    {
        mutex.lock ();
        reset_unlocked ();
        mutex.unlock ();
    }

    mt_const void init (PagePool    * mt_nonnull page_pool,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name,
                        Time         start_unixtime_sec,
                        Size         burst_size_limit);

    MediaReader (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool             (coderef_container),
          start_unixtime_sec    (0),
          burst_size_limit      (0),
          session_state         (SessionState_FileHeader),
          sequence_headers_sent (false),
          first_frame           (true),
          aac_seq_hdr_sent      (false),
          avc_seq_hdr_sent      (false)
    {}

    ~MediaReader ();
};

}


#endif /* MOMENT_NVR__MEDIA_READER__H__ */

