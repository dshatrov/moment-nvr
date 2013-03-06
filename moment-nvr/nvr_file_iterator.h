#ifndef MOMENT_NVR__NVR_FILE_ITERATOR__H__
#define MOMENT_NVR__NVR_FILE_ITERATOR__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

mt_unsafe class NvrFileIterator
{
private:
    Ref<Vfs> vfs;
    StRef<String> stream_name;

    bool got_first;
    // year/month/day/hour/minute/seconds
    unsigned cur_pos [6];

    static StRef<String> makePathForDepth (ConstMemory  stream_name,
                                           unsigned     depth,
                                           unsigned    * mt_nonnull pos);

    StRef<String> getNext_rec (Vfs::VfsDirectory * mt_nonnull parent_dir,
                               ConstMemory        parent_dir_name,
                               unsigned           depth,
                               bool               parent_pos_match);

public:
    StRef<String> getNext ();

    void init (Vfs         * mt_nonnull vfs,
               ConstMemory  stream_name,
               Time         start_unixtime_sec);

    NvrFileIterator ()
        : got_first (false)
    {}
};

}


#endif /* MOMENT_NVR__NVR_FILE_ITERATOR__H__ */

