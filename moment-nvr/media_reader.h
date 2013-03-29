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
        ReadFrameResult_BurstLimit,
        ReadFrameResult_Failure
    };

    typedef ReadFrameResult (*ReadFrameCallback) (/* TODO Send params */);

private:
    enum SessionState {
        SessionState_FileHeader,
        SessionState_SequenceHeaders,
        SessionState_Frame
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const Ref<Vfs> vfs;

    mt_const Time start_unixtime_sec;

    mt_mutex (mutex) SessionState session_state;
    mt_mutex (mutex) NvrFileIterator file_iter;

    mt_mutex (mutex) Ref<Vfs::VfsFile> vdat_file;

    mt_mutex (mutex) bool sequence_headers_sent;
    mt_mutex (mutex) bool first_frame;

    mt_mutex (mutex) bool aac_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead aac_seq_hdr;

    mt_mutex (mutex) bool avc_seq_hdr_sent;
    mt_mutex (mutex) PagePool::PageListHead avc_seq_hdr;

    AtomicInt send_blocked;

    mt_mutex (mutex) bool tryOpenNextFile  ();
    mt_mutex (mutex) Result readFileHeader ();

    mt_mutex (mutex) ReadFrameResult readFrame (ReadFrameCallback  read_frame_cb,
                                                void              *read_frame_cb_data);

public:
    ReadFrameResult readMoreData (ReadFrameCallback  read_frame_cb,
                                  void              *read_frame_cb_data);

    mt_const void init (PagePool * mt_nonnull page_pool,
                        Vfs      * mt_nonnull vfs);

    MediaReader (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool (coderef_container)
    {}
};

}


#endif /* MOMENT_NVR__MEDIA_READER__H__ */

