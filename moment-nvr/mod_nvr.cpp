#include <libmary/module_init.h>
#include <moment/libmoment.h>


using namespace M;
using namespace Moment;


namespace {
class MomentNvrModule : public Object
{
private:
};
}

static MomentNvrModule *moment_nvr;


static void momentNvrInit ()
{
    logD_ (_func_);

    moment_nvr = new MomentNvrModule;
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

