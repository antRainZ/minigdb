#include "elf/elf.hpp"
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

namespace elf
{
/**
 * @brief 通过mmap将一个可执行文件映射到内存中
 *
 */
class mmap_loader : public loader
{
    void *base;
    size_t lim;

  public:
    mmap_loader(int fd)
    {
        // 函数获取文件的长度
        off_t end = lseek(fd, 0, SEEK_END);
        if (end == (off_t)-1)
            throw system_error(errno, system_category(),
                               "finding file length");
        lim = end;

        base = mmap(nullptr, lim, PROT_READ, MAP_SHARED, fd, 0);
        if (base == MAP_FAILED)
            throw system_error(errno, system_category(),
                               "mmap'ing file");
        close(fd);
    }

    ~mmap_loader()
    {
        munmap(base, lim);
    }
    /**
     * @brief 加载文件中的一部分数据
     * 
     * @param offset 表示要加载的数据在文件中的偏移量
     * @param size 表示要加载的数据的字节数
     * @return const void* 
     */
    const void *load(off_t offset, size_t size)
    {
        if (offset + size > lim)
            throw range_error("offset exceeds file size");
        return (const char *)base + offset;
    }
};

/**
 * @brief 创建可执行文件的加载器
 * 
 * @param fd 读取可执行文件返回的fd 
 * @return std::shared_ptr<loader> 
 */
std::shared_ptr<loader>
create_mmap_loader(int fd)
{
    return make_shared<mmap_loader>(fd);
}

} // namespace elf