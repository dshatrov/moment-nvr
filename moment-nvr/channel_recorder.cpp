#include <moment-nvr/channel_recorder.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

Channel::ChannelEvents const ChannelRecorder::channel_events = {
    channelStartItem,
    channelStopItem,
    channelNewVideoStream,
    channelDestroyed
};

void
ChannelRecorder::channelStartItem (VideoStream * const stream,
                                   bool          const stream_changed,
                                   void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    logD_ (_func_);
    if (stream_changed)
        channel_entry->media_recorder.setVideoStream (stream);
}

void
ChannelRecorder::channelStopItem (VideoStream * const stream,
                                  bool          const stream_changed,
                                  void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    logD_ (_func_);
    if (stream_changed)
        channel_entry->media_recorder.setVideoStream (stream);
}

void
ChannelRecorder::channelNewVideoStream (VideoStream * const stream,
                                        void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    logD_ (_func_);
    channel_entry->media_recorder.setVideoStream (stream);
}

void
ChannelRecorder::channelDestroyed (void * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    Ref<ChannelRecorder> const self = channel_entry->weak_channel_recorder.getRef ();
    if (!self)
        return;

    self->doDestroyChannel (channel_entry);
}

ChannelManager::Events const ChannelRecorder::channel_manager_events = {
    channelCreated
};

void
ChannelRecorder::channelCreated (ChannelManager::ChannelInfo * const mt_nonnull channel_info,
                                 void * const _self)
{
    ChannelRecorder * const self = static_cast <ChannelRecorder*> (_self);

    logD_ (_func, "channel_name: ", channel_info->channel_name);
    self->doCreateChannel (channel_info);
}

// May be called with 'channel_manager' locked.
void
ChannelRecorder::doCreateChannel (ChannelManager::ChannelInfo * const mt_nonnull channel_info)
{
    logD_ (_func, "channel_name: ", channel_info->channel_name);

    Channel * const channel = channel_info->channel;

    Ref<ChannelEntry> const channel_entry = grab (new (std::nothrow) ChannelEntry);
    channel_entry->weak_channel_recorder = this;
    // TODO release thread_ctx
    channel_entry->thread_ctx = moment->getRecorderThreadPool()->grabThreadContext (ConstMemory() /* TODO filename */);
    channel_entry->channel = channel;
    channel_entry->channel_name = st_grab (new (std::nothrow) String (channel_info->channel_name));
    channel_entry->media_recorder.init (moment->getPagePool (),
                                        channel_entry->thread_ctx,
                                        vfs,
                                        naming_scheme,
                                        channel_info->channel_name);

    mutex.lock ();
#warning TODO Deal with duplicate channel names.

    channel_hash.add (channel_info->channel_name, channel_entry);
    mutex.unlock ();

    {
        channel->channelLock ();
        if (channel->isDestroyed_unlocked()) {
            channel->channelUnlock ();
            doDestroyChannel (channel_entry);
        } else {
            channel_entry->media_recorder.setVideoStream (channel->getVideoStream_unlocked());
            channel->getEventInformer()->subscribe_unlocked (
                    CbDesc<Channel::ChannelEvents> (&channel_events, channel_entry, channel_entry));
            channel->channelUnlock ();

            channel_entry->media_recorder.startRecording ();
        }
    }
}

void
ChannelRecorder::doDestroyChannel (ChannelEntry * const mt_nonnull channel_entry)
{
    logD_ (_func, "channel: ", channel_entry->channel_name);
}

mt_const void
ChannelRecorder::init (MomentServer * const mt_nonnull moment,
                       Vfs          * const mt_nonnull vfs,
                       NamingScheme * const mt_nonnull naming_scheme)
{
    logD_ (_func_);

    this->moment = moment;
    this->vfs = vfs;
    this->naming_scheme = naming_scheme;

    Ref<ChannelManager> const channel_manager = moment->getChannelManager();

    {
        channel_manager->channelManagerLock ();

        ChannelManager::ChannelInfo channel_info;
        ChannelManager::channel_iterator iter (channel_manager);
        while (!iter.done()) {
            logD_ (_func, "iteration");
            iter.next (&channel_info);
            logD_ (_func, "channel_name: ", channel_info.channel_name);
            doCreateChannel (&channel_info);
        }

        channel_manager->getEventInformer()->subscribe_unlocked (
                CbDesc<ChannelManager::Events> (&channel_manager_events, this, this));

        channel_manager->channelManagerUnlock ();
    }
}

ChannelRecorder::ChannelRecorder ()
{
}

}

