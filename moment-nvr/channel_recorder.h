#ifndef MOMENT_NVR__CHANNEL_RECORDER__H__
#define MOMENT_NVR__CHANNEL_RECORDER__H__


#include <moment/libmoment.h>

#include <moment-nvr/media_recorder.h>
#include <moment-nvr/nvr_cleaner.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class ChannelRecorder : public Object
{
private:
    StateMutex mutex;

    class ChannelEntry : public Object
    {
    public:
        mt_const WeakRef<ChannelRecorder> weak_channel_recorder;

        mt_const CodeDepRef<ServerThreadContext> thread_ctx;
        mt_const ServerThreadContext *recorder_thread_ctx;

        mt_const Ref<Channel> channel;
        mt_const StRef<String> channel_name;

        mt_const Ref<MediaRecorder> media_recorder;
        mt_const Ref<NvrCleaner> nvr_cleaner;

        mt_const GenericStringHash::EntryKey hash_entry_key;

        mt_mutex (ChannelRecorder::mutex) bool valid;
    };

    mt_const Ref<MomentServer>   moment;
    mt_const Ref<ChannelManager> channel_manager;
    mt_const Ref<Vfs>            vfs;
    mt_const Ref<NamingScheme>   naming_scheme;

    typedef StringHash< Ref<ChannelEntry> > ChannelHash;
    mt_mutex (mutex) ChannelHash channel_hash;

  mt_iface (Channel::ChannelEvents)
    static Channel::ChannelEvents const channel_events;

    static void channelStartItem      (VideoStream *stream,
                                       bool         stream_changed,
                                       void        *_channel_entry);

    static void channelStopItem       (VideoStream *stream,
                                       bool         stream_changed,
                                       void        *_channel_entry);

    static void channelNewVideoStream (VideoStream *stream,
                                       void        *_channel_entry);

    static void channelDestroyed      (void *_channel_entry);
  mt_iface_end

  mt_iface (ChannelManager::Events)
    static ChannelManager::Events const channel_manager_events;

    static void channelCreated (ChannelManager::ChannelInfo * mt_nonnull channel_info,
                                void *_self);
  mt_iface_end

    void doCreateChannel (ChannelManager::ChannelInfo * mt_nonnull channel_info);

    mt_mutex (mutex) void doDestroyChannel (ChannelEntry * mt_nonnull channel_entry);

public:
    mt_const void init (MomentServer * mt_nonnull moment,
                        Vfs          * mt_nonnull vfs,
                        NamingScheme * mt_nonnull naming_scheme);

     ChannelRecorder ();
    ~ChannelRecorder ();
};

}


#endif /* MOMENT_NVR__CHANNEL_RECORDER__H__ */

