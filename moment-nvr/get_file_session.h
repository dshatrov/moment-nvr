#ifndef MOMENT_NVR__GET_FILE_SESSION__H__
#define MOMENT_NVR__GET_FILE_SESSION__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_reader.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class GetFileSession : public Object
{
private:
    StateMutex mutex;

public:
    struct Frontend {
        void (*done) (Result  res,
                      void   *cb_data);
    };

private:
    enum SessionState {
        SessionState_PassOne,
        SessionState_PassTwo
    };

    mt_const Ref<MomentServer> moment;
    mt_const DataDepRef<Sender> sender;
    mt_const Cb<Frontend> frontend;

    mt_const Time duration_sec;

    mt_const DataDepRef<ServerThreadContext> thread_ctx;
    mt_const ServerThreadContext *reader_thread_ctx;

    MediaReader media_reader;
    mt_sync_domain (readTask) Mp4Muxer mp4_muxer;

    mt_sync_domain (readTask) SessionState session_state;

    DeferredProcessor::Task read_task;
    DeferredProcessor::Registration deferred_reg;

    mt_mutex (mutex) bool started;

  mt_iface (Mp4Muxer::Frontend)
    static Mp4Muxer::Frontend const mp4_muxer_frontend;

    static void muxerStateChanged (Mp4Muxer::MuxState  mux_state,
                                   void               *_self);
  mt_iface_end

  mt_iface (MediaReader::ReadFrameBackend)
    static MediaReader::ReadFrameBackend const read_frame_backend;

    mt_sync_domain (readTask) static
            MediaReader::ReadFrameResult audioFrame (VideoStream::AudioMessage * mt_nonnull msg,
                                                     void                      *_self);

    mt_sync_domain (readTask) static
            MediaReader::ReadFrameResult videoFrame (VideoStream::VideoMessage * mt_nonnull msg,
                                                     void                      *_self);
  mt_iface_end

    static bool readTask (void * const _self);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_self);

    static void senderClosed (Exception *exc,
                              void      *_self);
  mt_iface_end 

public:
    void start ();

    mt_const void init (MomentServer * const mt_nonnull moment,
                        HttpRequest  * const mt_nonnull req,
                        Sender       * const mt_nonnull sender,
                        PagePool     * const page_pool,
                        Vfs          * const vfs,
                        ConstMemory    const stream_name,
                        Time           const start_unixtime_sec,
                        Time           const duration_sec,
                        CbDesc<Frontend> const &frontend);

     GetFileSession ();
    ~GetFileSession ();
};

}


#endif /* MOMENT_NVR__GET_FILE_SESSION__H__ */
