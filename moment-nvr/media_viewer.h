#ifndef MOMENT_NVR__MEDIA_VIEWER__H__
#define MOMENT_NVR__MEDIA_VIEWER__H__


#include <moment/libmoment.h>

#include <moment-nvr/naming_scheme.h>
#include <moment-nvr/nvr_file_iterator.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MediaViewer : public Object
{
private:
    StateMutex mutex;

    enum SessionState {
        SessionState_FileHeader,
        SessionState_Frame
    };

    class Session : public Object
    {
    public:
        StateMutex session_mutex;

        mt_const WeakRef<MediaViewer> weak_media_viewer;

        mt_const Ref<Vfs::VfsFile> vdat_file;
        mt_const Ref<VideoStream> stream;

        mt_mutex (session_mutex) bool watching;
        mt_mutex (session_mutex) StRef<String> stream_name;

        mt_mutex (session_mutex) bool started;
        mt_mutex (session_mutex) GenericInformer::SubscriptionKey stream_sbn;

        mt_mutex (session_mutex) Timers::TimerKey send_timer;

        mt_mutex (session_mutex) Time start_unixtime_sec;

        mt_mutex (session_mutex) SessionState session_state;

        mt_mutex (session_mutex) NvrFileIterator file_iter;
        mt_mutex (session_mutex) bool file_opened;

        mt_mutex (session_mutex) bool aac_seq_hdr_sent;
        mt_mutex (session_mutex) PagePool::PageListHead aac_seq_hdr;

        mt_mutex (session_mutex) bool avc_seq_hdr_sent;
        mt_mutex (session_mutex) PagePool::PageListHead avc_seq_hdr;
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Timers> timers;

    mt_const Ref<Vfs>          vfs;
    mt_const Ref<NamingScheme> naming_scheme;

    mt_mutex (Session::session_mutex) bool tryOpenNextFile (Session *session);
    mt_mutex (Session::session_mutex) void readFileHeader  (Session *session);
    mt_mutex (Session::session_mutex) void readFrame       (Session *session);
    mt_mutex (Session::session_mutex) void sendMoreData    (Session *session);

    static void sendTimerTick (void *_session);

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const stream_handler;

    static void streamNumWatchersChanged (Count  num_watchers,
                                          void  *_session);
  mt_iface_end

  mt_iface (MomentServer::ClientHandler)
    static MomentServer::ClientHandler const client_handler;

    static void rtmpClientConnected (MomentServer::ClientSession *client_session,
                                     ConstMemory  app_name,
                                     ConstMemory  full_app_name,
                                     void        *_self);
  mt_iface_end

  mt_iface (MomentServer::ClientSession::Events)
    static MomentServer::ClientSession::Events const client_session_events;

    static void rtmpCommandMessage (RtmpConnection       * mt_nonnull rtmp_conn,
                                    VideoStream::Message * mt_nonnull msg,
                                    ConstMemory           method_name,
                                    AmfDecoder           * mt_nonnull amf_decoder,
                                    void                 *_session);

    static void rtmpClientDisconnected (void *_session);
  mt_iface_end

    struct StreamParams
    {
        Time start_unixtime_sec;

        StreamParams ()
            : start_unixtime_sec (0)
        {}
    };

    static void parseStreamParams_paramCallback (ConstMemory  name,
                                                 ConstMemory  value,
                                                 void        *_stream_params);

    static void parseStreamParams (ConstMemory   stream_name_with_params,
                                   StreamParams * mt_nonnull stream_params);

  mt_iface (MomentServer::ClientSession::Backend)
    static MomentServer::ClientSession::Backend const client_session_backend;

    static bool rtmpStartWatching (ConstMemory       stream_name,
                                   ConstMemory       stream_name_with_params,
                                   IpAddress         client_addr,
                                   CbDesc<MomentServer::StartWatchingCallback> const &cb,
                                   Ref<VideoStream> * mt_nonnull ret_stream,
                                   void             *_session);

    static bool rtmpStartStreaming (ConstMemory    stream_name,
                                    IpAddress      client_addr,
                                    VideoStream   * mt_nonnull stream,
                                    RecordingMode  rec_mode,
                                    CbDesc<MomentServer::StartStreamingCallback> const &cb,
                                    Result        * mt_nonnull ret_res,
                                    void          *_session);
  mt_iface_end

public:
    void init (MomentServer * mt_nonnull moment,
               Vfs          * mt_nonnull vfs,
               NamingScheme * mt_nonnull naming_scheme);

    MediaViewer ();
};

}


#endif /* MOMENT_NVR__MEDIA_VIEWER__H__ */

