#include <libmary/module_init.h>

#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;
using namespace MomentNvr;


static MomentNvrModule *moment_nvr;

static void momentNvrInit ()
{
    logD_ (_func_);

    Ref<MomentServer> const moment = MomentServer::getInstance();
    Ref<MConfig::Config> const config = moment->getConfig ();

    {
        ConstMemory const opt_name = "mod_nvr/enable";
        MConfig::BooleanValue const enable = config->getBoolean (opt_name);
        if (enable == MConfig::Boolean_Invalid) {
            logE_ (_func, "Invalid value for ", opt_name, ": ", config->getString (opt_name));
            return;
        }

        if (enable != MConfig::Boolean_True) {
            logI_ (_func, "NVR module is not enabled. "
                          "Set \"", opt_name, "\" option to \"y\" to enable.");
            return;
        }
    }

    moment_nvr = new (std::nothrow) MomentNvrModule;
    assert (moment_nvr);
    moment_nvr->init (moment);
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

