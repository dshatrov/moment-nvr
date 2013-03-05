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
    Ref<ChannelRecorder> channel_recorder;
    Ref<MediaViewer>     media_viewer;

public:
    void init (MomentServer * mt_nonnull moment);
};

}


#endif /* MOMENT_NVR__MOMENT_NVR_MODULE__H__ */

