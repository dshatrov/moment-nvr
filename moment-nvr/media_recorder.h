#ifndef MOMENT_NVR__MEDIA_RECORDER__H__
#define MOMENT_NVR__MEDIA_RECORDER__H__


#include <moment/libmoment.h>

#include <moment-nvr/types.h>
#include <moment-nvr/naming_scheme.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MediaRecorder : public Object
{
private:
    StateMutex mutex;

private:
    struct Recording : public Object
    {
        WeakDepRef<MediaRecorder> weak_media_recorder;

        DeferredConnectionSender vdat_sender;
        mt_mutex (MediaRecorder::mutex) Ref<Vfs::VfsFile> vdat_file;
        FileConnection vdat_conn;

        DeferredConnectionSender idx_sender;
        mt_mutex (MediaRecorder::mutex) Ref<Vfs::VfsFile> idx_file;
        FileConnection idx_conn;

        mt_mutex (MediaRecorder::mutex) Size cur_data_offset;

        Recording ()
            : vdat_sender (this /* coderef_container */),
              idx_sender  (this /* coderef_container */),
              cur_data_offset (0)
        {}
    };

    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<ServerThreadContext> thread_ctx;
    mt_const Ref<Vfs> vfs;

    mt_const Ref<NamingScheme> naming_scheme;
    mt_const StRef<String> channel_name;

    struct StreamTicket : public Referenced
        { MediaRecorder *media_recorder; };

    mt_mutex (mutex) Ref<StreamTicket> cur_stream_ticket;
    mt_mutex (mutex) Ref<VideoStream> cur_stream;
    mt_mutex (mutex) GenericInformer::SubscriptionKey stream_sbn;
    mt_mutex (mutex) bool got_unixtime_offset;
    mt_mutex (mutex) Time unixtime_offset_nanosec;
    mt_mutex (mutex) Time prv_unixtime_timestamp_nanosec;

    bool got_pending_aac_seq_hdr;
    mt_mutex (mutex) VideoStream::AudioMessage pending_aac_seq_hdr;

    bool got_pending_avc_seq_hdr;
    mt_mutex (mutex) VideoStream::VideoMessage pending_avc_seq_hdr;

    mt_mutex (mutex) Time next_idx_unixtime_nanosec;

    mt_mutex (mutex) Ref<Recording> recording;

    mt_mutex (mutex) Time next_file_unixtime_nanosec;

    mt_mutex (mutex) void recordStreamHeaders ();

    mt_mutex (mutex) void recordMessage (VideoStream::Message * mt_nonnull msg,
                                         bool                  is_audio_msg,
                                         ConstMemory           header);

    mt_mutex (mutex) void doRecordMessage (VideoStream::Message * mt_nonnull msg,
                                           ConstMemory           header,
                                           Time                  unixtime_timestamp_nanosec);

    mt_mutex (mutex) Result openVdatFile (ConstMemory _filename,
                                          Time        start_unixtime_nanosec);

    mt_mutex (mutex) Result openIdxFile  (ConstMemory _filename);

    mt_mutex (mutex) Result doStartRecording (Time cur_unixtime_nanosec);

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_recording);

    static void senderClosed (Exception *_exc,
                              void      *_recording);
  mt_iface_end

  mt_iface (VideoStream::EventHandler)
    static VideoStream::EventHandler const stream_event_handler;

    static void streamAudioMessage (VideoStream::AudioMessage * mt_nonnull audio_msg,
                                    void *_stream_ticket);

    static void streamVideoMessage (VideoStream::VideoMessage * mt_nonnull video_msg,
                                    void *_stream_ticket);

    static void streamClosed (void *_stream_ticket);
  mt_iface_end

public:
    void setVideoStream (VideoStream *stream);

    void startRecording ();
    void stopRecording  ();

    void init (PagePool            * mt_nonnull page_pool,
               ServerThreadContext * mt_nonnull thread_ctx,
               Vfs                 * mt_nonnull vfs,
               NamingScheme        * mt_nonnull naming_scheme,
               ConstMemory          channel_name);

     MediaRecorder ();
    ~MediaRecorder ();
};

}


#endif /* MOMENT_NVR__MEDIA_RECORDER__H__ */

