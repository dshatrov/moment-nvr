#ifndef MOMENT_NVR__MOMENT_NVR_MODULE__H__
#define MOMENT_NVR__MOMENT_NVR_MODULE__H__


#include <moment/libmoment.h>

#include <moment-nvr/channel_recorder.h>
#include <moment-nvr/media_viewer.h>
#include <moment-nvr/get_file_session.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MomentNvrModule : public Object
{
private:
    StateMutex mutex;

    mt_const Ref<MomentServer>    moment;
    mt_const Ref<ChannelRecorder> channel_recorder;
    mt_const Ref<MediaViewer>     media_viewer;
    mt_const DataDepRef<PagePool> page_pool;

    mt_const StRef<String> record_dir;

    mt_mutex (mutex) List< Ref<GetFileSession> > get_file_sessions;

    static StRef<String> channelStateToJson (ChannelRecorder::ChannelState * mt_nonnull channel_state,
                                             ConstMemory seq);

  mt_iface (GetFileSession::Frontend)
    static GetFileSession::Frontend const get_file_session_frontend;

    static void getFileSession_done (Result  res,
                                     void   *_self);
  mt_iface_end

    struct Frontend {
        void (*done) (Result  res,
                      void   *cb_data);
    };


    void doGetFile (HttpRequest * mt_nonnull req,
                    Sender      * mt_nonnull sender,
                    ConstMemory  stream_name,
                    Time         start_unixtime_sec,
                    Time         duration_sec,
                    bool         download);

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result httpRequest (HttpRequest  * mt_nonnull req,
                               Sender       * mt_nonnull conn_sender,
                               Memory const &msg_body,
                               void        ** mt_nonnull ret_msg_data,
                               void         *_self);
  mt_iface_end

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const admin_http_handler;

    static Result adminHttpRequest (HttpRequest  * mt_nonnull req,
                                    Sender       * mt_nonnull conn_sender,
                                    Memory const &msg_body,
                                    void        ** mt_nonnull ret_msg_data,
                                    void         *_self);
  mt_iface_end

public:
    mt_const void init (MomentServer * mt_nonnull moment);

    MomentNvrModule ();
};

}


#endif /* MOMENT_NVR__MOMENT_NVR_MODULE__H__ */

