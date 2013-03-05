#include <libmary/module_init.h>

#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;
using namespace MomentNvr;


static MomentNvrModule *moment_nvr;

static void momentNvrInit ()
{
    logD_ (_func_);

    moment_nvr = new (std::nothrow) MomentNvrModule;
    assert (moment_nvr);
    moment_nvr->init (MomentServer::getInstance());
}

static void momentNvrUnload ()
{
    logD_ (_func_);

    moment_nvr->unref ();
}


namespace M {

void libMary_moduleInit ()
{
    momentNvrInit ();
}

void libMary_moduleUnload ()
{
    momentNvrUnload ();
}

}

