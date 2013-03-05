#include <moment-nvr/nvr_file_iterator.h>


using namespace M;
using namespace Moment;

namespace MomentNvr {

StRef<String>
NvrFileIterator::makePathForDepth (unsigned const depth) const
{
    StRef<String> str;

    Format fmt;
    fmt.min_digits = 2;

    switch (depth) {
        case 5: str = st_makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2],"/",pos[3],"/",pos[4]); break;
        case 4: str = st_makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2],"/",pos[3]); break;
        case 3: str = st_makeString (fmt, stream_name,"/",pos[0],"/",pos[1],"/",pos[2]); break;
        case 2: str = st_makeString (fmt, stream_name,"/",pos[0],"/",pos[1]); break;
        case 1: str = st_makeString (fmt, stream_name,"/",pos[0]); break;
        default:
            unreachable ();
    }
    return str;
}

StRef<String>
NvrFileIterator::getNext_rec (Vfs::VfsDirectory * const mt_nonnull parent_dir,
                              ConstMemory         const parent_dir_name,
                              unsigned            const depth)
{
    unsigned const target = pos [depth];

    logD_ (_func, "depth: ", depth, ", parent_dir_name: ", parent_dir_name, ", target: ", target);

    bool got_best_number = false;
    bool best_is_vdat = false;
    unsigned best_number = 0;
    Ref<String> best_entry_name;

    if (depth < 5) {
        StRef<String> const dir_name = st_makeString (parent_dir_name, "/", target);
        Ref<Vfs::VfsDirectory> const dir = vfs->openDirectory (dir_name->mem());
        if (dir) {
            logD_ (_func, "descending into ", dir_name);
            if (StRef<String> const str = getNext_rec (dir, fir_name->mem(), depth + 1)) {
                logD_ (_func, "result: ", str);
                return str;
            }
        }
    }

    // TODO 1. get all entries and sort them alphabetically.
    //      2. walk through the entries in a loop, descending into dirs recursively.

    AvlTree<unsigned> subdir_tree;
    AvlTree<unsigned> vdat_tree;
    {
        Ref<String> entry_name;
        while (dir->getNextEntry (entry_name) && entry_name) {
            bool is_vdat = false;
            ConstMemory number_mem;
            if (stringHasSuffix (entry_name->mem(), ".vdat", &number_mem)) {
                is_vdat = true;
            } else {
                number_mem = entry_name->mem();
            }

            logD_ (_func, "is_vdat: ", is_vdar, ", number_mem: ", number_mem);

            Uint32 number = 0;
            if (strToUint32_safe (number_mem, &number, 10 /* base */)) {
                if (is_vdat)
                    vdat_tree.add (number);
                else
                    subdir_tree.add (number);
            }
        }
    }

    Format fmt;
    fmt.min_digits = 2;

    if (vdat_tree.isEmpty() && depth < 5) {
        logD_ (_func, "walking subdir_tree");

        AvlTree<unsigned>::bl_iterator iter (subdir_tree);
        while (!iter.done()) {
            unsigned const number = iter.next ();
            if (number < target)
                continue;

            StRef<String> const dir_name = st_makeString (parent_dir_name, "/", fmt, number);
            Ref<Vfs::VfsDirectory> const dir = vfs->openDirectory (dir_name->mem());
            if (dir) {
                if (StRef<String> const str = getNext_rec (dir, dir_name, depth + 1))
                    return str;
            }
        }
    } else {
        logD_ (_func, "walking vdat_tree");

        AvlTree<unsigned>::bl_iterator iter (vdat_tree);
        while (!iter.done()) {
            unsigned const number = iter.next ();
            if (number < target || (got_first && number == target))
                continue;

            got_first = true;

            StRef<String> const filename = st_makeString (parent_dir_name, "/", fmt, number);
            return filename;
        }
    }

    return NULL;

#if 0
    // TODO Use StRef
    Ref<String> entry_name;
    while (dir->getNextEntry (entry_name) && entry_name) {
        bool is_vdat = false;
        ConstMemory number_mem;
        if (stringHasSuffix (entry_name->mem(), ".vdat", &number_mem)) {
            is_vdat = true;
        } else {
            number_mem = entry_name->mem();
        }

        Uint32 number = 0;
        if (strToUint32_safe (number_mem, &number, 10 /* base */)) {
            bool best = false;
            if (got_first) {
                if (number > target && (!got_best_number || number < best_number))
                    best = true;
            } else {
                if (number <= target && (!got_best_number || number > best_number))
                    best = true;
            }

            if (best) {
                got_best_number = true;
                best_is_vdat = is_vdat;
                best_number = number;
                best_entry_name = grab (new (std::nothrow) String (number_mem)); //entry_name;
            }
        }
    }

    if (!got_best_number)
        return NULL;

    // TODO update pos properly
    pos [depth] = best_number;

    getNext_ref ();
#endif
}

StRef<String>
NvrFileIterator::getNext ()
{
#warning Iteration logics is wrong. Write a suitable algorithm first.

  // Алгоритм итерирования по файлам в архиве.
  //
  // 1. Берём разбивку pos для start_unixtime. Далее эта разбивка является
  //    опорной точкой при поиске очередного файла.
  // 2. Открываем ближайший каталог-кандидат. Если на каком-то уровне
  //    произошло несовпадение с pos, то значения pos на более низких уровнях
  //    обнуляем.
  // 3. Обходим файлы в открытом каталоге. По окончании переходим на уровень выше.
  //    (т е существует граница применимости значений pos, которая сдвигается
  //    только вверх).
  // 4. Это обход по дереву => рекурсия. Начало поиска - pos. Рекурсия несложная (!).

    StRef<String> dir_path;
    Ref<Vfs::VfsDirectory> dir;
    unsigned depth = 5;
    for (; depth > 0; --depth) {
        dir_path = makePathForDepth (depth);
        logD_ (_func, "dir_path: ", dir_path);
        dir = vfs->openDirectory (dir_path->mem());
        if (dir)
            break;
    }

    if (!dir) {
        logD_ (_func, "!dir");
        return NULL;
    }

    unsigned const target = pos [depth];

    bool got_best_number = false;
    unsigned best_number = 0;
    Ref<String> best_entry_name;

    // TODO Use StRef
    Ref<String> entry_name;
    while (dir->getNextEntry (entry_name) && entry_name) {
//            logD_ (_func, "entry_name: 0x", fmt_hex, (UintPtr) entry_name.ptr());
//            logD_ (_func, "entry_name: ", entry_name);
        ConstMemory number_mem;
        if (stringHasSuffix (entry_name->mem(), ".vdat", &number_mem)) {
            Uint32 number = 0;
            if (strToUint32_safe (number_mem, &number, 10 /* base */)) {
                bool best = false;
                if (got_first) {
                    if (number > target && (!got_best_number || number < best_number))
                        best = true;
                } else {
                    if (number <= target && (!got_best_number || number > best_number))
                        best = true;
                }

                if (best) {
                    got_best_number = true;
                    best_number = number;
                    best_entry_name = grab (new (std::nothrow) String (number_mem)); //entry_name;
                }
            }
        }
    }

    if (!got_best_number)
        return NULL;

    // TODO update pos properly
    pos [depth] = best_number;

    got_first = true;
    return st_makeString (dir_path->mem(), "/", best_entry_name->mem());
}

void
NvrFileIterator::init (Vfs         * const mt_nonnull vfs,
                       ConstMemory   const stream_name,
                       Time          const start_unixtime_sec)
{
    this->vfs = vfs;
    this->stream_name = st_grab (new (std::nothrow) String (stream_name));

    struct tm tm;
    if (!unixtimeToStructTm (start_unixtime_sec, &tm)) {
        logE_ (_func, "unixtimeToStructTm() failed");
        memset (pos, 0, sizeof (pos));
        return;
    }

    pos [0] = tm.tm_year + 1900;
    pos [1] = tm.tm_mon + 1;
    pos [2] = tm.tm_mday;
    pos [3] = tm.tm_hour;
    pos [4] = tm.tm_min;
    pos [5] = tm.tm_sec;
}

}

