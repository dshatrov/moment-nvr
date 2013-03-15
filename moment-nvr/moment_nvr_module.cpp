#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

void
MomentNvrModule::doGetFile (HttpRequest * const mt_nonnull req,
                            Sender      * const mt_nonnull sender,
                            ConstMemory   const stream_name,
                            Time          const start_unixtime_sec,
                            Time          const duration_sec)
{
    logD_ (_func, "stream: ", stream_name, ", "
           "start: ", start_unixtime_sec, ", "
           "duration: ", duration_sec);

#warning TODO

    MOMENT_SERVER__HEADERS_DATE

    sender->send (page_pool,
                  true /* do_flush */,
                  // TODO No cache
                  MOMENT_SERVER__OK_HEADERS ("text/html", 0),
                  "\r\n");

    logA_ ("mod_nvr 200 ", req->getClientAddress(), " ", req->getRequestLine());

    if (!req->getKeepalive())
        sender->closeAfterFlush ();
}

HttpService::HttpHandler const MomentNvrModule::http_handler =
{
    httpRequest,
    NULL /* httpMessageBody */
};

Result
MomentNvrModule::httpRequest (HttpRequest  * const mt_nonnull req,
                              Sender       * const mt_nonnull conn_sender,
                              Memory const & /* msg_body */,
                              void        ** const mt_nonnull /* ret_msg_data */,
                              void         * const _self)
{
    MomentNvrModule * const self = static_cast <MomentNvrModule*> (_self);

    logD_ (_func_);

    MOMENT_SERVER__HEADERS_DATE

    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "unixtime"))
    {
        StRef<String> const unixtime_str = st_makeString (getUnixtime());
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           // TODO No cache
                           MOMENT_SERVER__OK_HEADERS ("text/html", unixtime_str->len()),
                           "\r\n",
                           unixtime_str);

        logA_ ("mod_nvr 200 ", req->getClientAddress(), " ", req->getRequestLine());
    } else
    if (req->getNumPathElems() >= 2
        && equal (req->getPath (1), "file"))
    {
        ConstMemory const stream_name = req->getParameter ("stream");

        Uint64 start_unixtime_sec = 0;
        if (!strToUint64_safe (req->getParameter ("start"), &start_unixtime_sec, 10 /* base */)) {
            logE_ (_func, "Bad \"start\" request parameter value");
            goto _bad_request;
        }

        Uint64 duration_sec = 0;
        if (!strToUint64_safe (req->getParameter ("duration"), &duration_sec, 10 /* base */)) {
            logE_ (_func, "Bad \"duration\" request parameter value");
            goto _bad_request;
        }

        self->doGetFile (req, conn_sender, stream_name, start_unixtime_sec, duration_sec);
        return Result::Success;
    } else {
        logE_ (_func, "Unknown request: ", req->getFullPath());

        ConstMemory const reply_body = "404 Not Found (mod_nvr)";
        conn_sender->send (self->page_pool,
                           true /* do_flush */,
                           MOMENT_SERVER__404_HEADERS (reply_body.len()),
                           "\r\n",
                           reply_body);

        logA_ ("mod_nvr 404 ", req->getClientAddress(), " ", req->getRequestLine());
    }

    goto _return;

_bad_request:
    {
	MOMENT_SERVER__HEADERS_DATE
	ConstMemory const reply_body = "400 Bad Request (mod_nvr)";
	conn_sender->send (
		self->page_pool,
		true /* do_flush */,
		MOMENT_SERVER__400_HEADERS (reply_body.len()),
		"\r\n",
		reply_body);

	logA_ ("lectorium 400 ", req->getClientAddress(), " ", req->getRequestLine());
    }

_return:
    if (!req->getKeepalive())
        conn_sender->closeAfterFlush ();

    return Result::Success;
}

void
MomentNvrModule::init (MomentServer * const mt_nonnull moment)
{
    page_pool = moment->getPagePool();

    Ref<NamingScheme> const naming_scheme =
            grab (new (std::nothrow) DefaultNamingScheme (5 /* file_duration_sec */));
    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs ("/home/erdizz/record" /* TODO Config parameter */);

    channel_recorder = grab (new (std::nothrow) ChannelRecorder);
    channel_recorder->init (moment, vfs, naming_scheme);

    media_viewer = grab (new (std::nothrow) MediaViewer);
    media_viewer->init (moment, vfs);

    moment->getHttpService()->addHttpHandler (
            CbDesc<HttpService::HttpHandler> (&http_handler, this, this),
            "mod_nvr",
            true /* preassembly */,
            1 << 20 /* 1 Mb */ /* preassembly_limit */,
            true /* parse_body_params */);
}

MomentNvrModule::MomentNvrModule ()
    : page_pool (this /* coderef_container */)
{
}

}

