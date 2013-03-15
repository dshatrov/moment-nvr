#ifndef MOMENT_NVR__MOMENT_NVR_MODULE__H__
#define MOMENT_NVR__MOMENT_NVR_MODULE__H__


#include <moment/libmoment.h>

#include <moment-nvr/channel_recorder.h>
#include <moment-nvr/media_viewer.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class MomentNvrModule : public Object
{
private:
    mt_const Ref<ChannelRecorder> channel_recorder;
    mt_const Ref<MediaViewer>     media_viewer;
    mt_const DataDepRef<PagePool> page_pool;

    void doGetFile (HttpRequest * mt_nonnull req,
                    Sender      * mt_nonnull sender,
                    ConstMemory  stream_name,
                    Time         start_unixtime_sec,
                    Time         duration_sec);

  mt_iface (HttpService::HttpHandler)
    static HttpService::HttpHandler const http_handler;

    static Result httpRequest (HttpRequest  * mt_nonnull req,
                               Sender       * mt_nonnull conn_sender,
                               Memory const &msg_body,
                               void        ** mt_nonnull ret_msg_data,
                               void         *_self);
  mt_iface_end

public:
    void init (MomentServer * mt_nonnull moment);

    MomentNvrModule ();
};

}


#endif /* MOMENT_NVR__MOMENT_NVR_MODULE__H__ */

