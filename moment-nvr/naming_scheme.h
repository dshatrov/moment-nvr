#ifndef MOMENT_NVR__NAMING_SCHEME__H__
#define MOMENT_NVR__NAMING_SCHEME__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

struct NamingScheme : public Referenced
{
    virtual StRef<String> getPath (ConstMemory  channel_name,
                                   Time         unixtime_sec,
                                   Time        *ret_next_unixtime_sec) = 0;
};

class DefaultNamingScheme : public NamingScheme
{
private:
    Time          const file_duration_sec;

public:
    StRef<String> getPath (ConstMemory  channel_name,
                           Time         unixtime_sec,
                           Time        *ret_next_unixtime_sec);

    DefaultNamingScheme (Time const file_duration_sec)
        : file_duration_sec (file_duration_sec)
    {}
};

}


#endif /* MOMENT_NVR__NAMING_SCHEME__H__ */

