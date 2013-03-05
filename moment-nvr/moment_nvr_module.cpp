#include <moment-nvr/moment_nvr_module.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

void
MomentNvrModule::init (MomentServer * const mt_nonnull moment)
{
    Ref<NamingScheme> const naming_scheme =
            grab (new (std::nothrow) DefaultNamingScheme (5 /* file_duration_sec */));
    Ref<Vfs> const vfs = Vfs::createDefaultLocalVfs ("/home/erdizz/record" /* TODO Config parameter */);

    channel_recorder = grab (new (std::nothrow) ChannelRecorder);
    channel_recorder->init (moment, vfs, naming_scheme);

    media_viewer = grab (new (std::nothrow) MediaViewer);
    media_viewer->init (moment, vfs, naming_scheme);
}

}

