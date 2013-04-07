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
//    logD_ (_func_);
    if (stream_changed) {
        channel_entry->media_recorder->setVideoStream (stream);
        channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelStopItem (VideoStream * const stream,
                                  bool          const stream_changed,
                                  void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
//    logD_ (_func_);
    if (stream_changed) {
        channel_entry->media_recorder->setVideoStream (stream);
        channel_entry->media_recorder->startRecording ();
    }
}

void
ChannelRecorder::channelNewVideoStream (VideoStream * const stream,
                                        void        * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
//    logD_ (_func_);
    channel_entry->media_recorder->setVideoStream (stream);
    channel_entry->media_recorder->startRecording ();
}

void
ChannelRecorder::channelDestroyed (void * const _channel_entry)
{
    ChannelEntry * const channel_entry = static_cast <ChannelEntry*> (_channel_entry);
    Ref<ChannelRecorder> const self = channel_entry->weak_channel_recorder.getRef ();
    if (!self)
        return;

    self->mutex.lock ();
    self->doDestroyChannel (channel_entry);
    self->mutex.unlock ();
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
    channel_entry->valid = true;
    channel_entry->weak_channel_recorder = this;
    // TODO release thread_ctx
    channel_entry->thread_ctx = moment->getRecorderThreadPool()->grabThreadContext (ConstMemory() /* TODO filename */);
    if (channel_entry->thread_ctx) {
        channel_entry->recorder_thread_ctx = channel_entry->thread_ctx;
    } else {
        logE_ (_func, "Couldn't get recorder thread context: ", exc->toString());
        channel_entry->recorder_thread_ctx = NULL;
        channel_entry->thread_ctx = moment->getServerApp()->getServerContext()->getMainThreadContext();
    }

    channel_entry->channel = channel;
    channel_entry->channel_name = st_grab (new (std::nothrow) String (channel_info->channel_name));

    channel_entry->media_recorder = grab (new (std::nothrow) MediaRecorder);
    channel_entry->media_recorder->init (moment->getPagePool (),
                                         channel_entry->thread_ctx,
                                         vfs,
                                         naming_scheme,
                                         channel_info->channel_name);

    channel_entry->nvr_cleaner = grab (new (std::nothrow) NvrCleaner);
    channel_entry->nvr_cleaner->init (moment->getServerApp()->getServerContext()->getMainThreadContext()->getTimers(),
                                      vfs,
                                      channel_info->channel_name,
//                                      3600 /* max_age_sec */);
                                      3600 /* max_age_sec */);

    mutex.lock ();
#warning TODO Deal with duplicate channel names.

    channel_entry->hash_entry_key = channel_hash.add (channel_info->channel_name, channel_entry);
    mutex.unlock ();

    {
        channel->channelLock ();
        if (channel->isDestroyed_unlocked()) {
            channel->channelUnlock ();

            mutex.lock ();
            doDestroyChannel (channel_entry);
            mutex.unlock ();
        } else {
            channel_entry->media_recorder->setVideoStream (channel->getVideoStream_unlocked());
            channel->getEventInformer()->subscribe_unlocked (
                    CbDesc<Channel::ChannelEvents> (&channel_events, channel_entry, channel_entry));
            channel->channelUnlock ();

            channel_entry->media_recorder->startRecording ();
        }
    }
}

mt_mutex (mutex) void
ChannelRecorder::doDestroyChannel (ChannelEntry * const mt_nonnull channel_entry)
{
    logD_ (_func, "channel: ", channel_entry->channel_name);

    if (!channel_entry->valid) {
        return;
    }
    channel_entry->valid = false;

    if (channel_entry->recorder_thread_ctx) {
        moment->getRecorderThreadPool()->releaseThreadContext (channel_entry->recorder_thread_ctx);
        channel_entry->recorder_thread_ctx = NULL;
    }

    channel_hash.remove (channel_entry->hash_entry_key);
    // 'channel_entry' is not valid anymore.
}

ChannelRecorder::ChannelResult
ChannelRecorder::getChannelState (ConstMemory    const channel_name,
                                  ChannelState * const mt_nonnull ret_state)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    ret_state->recording = channel_entry->media_recorder->isRecording();

    mutex.unlock ();
    return ChannelResult_Success;
}

ChannelRecorder::ChannelResult
ChannelRecorder::setRecording (ConstMemory const channel_name,
                               bool        const set_on)
{
    mutex.lock ();
    ChannelHash::EntryKey const channel_entry_key = channel_hash.lookup (channel_name);
    if (!channel_entry_key) {
        mutex.unlock ();
        return ChannelResult_ChannelNotFound;
    }
    Ref<ChannelEntry> const &channel_entry = channel_entry_key.getData();

    if (set_on)
        channel_entry->media_recorder->startRecording ();
    else
        channel_entry->media_recorder->stopRecording ();

    mutex.unlock ();
    return ChannelResult_Success;
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

ChannelRecorder::~ChannelRecorder ()
{
    mutex.lock ();

    ChannelHash::iterator iter (channel_hash);
    while (!iter.done()) {
        Ref<ChannelEntry> * const channel_entry = iter.next ();
        doDestroyChannel (*channel_entry);
    }
    assert (channel_hash.isEmpty());

    mutex.unlock ();
}

}

