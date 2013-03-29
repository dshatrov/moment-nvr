#include <moment-nvr/get_file_session.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

Sender::Frontend const GetFileSession::sender_frontend = {
    senderStateChanged,
    senderClosed
};

void
GetFileSession::senderStateChanged (Sender::SendState   const send_state,
                                    void              * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
}

void
GetFileSession::senderClosed (Exception * const exc,
                              void      * const _self)
{
    GetFileSession * const self = static_cast <GetFileSession*> (_self);
}

mt_const void
GetFileSession::init (HttpRequest * const mt_nonnull req,
                      Sender      * const mt_nonnull sender,
                      ConstMemory   const stream_name,
                      Time          const start_unixtime_sec,
                      Time          const duration_sec,
                      CbDesc<Frontend> const &frontend)
{
    this->frontend = frontend;

    sender->getEventInformer()->subscribe (
            CbDesc<Sender::Frontend> (&sender_frontend, this, this));
}

}

