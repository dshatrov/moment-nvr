#ifndef MOMENT_NVR__NVR_CLEANER__H__
#define MOMENT_NVR__NVR_CLEANER__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class NvrCleaner : public Object
{
private:
    mt_const Ref<Vfs> vfs;
    mt_const StRef<String> stream_name;
    mt_const Time max_age_sec;

    static void cleanupTimerTick (void *_self);

public:
    mt_const void init (Timers      * mt_nonnull timers,
                        Vfs         * mt_nonnull vfs,
                        ConstMemory  stream_name,
                        Time         max_age_sec);
};

}


#endif /* MOMENT_NVR__NVR_CLEANER__H__ */

