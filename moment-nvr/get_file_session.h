#ifndef MOMENT_NVR__GET_FILE_SESSION__H__
#define MOMENT_NVR__GET_FILE_SESSION__H__


#include <moment/libmoment.h>


namespace MomentNvr {

using namespace M;
using namespace Moment;

class GetFileSession : public Object
{
private:
    StateMutex mutex;

public:
    struct Frontend {
        void (*done) (Result  res,
                      void   *cb_data);
    };

private:
    mt_const DataDepRef<Sender> sender;
    mt_const Cb<Frontend> frontend;

    Mp4Muxer mp4_muxer;

  mt_iface (Sender::Frontend)
    static Sender::Frontend const sender_frontend;

    static void senderStateChanged (Sender::SendState  send_state,
                                    void              *_self);

    static void senderClosed (Exception *exc,
                              void      *_self);
  mt_iface_end 

public:
    mt_const void init (HttpRequest * const mt_nonnull req,
                        Sender      * const mt_nonnull sender,
                        ConstMemory   const stream_name,
                        Time          const start_unixtime_sec,
                        Time          const duration_sec,
                        CbDesc<Frontend> const &frontend);

    GetFileSession ()
        : sender    (this /* coderef_container */),
          mp4_muxer (this /* coderef_container */)
    {}
};

}


#endif /* MOMENT_NVR__GET_FILE_SESSION__H__ */

